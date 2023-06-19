#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "bq_design.h"
#include "biquad5_process.h"

//==============================================================================
Biquad5GPUAudioProcessor::Biquad5GPUAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{

    deviceIdx = 0;

    measurementFlag = false;
    firstPullFlag = false;

    platformContext = NULL;

    inData = NULL;
    outData = NULL;

    inClBuffer = NULL;
    outClBuffer = NULL;
    delayClBuffer = NULL;
    coeffsAClBuffer = NULL;
    coeffsBClBuffer = NULL;

    for (int i = 0; i < NUM_OF_DEVICES; i++)
    {
        device_id[i] = NULL;
        platform_id[i] = NULL;
        devNames[i] = NULL;
    }

    biquad5Kernel = NULL;
    biquad_params[0] = {  500.0f, 0.0f, 0.5f, BQ_LOSHELF_2ND};
    biquad_params[1] = { 1000.0f, 0.0f, 0.5f, BQ_PEAK };
    biquad_params[2] = { 2000.0f, 0.0f, 0.5f, BQ_PEAK };
    biquad_params[3] = { 5000.0f, 0.0f, 0.5f, BQ_PEAK };
    biquad_params[4] = { 8000.0f, 0.0f, 0.5f, BQ_HISHELF_2ND };
}

Biquad5GPUAudioProcessor::~Biquad5GPUAudioProcessor()
{
    cl_int ret = 0;

    if (inData != NULL)
    {
        delete[] inData;
    }

    if (outData != NULL)
    {
        delete[] outData;
    }

    if (inClBuffer != NULL)
    {
        ret = clReleaseMemObject(inClBuffer);
    }

    if (outClBuffer != NULL)
    {
        ret = clReleaseMemObject(outClBuffer);
    }

    if (delayClBuffer != NULL)
    {
        ret = clReleaseMemObject(delayClBuffer);
    }

    if (coeffsAClBuffer != NULL)
    {
        ret = clReleaseMemObject(coeffsAClBuffer);
    }

    if (coeffsBClBuffer != NULL)
    {
        ret = clReleaseMemObject(coeffsBClBuffer);
    }

    for (int i = 0; i < NUM_OF_DEVICES; i++)
    {
        if (devNames[i] != NULL)
        {
            delete[] devNames[i];
        }
    }

    if (biquad5Kernel != NULL)
    {
        ret = clReleaseKernel(biquad5Kernel);
    }

    if (platformContext != NULL)
	{
        clReleaseContext(platformContext);
	}
}

//==============================================================================
const juce::String Biquad5GPUAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Biquad5GPUAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Biquad5GPUAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Biquad5GPUAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double Biquad5GPUAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Biquad5GPUAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int Biquad5GPUAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Biquad5GPUAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused(index);
}

const juce::String Biquad5GPUAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused(index);
    return {};
}

void Biquad5GPUAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused(index);
    juce::ignoreUnused(newName);
}

//==============================================================================
void Biquad5GPUAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
        if (inData != NULL)
        {
            delete[] inData;
        }
        inData = new float[STEREO_CHANNELS*samplesPerBlock];

        if (outData != NULL)
        {
            delete[] outData;
        }
        outData = new float[STEREO_CHANNELS*samplesPerBlock];

    for (int i = 0; i < NUM_OF_DEVICES; i++)
    {
        if (devNames[i] != NULL)
        {
            delete[] devNames[i];
        }
        devNames[i] = new char[NAME_LENGTH];
        memset(devNames[i], 0, sizeof(char) * NAME_LENGTH);
    }

    getOpenCLDevices();

    bufferSize = samplesPerBlock;
    fs = (int) sampleRate;

    createPlatformContext();
}

void Biquad5GPUAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Biquad5GPUAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void Biquad5GPUAudioProcessor::updateParams(float gains[5])
{
    for (int bq = 0; bq < 5; bq++)
    {
        biquad_params[bq].gain = gains[bq];
    }
    designBiquads();
    doCoeffsUpdate = true;
}

void Biquad5GPUAudioProcessor::designBiquads()
{
    for (int bq = 0; bq < 5; bq++)
    {
        bqdesignf(&biquad_coeffsB[0][bq][0], &biquad_coeffsA[0][bq][0], float(fs), biquad_params[bq].f0, biquad_params[bq].gain, biquad_params[bq].Q, biquad_params[bq].type);
        bqdesignf(&biquad_coeffsB[1][bq][0], &biquad_coeffsA[1][bq][0], float(fs), biquad_params[bq].f0, biquad_params[bq].gain, biquad_params[bq].Q, biquad_params[bq].type);
    }
}

void Biquad5GPUAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();

    cl_int ret = 0;

    LARGE_INTEGER StartingTime, EndingTimeTotal, EndingTimeIn, EndingTimeBiquad, EndingTimeOut;
    LARGE_INTEGER Frequency;

    QueryPerformanceFrequency(&Frequency);
    QueryPerformanceCounter(&StartingTime);

    if (deviceIsOpenCL())
    {
		// prepare input data in flat layout
		for (int channel = 0; channel < totalNumInputChannels; ++channel)
		{
			auto* channelData = buffer.getWritePointer(channel);
			memcpy(&inData[channel*bufferSize], channelData, sizeof(float) * bufferSize);
		}

		if (firstPullFlag)
        {
			// collect previous output data to VST buffer
			for (int channel = 0; channel < totalNumInputChannels; ++channel)
			{
				auto* channelData = buffer.getWritePointer(channel);
				memcpy(channelData, &outData[channel*bufferSize], sizeof(float) * bufferSize);
			}
		}

		/* Initial part in non-blocking mode - transfering input buffers, etc */
		ret = clEnqueueWriteBuffer(getOpenCLActiveQueue(), inClBuffer, CL_FALSE, 0,
            totalNumInputChannels*bufferSize * sizeof(float), inData, 0, NULL, NULL);

        if (doCoeffsUpdate)
        {
            ret = clEnqueueWriteBuffer(getOpenCLActiveQueue(), coeffsAClBuffer, CL_FALSE, 0,
                sizeof(biquad_coeffsA), biquad_coeffsA, 0, NULL, NULL);
            ret = clEnqueueWriteBuffer(getOpenCLActiveQueue(), coeffsBClBuffer, CL_FALSE, 0,
                sizeof(biquad_coeffsB), biquad_coeffsB, 0, NULL, NULL);
            doCoeffsUpdate = false;
        }

		// schedule biquad computation on GPU
		size_t global_item_size = totalNumInputChannels; // parallel channels processing
        ret = clEnqueueNDRangeKernel(getOpenCLActiveQueue(), biquad5Kernel, 1, NULL,
            &global_item_size, NULL, 0, NULL, NULL);

		QueryPerformanceCounter(&EndingTimeIn);

		if (!firstPullFlag)
		{
			ret = clFinish(getOpenCLActiveQueue()); // wait for kernel to finish for time measurement
		}
		QueryPerformanceCounter(&EndingTimeBiquad);

		if (firstPullFlag)
        {
			/* Final part - schedule transfering output buffers in non-blocking mode */
			ret = clEnqueueReadBuffer(getOpenCLActiveQueue(), outClBuffer, CL_FALSE, 0,
				totalNumInputChannels*bufferSize * sizeof(float), outData, 0, NULL, NULL);
		}
		else
		{
			/* Final part - transfering output buffers in blocking mode */
			ret = clEnqueueReadBuffer(getOpenCLActiveQueue(), outClBuffer, CL_TRUE, 0,
				totalNumInputChannels*bufferSize * sizeof(float), outData, 0, NULL, NULL);

			// collect current output data to VST buffer
			for (int channel = 0; channel < totalNumInputChannels; ++channel)
			{
				auto* channelData = buffer.getWritePointer(channel);
				memcpy(channelData, &outData[channel*bufferSize], sizeof(float) * bufferSize);
			}
		}
    }
    else // processing on CPU 
    {
        // prepare input data in flat layout
		for (int channel = 0; channel < totalNumInputChannels; ++channel)
		{
			auto* channelData = buffer.getWritePointer(channel);
			memcpy(&inData[channel*bufferSize], channelData, sizeof(float) * bufferSize);
		}

		QueryPerformanceCounter(&EndingTimeIn);
        biquad5_process(inData, outData, &biquad_delays[0][0][0], &biquad_coeffsA[0][0][0], &biquad_coeffsB[0][0][0], 2, bufferSize);
        QueryPerformanceCounter(&EndingTimeBiquad);

		// collect output data to VST buffer
		for (int channel = 0; channel < totalNumInputChannels; ++channel)
		{
			auto* channelData = buffer.getWritePointer(channel);
			memcpy(channelData, &outData[channel*bufferSize], sizeof(float) * bufferSize);
		}
	}


    QueryPerformanceCounter(&EndingTimeOut);

    QueryPerformanceCounter(&EndingTimeTotal);

    processTimeTotal.addValue((EndingTimeTotal.QuadPart - StartingTime.QuadPart) * 1000.0f / Frequency.QuadPart);
    initialPartTime.addValue((EndingTimeIn.QuadPart - StartingTime.QuadPart) * 1000.0f / Frequency.QuadPart);
    biquadTanTime.addValue((EndingTimeBiquad.QuadPart - EndingTimeIn.QuadPart) * 1000.0f / Frequency.QuadPart);
    finalPartTime.addValue((EndingTimeOut.QuadPart - EndingTimeBiquad.QuadPart) * 1000.0f / Frequency.QuadPart);
}

//==============================================================================
bool Biquad5GPUAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Biquad5GPUAudioProcessor::createEditor()
{
    return new Biquad5GPUAudioProcessorEditor (*this);
}

//==============================================================================
void Biquad5GPUAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void Biquad5GPUAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data);
    juce::ignoreUnused(sizeInBytes);
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Biquad5GPUAudioProcessor();
}

//==============================================================================
void Biquad5GPUAudioProcessor::getOpenCLDevices()
{

    cl_uint i, j;
    int idx = 0;
    size_t valueSize;
    cl_uint platformCount;
    cl_uint deviceCount;

    // get all platforms
    clGetPlatformIDs(0, NULL, &platformCount);
    clGetPlatformIDs(platformCount, platform_id, NULL);

    for (i = 0; i < platformCount; i++)
    {
        // get all devices
        clGetDeviceIDs(platform_id[idx], CL_DEVICE_TYPE_ALL, 0, NULL, &deviceCount);
        clGetDeviceIDs(platform_id[idx], CL_DEVICE_TYPE_ALL, deviceCount, &device_id[idx], NULL);

        for (j = 0; j < deviceCount; j++)
        {
            clGetDeviceInfo(device_id[idx], CL_DEVICE_NAME, 0, NULL, &valueSize);
            clGetDeviceInfo(device_id[idx], CL_DEVICE_NAME, valueSize, devNames[idx], NULL);

            idx++;
        }
    }
    numOpenCLDevices = idx;

	// add CPU handled without openCL (AMD does no support CPU openCL anymore)
	strcpy(devNames[idx], "CPU-openMP");
	device_id[idx] = NULL;
	platform_id[idx] = NULL;
	idx++;
}

cl_command_queue Biquad5GPUAudioProcessor::getOpenCLActiveQueue()
{
    return reservedCuFlag ? RTqueue : GPqueue;
}

void Biquad5GPUAudioProcessor::createPlatformContext()
{
    if (deviceIsOpenCL())
    {
        cl_int error = CL_DEVICE_NOT_FOUND;
        cl_context_properties contextProps[3] =
        {
            CL_CONTEXT_PLATFORM,
            (cl_context_properties)platform_id[deviceIdx],
            0
        };

        if (platformContext != NULL)
        {
            clReleaseContext(platformContext);
        }

        platformContext = clCreateContext(contextProps, 1, &device_id[deviceIdx], NULL, NULL, &error);
        if (GPqueue != nullptr)
        {
            clReleaseCommandQueue(GPqueue);
            GPqueue = nullptr;
        }
        if (RTqueue != nullptr)
        {
            clReleaseCommandQueue(RTqueue);
            RTqueue = nullptr;
        }
        CreateQueuesGPU(platformContext, GPqueue, RTqueue);

        cl_int ret;

        if (biquad5Kernel != NULL)
        {
            ret = clReleaseKernel(biquad5Kernel);
        }

        if (inClBuffer != NULL)
        {
            ret = clReleaseMemObject(inClBuffer);
        }
        inClBuffer = clCreateBuffer(platformContext, CL_MEM_READ_WRITE,
            STEREO_CHANNELS*bufferSize * sizeof(float), NULL, &ret);

        if (outClBuffer != NULL)
        {
            ret = clReleaseMemObject(outClBuffer);
        }
        outClBuffer = clCreateBuffer(platformContext, CL_MEM_READ_WRITE,
            STEREO_CHANNELS*bufferSize * sizeof(float), NULL, &ret);

        if (delayClBuffer != NULL)
        {
            ret = clReleaseMemObject(delayClBuffer);
        }
        delayClBuffer = clCreateBuffer(platformContext, CL_MEM_READ_WRITE,
            sizeof(biquad_delays), NULL, &ret);

        if (coeffsAClBuffer != NULL)
        {
            ret = clReleaseMemObject(coeffsAClBuffer);
        }
        coeffsAClBuffer = clCreateBuffer(platformContext, CL_MEM_READ_WRITE,
            sizeof(biquad_coeffsA), NULL, &ret);

        if (coeffsBClBuffer != NULL)
        {
            ret = clReleaseMemObject(coeffsBClBuffer);
        }
        coeffsBClBuffer = clCreateBuffer(platformContext, CL_MEM_READ_WRITE,
            sizeof(biquad_coeffsB), NULL, &ret);

        const char* source_str = BinaryData::biquad5_kernel_cl;
        size_t source_size = BinaryData::biquad5_kernel_clSize;

        cl_program program = clCreateProgramWithSource(platformContext, 1,
            (const char **)&source_str, (const size_t *)&source_size, &ret);

        ret = clBuildProgram(program, 1, &device_id[deviceIdx], NULL, NULL, NULL);

        biquad5Kernel = clCreateKernel(program, "biquad5", &ret);
        ret = clReleaseProgram(program);
		/* biquad kernel setup */
		ret = clSetKernelArg(biquad5Kernel, 0, sizeof(cl_mem), (void *)&inClBuffer);
		ret = clSetKernelArg(biquad5Kernel, 1, sizeof(cl_mem), (void *)&outClBuffer);
		ret = clSetKernelArg(biquad5Kernel, 2, sizeof(cl_mem), (void *)&delayClBuffer);
		ret = clSetKernelArg(biquad5Kernel, 3, sizeof(cl_mem), (void *)&coeffsAClBuffer);
		ret = clSetKernelArg(biquad5Kernel, 4, sizeof(cl_mem), (void *)&coeffsBClBuffer);
		ret = clSetKernelArg(biquad5Kernel, 5, sizeof(int), (void *)&bufferSize);

        //  transfer initial delays to GPU and request coefficients update
        memset(biquad_delays, 0, sizeof(biquad_delays));
        ret = clEnqueueWriteBuffer(getOpenCLActiveQueue(), delayClBuffer, CL_TRUE, 0,
            sizeof(biquad_delays), biquad_delays, 0, NULL, NULL);
        designBiquads();
        doCoeffsUpdate = true;
    }
    else
    {
        total_compute_units = max_rt_compute_units = reserved_compute_units = 0;
    }
}

void Biquad5GPUAudioProcessor::updatePlatformContext()
{
    suspendProcessing(true);

    createPlatformContext();

    processTimeTotal.reset();
    initialPartTime.reset();
    biquadTanTime.reset();
    finalPartTime.reset();

    suspendProcessing(false);
}

bool Biquad5GPUAudioProcessor::deviceIsOpenCL()
{
	if (deviceIdx >= numOpenCLDevices) // CPU handled without openCL is last on the list
	{
		return false;
	}

    // we only treat GPU devices via OpenCL
	cl_device_type type;
	size_t valueSize;

	clGetDeviceInfo(device_id[deviceIdx], CL_DEVICE_TYPE, 0, NULL, &valueSize);
    clGetDeviceInfo(device_id[deviceIdx], CL_DEVICE_TYPE, valueSize, &type, NULL);

    if (type == CL_DEVICE_TYPE_GPU)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// definitions for accessing driver RTQ features through AMD OpenCL extensions
const uint64_t CL_DEVICE_MAX_REAL_TIME_COMPUTE_UNITS_AMD = 0x404E;
const uint64_t CL_QUEUE_REAL_TIME_COMPUTE_UNITS_AMD = 0x404f;
const uint64_t CL_QUEUE_MEDIUM_PRIORITY = 0x4050;

// use RT_QUEUE define to select between CU_RESERVE and PRIORITY queue
#define RT_QUEUE CL_QUEUE_REAL_TIME_COMPUTE_UNITS_AMD

bool Biquad5GPUAudioProcessor::CreateQueuesGPU(cl_context platform_context, cl_command_queue& gp_queue, cl_command_queue& rt_queue)
{
    cl_int error;
    // use clCreateCommandQueueWithProperties to pass custom queue properties to driver:

    total_compute_units = 0;
    clGetDeviceInfo(device_id[deviceIdx], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(total_compute_units), &total_compute_units, NULL);
    max_rt_compute_units = 0;
    clGetDeviceInfo(device_id[deviceIdx], CL_DEVICE_MAX_REAL_TIME_COMPUTE_UNITS_AMD, sizeof(max_rt_compute_units), &max_rt_compute_units, NULL);
    reserved_compute_units = max_rt_compute_units / 2;

// AMF may mix-up device selection if there is more than one available
//    if((numOpenCLDevices != 1) || AMF_OK != CreateCommandQueuesVIAamf(deviceIdx, RT_QUEUE, reserved_compute_units, &rt_queue, 0, 0, &gp_queue, AMF_CONTEXT_DEVICE_TYPE_GPU))
    {
// deactivated as it does not work with multi-instance
//        if (false == CreateCommandQueuesVIAocl(platform_context, device_id[deviceIdx], RT_QUEUE, reserved_compute_units, &rt_queue, 0, 0, &gp_queue))
        {
            // fallback: create two general purpose queues
			reserved_compute_units = 0;
            rt_queue = clCreateCommandQueue(platform_context, device_id[deviceIdx], NULL, &error);
            gp_queue = clCreateCommandQueue(platform_context, device_id[deviceIdx], NULL, &error);
        }
    }
    clGetCommandQueueInfo(rt_queue, CL_QUEUE_REAL_TIME_COMPUTE_UNITS_AMD, sizeof(reserved_compute_units), &reserved_compute_units, NULL);

    return true;
}

AMF_RESULT Biquad5GPUAudioProcessor::CreateCommandQueuesVIAamf(int deviceIndex, uint64_t type1, int32_t param1, cl_command_queue* pcmdQueue1, uint64_t type2, int32_t param2, cl_command_queue* pcmdQueue2, int amfDeviceType)
{
    bool AllIsOK = true;

    if (pcmdQueue1 == NULL || pcmdQueue2 == NULL) {
        return AMF_INVALID_ARG;
    }

    if (NULL != *pcmdQueue1)
    {
        clReleaseCommandQueue(*pcmdQueue1);
        *pcmdQueue1 = NULL;
    }
    if (NULL != *pcmdQueue2)
    {
        clReleaseCommandQueue(*pcmdQueue2);
        *pcmdQueue2 = NULL;
    }

    AMF_RESULT res = g_AMFFactory.Init();   // initialize AMF
    if (AMF_OK == res)
    {
        // Create default CPU AMF context.
        amf::AMFContextPtr pContextAMF = NULL;
        res = g_AMFFactory.GetFactory()->CreateContext(&pContextAMF);

        pContextAMF->SetProperty(AMF_CONTEXT_DEVICE_TYPE, amfDeviceType);
        if (AMF_OK == res)
        {
            amf::AMFComputeFactoryPtr pOCLFactory = NULL;
            res = pContextAMF->GetOpenCLComputeFactory(&pOCLFactory);
            if (AMF_OK == res)
            {
                amf_int32 device_count = pOCLFactory->GetDeviceCount();
                if (deviceIndex < device_count)
                {
                    amf::AMFComputeDevicePtr pDeviceAMF;
                    res = pOCLFactory->GetDeviceAt(deviceIndex, &pDeviceAMF);
                    if (nullptr != pDeviceAMF)
                    {
                        pContextAMF->InitOpenCLEx(pDeviceAMF);

                        if (NULL != pcmdQueue1)
                        {
                            int ComputeFlag = 0;

                            if (CL_QUEUE_MEDIUM_PRIORITY == type1)
                            {
                                ComputeFlag = 2;
                            }
                            if (CL_QUEUE_REAL_TIME_COMPUTE_UNITS_AMD == type1)
                            {
                                ComputeFlag = 1;
                            }

                            cl_command_queue tempQueue = NULL;
                            pDeviceAMF->SetProperty(L"MaxRealTimeComputeUnits", param1);
                            amf::AMFComputePtr AMFDevice;
                            pDeviceAMF->CreateCompute(&ComputeFlag, &AMFDevice);
                            if (nullptr != AMFDevice)
                            {
                                tempQueue = static_cast<cl_command_queue>(AMFDevice->GetNativeCommandQueue());

                                if (NULL == tempQueue)
                                {
                                    AllIsOK = false;
                                }
                                clRetainCommandQueue(tempQueue);
                                *pcmdQueue1 = tempQueue;
                            }
                        }

                        if (NULL != pcmdQueue2)
                        {
                            int ComputeFlag = 0;

                            if (CL_QUEUE_MEDIUM_PRIORITY == type2)
                            {
                                ComputeFlag = 2;
                            }
                            if (CL_QUEUE_REAL_TIME_COMPUTE_UNITS_AMD == type2)
                            {
                                ComputeFlag = 1;
                            }

                            cl_command_queue tempQueue = NULL;
                            pDeviceAMF->SetProperty(L"MaxRealTimeComputeUnits", param2);
                            amf::AMFComputePtr AMFDevice;
                            pDeviceAMF->CreateCompute(&ComputeFlag, &AMFDevice);
                            if (nullptr != AMFDevice)
                            {
                                tempQueue = static_cast<cl_command_queue>(AMFDevice->GetNativeCommandQueue());

                                if (NULL == tempQueue)
                                {
                                    AllIsOK = false;
                                }
                                clRetainCommandQueue(tempQueue);
                                *pcmdQueue2 = tempQueue;
                            }
                        }
                    }
                }
                else
                {
                    res = AMF_INVALID_ARG;
                }
            }
            pOCLFactory.Release();
        }
        pContextAMF.Release();
        g_AMFFactory.Terminate();

    }
    else
    {
        return AMF_NOT_INITIALIZED;
    }

    if (false == AllIsOK)
    {
        if (NULL != pcmdQueue1)
        {
            if (NULL != *pcmdQueue1)
            {
                clReleaseCommandQueue(*pcmdQueue1);
                *pcmdQueue1 = NULL;
            }
        }
        if (NULL != pcmdQueue2)
        {
            if (NULL != *pcmdQueue2)
            {
                clReleaseCommandQueue(*pcmdQueue2);
                *pcmdQueue2 = NULL;
            }
        }
    }

    return res;
}

bool Biquad5GPUAudioProcessor::CreateCommandQueuesVIAocl(cl_context clContext, cl_device_id clDevice, uint64_t type1, int32_t param1, cl_command_queue* pcmdQueue1, uint64_t type2, int32_t param2, cl_command_queue* pcmdQueue2)
{
    bool AllIsOK = true;

    if (NULL != pcmdQueue1)
    {
        cl_command_queue tempQueue = createQueue(clContext, clDevice, type1, param1);
        if (NULL == tempQueue)
        {
            AllIsOK = false;
        }
        *pcmdQueue1 = tempQueue;
    }

    if (NULL != pcmdQueue2)
    {
        cl_command_queue tempQueue = createQueue(clContext, clDevice, type2, param2);
        if (NULL == tempQueue)
        {
            AllIsOK = false;
        }
        *pcmdQueue2 = tempQueue;
    }

    if (false == AllIsOK)
    {
        if (NULL != pcmdQueue1)
        {
            if (NULL != *pcmdQueue1)
            {
                clReleaseCommandQueue(*pcmdQueue1);
                *pcmdQueue1 = NULL;
            }
        }
        if (NULL != pcmdQueue2)
        {
            if (NULL != *pcmdQueue2)
            {
                clReleaseCommandQueue(*pcmdQueue2);
                *pcmdQueue2 = NULL;
            }
        }
    }

    return AllIsOK;
}

cl_command_queue Biquad5GPUAudioProcessor::createQueue(cl_context context, cl_device_id device, uint64_t type, int32_t param)
{
    cl_int error = 0;
    cl_command_queue cmdQueue = NULL;

    // Create a command queue
#if CL_TARGET_OPENCL_VERSION >= 200
    if (type != 0)
    {
        // use clCreateCommandQueueWithProperties to pass custom queue properties to driver:
        const cl_queue_properties cprops[8] = {
            CL_QUEUE_PROPERTIES,
            0,
            static_cast<cl_queue_properties>(std::uint64_t(type)),
            static_cast<cl_queue_properties>(std::uint64_t(param)),
            static_cast<cl_queue_properties>(std::uint64_t(0))
        };
        // OpenCL 2.0
        cmdQueue = clCreateCommandQueueWithProperties(context, device, cprops, &error);
    }
    else
#endif
    {
        // OpenCL 1.2
        cmdQueue = clCreateCommandQueue(context, device, NULL, &error);
    }

    return cmdQueue;
}
