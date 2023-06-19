#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ReverbGPUAudioProcessor::ReverbGPUAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
{

    dryWet = (juce::Value)50.0f;
    responseLength = 65536;

    deviceIdx = 0;
    impulseResponseIdx = 0;
    impulseResponseFile = juce::MemoryBlock(BinaryData::Church_wav, BinaryData::Church_wavSize);

    measurementFlag = false;
    firstPullFlag = false;

	platformContext = NULL;
    pTANContext = NULL;
    pConvolution = NULL;
	GPqueue = NULL;
	RTqueue = NULL;

    for (int i = 0; i < STEREO_CHANNELS; i++)
    {
        impulseResponses[i] = NULL;
        inData[i] = NULL;
        outData[i] = NULL;

        inClBuffer[i] = NULL;
        outClBuffer[i] = NULL;
    }

    for (int i = 0; i < NUM_OF_DEVICES; i++)
    {
        device_id[i] = NULL;
        platform_id[i] = NULL;
        devNames[i] = NULL;
    }

    dryWetKernel = NULL;
}

ReverbGPUAudioProcessor::~ReverbGPUAudioProcessor()
{
    cl_int ret = 0;

    for (int i = 0; i < STEREO_CHANNELS; i++)
    {
        if (impulseResponses[i] != NULL)
        {
            delete[] impulseResponses[i];
        }

        if (inData[i] != NULL)
        {
            delete[] inData[i];
        }

        if (outData[i] != NULL)
        {
            delete[] outData[i];
        }

        if (inClBuffer[i] != NULL)
        {
            ret = clReleaseMemObject(inClBuffer[i]);
        }

        if (outClBuffer[i] != NULL)
        {
            ret = clReleaseMemObject(outClBuffer[i]);
        }
    }

    for (int i = 0; i < NUM_OF_DEVICES; i++)
    {
        if (devNames[i] != NULL)
        {
            delete[] devNames[i];
        }
    }

    if (dryWetKernel != NULL)
    {
        ret = clReleaseKernel(dryWetKernel);
    }

	if (pTANContext != NULL)
	{
		pTANContext->Terminate();
		pTANContext = NULL;
	}
}

//==============================================================================
const juce::String ReverbGPUAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ReverbGPUAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool ReverbGPUAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool ReverbGPUAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double ReverbGPUAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ReverbGPUAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ReverbGPUAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ReverbGPUAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String ReverbGPUAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void ReverbGPUAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index);
    juce::ignoreUnused(newName);
}

//==============================================================================
void ReverbGPUAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < STEREO_CHANNELS; i++)
    {
        if (impulseResponses[i] != NULL)
        {
            delete[] impulseResponses[i];
        }

        impulseResponses[i] = new float[responseLength];

        if (inData[i] != NULL)
        {
            delete[] inData[i];
        }

        inData[i] = new float[samplesPerBlock];

        if (outData[i] != NULL)
        {
            delete[] outData[i];
        }

        outData[i] = new float[samplesPerBlock];
    }

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

	if (pTANContext != NULL)
	{
		pTANContext->Terminate();
		pTANContext = NULL;
	}
    createTANContext();
    updateIR();
}

void ReverbGPUAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ReverbGPUAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
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

void ReverbGPUAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();

    AMF_RESULT res = AMF_OK;
    cl_int ret = 0;

    LARGE_INTEGER StartingTime, EndingTimeTotal, EndingTimeIn, EndingTimeReverb, EndingTimeMix, EndingTimeOut;
    LARGE_INTEGER Frequency;

    QueryPerformanceFrequency(&Frequency);
    QueryPerformanceCounter(&StartingTime);

	if (deviceIsOpenCL())
	{
		/* Initial part - transfering input buffers, etc */
		for (int channel = 0; channel < totalNumInputChannels; ++channel)
		{
			auto* channelData = buffer.getWritePointer(channel);

			memcpy(inData[channel], channelData, sizeof(float) * bufferSize);

			if (firstPullFlag)
			{
				// take previous output
				memcpy(channelData, outData[channel], sizeof(float) * bufferSize);
			}

			ret = clEnqueueWriteBuffer(getOpenCLActiveQueue(), inClBuffer[channel], CL_FALSE, 0,
				bufferSize * sizeof(float), inData[channel], 0, NULL, NULL);
		}

		QueryPerformanceCounter(&EndingTimeIn);

		/* Convolution reverb processing */
		res = pConvolution->Process(inData, outClBuffer, bufferSize, nullptr, nullptr);

		QueryPerformanceCounter(&EndingTimeReverb);

		/* Dry/wet mix processing */
		if (res == AMF_OK)
		{
			for (int channel = 0; channel < totalNumInputChannels; ++channel)
			{
				float wet = (float)dryWet.getValue() * 0.01f;
				float dry = 1.0f - wet;

				// set kernel arguments
				ret = clSetKernelArg(dryWetKernel, 0, sizeof(float), (void *)&dry);
				ret = clSetKernelArg(dryWetKernel, 1, sizeof(float), (void *)&wet);
				ret = clSetKernelArg(dryWetKernel, 2, sizeof(cl_mem), (void *)&inClBuffer[channel]);
				ret = clSetKernelArg(dryWetKernel, 3, sizeof(cl_mem), (void *)&outClBuffer[channel]);

				size_t global_item_size = bufferSize;
				ret = clEnqueueNDRangeKernel(getOpenCLActiveQueue(), dryWetKernel, 1, NULL,
					&global_item_size, NULL, 0, NULL, NULL);
			}

			if (!firstPullFlag)
			{
				ret = clFinish(getOpenCLActiveQueue());
			}
			QueryPerformanceCounter(&EndingTimeMix);

			/* Final part - transfering output buffers */
			for (int channel = 0; channel < totalNumInputChannels; ++channel)
			{
				auto* channelData = buffer.getWritePointer(channel);

				if (!firstPullFlag)
				{
					// take current output in blocking mode
					ret = clEnqueueReadBuffer(getOpenCLActiveQueue(), outClBuffer[channel], CL_TRUE, 0,
						bufferSize * sizeof(float), outData[channel], 0, NULL, NULL);
					memcpy(channelData, outData[channel], sizeof(float) * bufferSize);
				}
				else
				{
					// take current output in non-blocking mode
					ret = clEnqueueReadBuffer(getOpenCLActiveQueue(), outClBuffer[channel], CL_FALSE, 0,
						bufferSize * sizeof(float), outData[channel], 0, NULL, NULL);
				}
			}

			QueryPerformanceCounter(&EndingTimeOut);
		}
	}
	else // processing on CPU
	{
		/* Initial part - transfering input buffers, etc */
		for (int channel = 0; channel < totalNumInputChannels; ++channel)
		{
			auto* channelData = buffer.getWritePointer(channel);

			memcpy(inData[channel], channelData, sizeof(float) * bufferSize);
		}

		QueryPerformanceCounter(&EndingTimeIn);

		res = pConvolution->Process(inData, outData, bufferSize, nullptr, nullptr);

		QueryPerformanceCounter(&EndingTimeReverb);

		for (int channel = 0; channel < totalNumInputChannels; ++channel)
		{
			auto* channelData = buffer.getWritePointer(channel);

			float wet = (float)dryWet.getValue() * 0.01f;
			float dry = 1.0f - wet;

			for (int i = 0; i < bufferSize; i++)
			{
				channelData[i] = dry * channelData[i] + wet * outData[channel][i];
			}
		}

		QueryPerformanceCounter(&EndingTimeMix);
		QueryPerformanceCounter(&EndingTimeOut);
	}

    QueryPerformanceCounter(&EndingTimeTotal);

    juce::ignoreUnused(EndingTimeMix);
    juce::ignoreUnused(EndingTimeOut);

    processTimeTotal.addValue((EndingTimeTotal.QuadPart - StartingTime.QuadPart) * 1000.0f / Frequency.QuadPart);
    initialPartTime.addValue((EndingTimeIn.QuadPart - StartingTime.QuadPart) * 1000.0f / Frequency.QuadPart);
    reverbTanTime.addValue((EndingTimeReverb.QuadPart - EndingTimeIn.QuadPart) * 1000.0f / Frequency.QuadPart);
    mixKernelTime.addValue((EndingTimeMix.QuadPart - EndingTimeReverb.QuadPart) * 1000.0f / Frequency.QuadPart);
    finalPartTime.addValue((EndingTimeOut.QuadPart - EndingTimeMix.QuadPart) * 1000.0f / Frequency.QuadPart);

}

//==============================================================================
bool ReverbGPUAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ReverbGPUAudioProcessor::createEditor()
{
    return new ReverbGPUAudioProcessorEditor(*this);
}

//==============================================================================
void ReverbGPUAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void ReverbGPUAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new ReverbGPUAudioProcessor();
}

//==============================================================================
void ReverbGPUAudioProcessor::getOpenCLDevices()
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

cl_command_queue ReverbGPUAudioProcessor::getOpenCLActiveQueue()
{
    return reservedCuFlag ? pTANContext->GetOpenCLConvQueue() : pTANContext->GetOpenCLGeneralQueue();
}

void ReverbGPUAudioProcessor::createTANContext()
{
    AMF_RESULT res = TANCreateContext(TAN_FULL_VERSION, &pTANContext);
    amf::TAN_CONVOLUTION_METHOD convMethod = amf::TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD;

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
			platformContext = NULL;
        }

        platformContext = clCreateContext(contextProps, 1, &device_id[deviceIdx], NULL, NULL, &error);
		if (GPqueue != NULL)
		{
			clReleaseCommandQueue(GPqueue);
			GPqueue = NULL;
		}
		if (RTqueue != NULL)
		{
			clReleaseCommandQueue(RTqueue);
			RTqueue = NULL;
		}
		CreateQueuesGPU(platformContext, GPqueue, RTqueue);
        res = pTANContext->InitOpenCL(GPqueue, RTqueue);
        res = TANCreateConvolution(pTANContext, &pConvolution);
        res = pConvolution->InitGpu(convMethod, responseLength, bufferSize, 2);

        cl_int ret;

        if (dryWetKernel != NULL)
        {
            ret = clReleaseKernel(dryWetKernel);
        }

        for (int i = 0; i < STEREO_CHANNELS; i++)
        {
            if (inClBuffer[i] != NULL)
            {
                ret = clReleaseMemObject(inClBuffer[i]);
            }
            inClBuffer[i] = clCreateBuffer(pTANContext->GetOpenCLContext(), CL_MEM_READ_WRITE,
                bufferSize * sizeof(float), NULL, &ret);

            if (outClBuffer[i] != NULL)
            {
                ret = clReleaseMemObject(outClBuffer[i]);
            }
            outClBuffer[i] = clCreateBuffer(pTANContext->GetOpenCLContext(), CL_MEM_READ_WRITE,
                bufferSize * sizeof(float), NULL, &ret);
        }

        const char* source_str = BinaryData::dry_wet_kernel_cl;
        size_t source_size = BinaryData::dry_wet_kernel_clSize;

        cl_program program = clCreateProgramWithSource(pTANContext->GetOpenCLContext(), 1,
            (const char **)&source_str, (const size_t *)&source_size, &ret);

        ret = clBuildProgram(program, 1, &device_id[deviceIdx], NULL, NULL, NULL);

        dryWetKernel = clCreateKernel(program, "dry_wet", &ret);
        ret = clReleaseProgram(program);
    }
    else
    {
        total_compute_units = max_rt_compute_units = reserved_compute_units = 0;
        res = pTANContext->InitOpenMP(4);
        res = TANCreateConvolution(pTANContext, &pConvolution);
        res = pConvolution->InitCpu(convMethod, responseLength, bufferSize, 2);
    }
}

void ReverbGPUAudioProcessor::updateTANContext()
{
    suspendProcessing(true);

	if (pTANContext != NULL)
	{
		pTANContext->Terminate();
		pTANContext = NULL;
	}
    createTANContext();

    processTimeTotal.reset();
    initialPartTime.reset();
    reverbTanTime.reset();
    mixKernelTime.reset();
    finalPartTime.reset();

    suspendProcessing(false);
}

void ReverbGPUAudioProcessor::updateIR()
{
    AMF_RESULT res;
    juce::AudioFormatManager formatManager;
    auto soundBuffer = std::make_unique<juce::MemoryInputStream>(impulseResponseFile, true);

    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.findFormatForFileExtension("wav")->createReaderFor(soundBuffer.release(), true));

    if (reader != NULL)
    {
        reader->read(&impulseResponses[0], 1, 0, responseLength);
        reader->read(&impulseResponses[1], 1, 0, responseLength);
    }

    reader.reset();

    suspendProcessing(true);
    res = pConvolution->UpdateResponseTD(impulseResponses, responseLength, nullptr, amf::TAN_CONVOLUTION_OPERATION_FLAG_BLOCK_UNTIL_READY);
    suspendProcessing(false);
}

bool ReverbGPUAudioProcessor::deviceIsOpenCL()
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

bool ReverbGPUAudioProcessor::CreateQueuesGPU(cl_context platform_context, cl_command_queue& gp_queue, cl_command_queue& rt_queue)
{
    cl_int error;
    // use clCreateCommandQueueWithProperties to pass custom queue properties to driver:

    total_compute_units = 0;
    clGetDeviceInfo(device_id[deviceIdx], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(total_compute_units), &total_compute_units, NULL);
    max_rt_compute_units = 0;
    clGetDeviceInfo(device_id[deviceIdx], CL_DEVICE_MAX_REAL_TIME_COMPUTE_UNITS_AMD, sizeof(max_rt_compute_units), &max_rt_compute_units, NULL);
    reserved_compute_units = max_rt_compute_units / 2;

    // AMF may mix-up device selection if there is more than one available
// disabled for testing, as AMF does not work with multi-instance
//    if((numOpenCLDevices != 1) || AMF_OK != CreateCommandQueuesVIAamf(deviceIdx, RT_QUEUE, reserved_compute_units, &rt_queue, 0, 0, &gp_queue, AMF_CONTEXT_DEVICE_TYPE_GPU))
    {
// disabled for testing, as CU reservation does not work with multi-instance
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

AMF_RESULT ReverbGPUAudioProcessor::CreateCommandQueuesVIAamf(int deviceIndex, uint64_t type1, int32_t param1, cl_command_queue* pcmdQueue1, uint64_t type2, int32_t param2, cl_command_queue* pcmdQueue2, int amfDeviceType)
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

bool ReverbGPUAudioProcessor::CreateCommandQueuesVIAocl(cl_context clContext, cl_device_id clDevice, uint64_t type1, int32_t param1, cl_command_queue* pcmdQueue1, uint64_t type2, int32_t param2, cl_command_queue* pcmdQueue2)
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

cl_command_queue ReverbGPUAudioProcessor::createQueue(cl_context context, cl_device_id device, uint64_t type, int32_t param)
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
