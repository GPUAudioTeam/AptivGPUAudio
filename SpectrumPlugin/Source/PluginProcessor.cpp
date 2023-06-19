#include "PluginProcessor.h"
#include "PluginEditor.h"

int GPU_VSTAudioProcessor::getOpenCLDevices(char *dev_names[], unsigned int count, cl_device_id *dev_id, cl_platform_id *platf_id)
{

    cl_uint i, j;
    int idx = 0;
    size_t valueSize;
    cl_uint platformCount;
    cl_uint deviceCount;

    juce::ignoreUnused(count);

    // get all platforms
    clGetPlatformIDs(0, NULL, &platformCount);
    clGetPlatformIDs(platformCount, platf_id, NULL);

    for (i = 0; i < platformCount; i++)
    {
        // get all devices
        clGetDeviceIDs(platf_id[idx], CL_DEVICE_TYPE_ALL, 0, NULL, &deviceCount);
        clGetDeviceIDs(platf_id[idx], CL_DEVICE_TYPE_ALL, deviceCount, &dev_id[idx], NULL);

        for (j = 0; j < deviceCount; j++)
        {
            clGetDeviceInfo(dev_id[idx], CL_DEVICE_NAME, 0, NULL, &valueSize);
            clGetDeviceInfo(dev_id[idx], CL_DEVICE_NAME, valueSize, dev_names[idx], NULL);

            idx++;
        }
    }
	numOpenCLDevices = idx;

	// add CPU handled without openCL (AMD does no support CPU openCL anymore)
	strcpy(devNames[idx], "CPU-openMP");
	device_id[idx] = NULL;
	platform_id[idx] = NULL;
	idx++;

	return 0;
}

void GPU_VSTAudioProcessor::createTANContext(unsigned int deviceIdx)
{
    AMF_RESULT res = TANCreateContext(TAN_FULL_VERSION, &pTANContext);

	currentDeviceIdx = deviceIdx;
    if (isOpenCLDevice())
    {
        cl_int error = CL_DEVICE_NOT_FOUND;
        cl_context_properties contextProps[3] =
        {
            CL_CONTEXT_PLATFORM,
            (cl_context_properties)platform_id[deviceIdx],
            0
        };

        cl_context gpu_context = clCreateContext(contextProps, 1, &device_id[deviceIdx], NULL, NULL, &error);
        res = pTANContext->InitOpenCL(clCreateCommandQueue(gpu_context, device_id[deviceIdx], NULL, &error),
                                      clCreateCommandQueue(gpu_context, device_id[deviceIdx], NULL, &error));

		cl_int ret;
		ret = clReleaseKernel(modulusKernel);

		const char* source_str = BinaryData::modulus_kernel_cl;
		size_t source_size = BinaryData::modulus_kernel_clSize;

		cl_program program = clCreateProgramWithSource(pTANContext->GetOpenCLContext(), 1,
			(const char **)&source_str, (const size_t *)&source_size, &ret);

		ret = clBuildProgram(program, 1, &device_id[deviceIdx], NULL, NULL, NULL);
		modulusKernel = clCreateKernel(program, "modulus", &ret);
		ret = clReleaseProgram(program);

		cl_context context = pTANContext->GetOpenCLContext();
		for (int i = 0; i < MAX_CHANNELS; i++)
		{
			clInBuff[i] = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float)*MAX_FFT_SIZE * 2, NULL, &ret);
			clOutBuff[i] = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float)*MAX_FFT_SIZE * 2, NULL, &ret);
		}
	}
    else
    {
        res = pTANContext->InitOpenMP(4);
    }

    res = TANCreateFFT(pTANContext, &pFFT);
    res = pFFT->Init();
}

void GPU_VSTAudioProcessor::updateTANContext(unsigned int deviceIdx)
{
    if (pTANContext)
    {
        pTANContext->Terminate();
    }

    if (pFFT)
    {
        pFFT->Terminate();
    }
	if (isOpenCLDevice())
	{
		for (int i = 0; i < MAX_CHANNELS; i++)
		{
			clReleaseMemObject(clInBuff[i]);
			clReleaseMemObject(clOutBuff[i]);
		}
	}

    createTANContext(deviceIdx);
}

bool GPU_VSTAudioProcessor::isOpenCLDevice()
{
	if (currentDeviceIdx >= numOpenCLDevices) // CPU handled without openCL is last on the list
	{
		return false;
	}

    // we only treat GPU devices via OpenCL
    cl_device_type type;
    size_t valueSize;

	clGetDeviceInfo(device_id[currentDeviceIdx], CL_DEVICE_TYPE, 0, NULL, &valueSize);
    clGetDeviceInfo(device_id[currentDeviceIdx], CL_DEVICE_TYPE, valueSize, &type, NULL);

    if (type == CL_DEVICE_TYPE_GPU)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

amf::TANFFT* GPU_VSTAudioProcessor::getTANFFT()
{
    return pFFT;
}

GPU_VSTAudioProcessor::GPU_VSTAudioProcessor()
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
    mChannelsNo = getTotalNumInputChannels();
    for (int i = 0; i < mChannelsNo; i++)
    {
        mChannelFifo.push_back(std::unique_ptr<SingleChannelSampleFifo>(new SingleChannelSampleFifo(i)));
    }
	currentDeviceIdx = 0;
    modulusKernel = NULL;
}

GPU_VSTAudioProcessor::~GPU_VSTAudioProcessor()
{
    cl_int ret = 0;

    if (pTANContext)
    {
        pTANContext->Terminate();
    }

    if (pFFT)
    {
        pFFT->Terminate();
    }

    ret = clReleaseKernel(modulusKernel);

    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        clReleaseMemObject(clInBuff[i]);
        clReleaseMemObject(clOutBuff[i]);
    }
}

const juce::String GPU_VSTAudioProcessor::getName() const
{
    return JucePlugin_Name;
}
bool GPU_VSTAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}
bool GPU_VSTAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}
bool GPU_VSTAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}
double GPU_VSTAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}
int GPU_VSTAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}
int GPU_VSTAudioProcessor::getCurrentProgram()
{
    return 0;
}
void GPU_VSTAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}
const juce::String GPU_VSTAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}
void GPU_VSTAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index);
    juce::ignoreUnused(newName);
}

void GPU_VSTAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    unsigned int deviceIdx = 0;

    juce::ignoreUnused(sampleRate);

    devNames[0] = devName0;
    devNames[1] = devName1;
    devNames[2] = devName2;
    devNames[3] = devName3;
    devNames[4] = devName4;

    getOpenCLDevices(devNames, 5, &device_id[0], &platform_id[0]);

    createTANContext(deviceIdx);
    gpuName = juce::String(devNames[deviceIdx]);

    for (auto &fifo : mChannelFifo)
    {
        fifo->prepare(samplesPerBlock);
    }
}

void GPU_VSTAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool GPU_VSTAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

void GPU_VSTAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    juce::ignoreUnused(midiMessages);

    for (auto &fifo : mChannelFifo)
    {
        fifo->update(buffer);
    }
}

bool GPU_VSTAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* GPU_VSTAudioProcessor::createEditor()
{
    return new GPU_VSTAudioProcessorEditor(*this);
}

void GPU_VSTAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused(destData);
}

void GPU_VSTAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused(data);
    juce::ignoreUnused(sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GPU_VSTAudioProcessor();
}

int GPU_VSTAudioProcessor::getChannelsNo() const
{
    return mChannelsNo;
}
