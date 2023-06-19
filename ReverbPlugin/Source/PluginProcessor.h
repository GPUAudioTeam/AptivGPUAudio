#pragma once

#include <JuceHeader.h>
#include "TrueAudioNext.h"
#include "CL/cl.h"
//AMF
#include "public/include/core/Variant.h"
#include "public/include/core/Context.h"
#include "public/include/core/ComputeFactory.h"
#include "public/common/AMFFactory.h"

//==============================================================================
/**
*/

const int NUM_OF_DEVICES = 5;
const int NAME_LENGTH = 128;
const int STEREO_CHANNELS = 2;

class ReverbGPUAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    ReverbGPUAudioProcessor();
    ~ReverbGPUAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChnnelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void updateIR();
    void updateTANContext();
    bool deviceIsOpenCL();

    juce::Value dryWet;

    juce::StatisticsAccumulator<float> processTimeTotal;
    juce::StatisticsAccumulator<float> initialPartTime;
    juce::StatisticsAccumulator<float> reverbTanTime;
    juce::StatisticsAccumulator<float> mixKernelTime;
    juce::StatisticsAccumulator<float> finalPartTime;

    int bufferSize;
    int fs;

    unsigned int deviceIdx;
    int impulseResponseIdx;
    juce::MemoryBlock impulseResponseFile;

    char *devNames[NUM_OF_DEVICES];
    bool measurementFlag;
    bool reservedCuFlag;
    bool firstPullFlag;

    int total_compute_units;
    int max_rt_compute_units;
    int reserved_compute_units;

private:
    //==============================================================================

    void getOpenCLDevices();
    cl_command_queue getOpenCLActiveQueue();
    bool CreateQueuesGPU(cl_context platform_context, cl_command_queue& gp_queue, cl_command_queue& rt_queue);
    AMF_RESULT CreateCommandQueuesVIAamf(int deviceIndex, uint64_t type1, int32_t param1, cl_command_queue* pcmdQueue1, uint64_t type2, int32_t param2, cl_command_queue* pcmdQueue2, int amfDeviceType = AMF_CONTEXT_DEVICE_TYPE_GPU);
    bool CreateCommandQueuesVIAocl(cl_context clContext, cl_device_id clDevice, uint64_t type1, int32_t param1, cl_command_queue* pcmdQueue1, uint64_t type2, int32_t param2, cl_command_queue* pcmdQueue2);
    cl_command_queue createQueue(cl_context context, cl_device_id device, uint64_t flag, int32_t var);

    void createTANContext();

	cl_context platformContext;
    amf::TANContext *pTANContext;
    amf::TANConvolution *pConvolution;

	cl_command_queue GPqueue;
	cl_command_queue RTqueue;

    float *impulseResponses[STEREO_CHANNELS];
    float *inData[STEREO_CHANNELS];
    float *outData[STEREO_CHANNELS];

    cl_uint numOpenCLDevices;
    cl_device_id device_id[NUM_OF_DEVICES];
    cl_platform_id platform_id[NUM_OF_DEVICES];

    cl_mem inClBuffer[STEREO_CHANNELS];
    cl_mem outClBuffer[STEREO_CHANNELS];
    cl_kernel dryWetKernel;

    int responseLength;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbGPUAudioProcessor)
};
