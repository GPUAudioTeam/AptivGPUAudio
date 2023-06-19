#pragma once

#include <JuceHeader.h>
#include "TrueAudioNext.h"
#include "CL/cl.h"
#include "Fifo.h"

enum FFTOrder
{
    order2048 = 11,
    order4096 = 12,
    order8192 = 13,
    orderMAX = order8192
};

constexpr int MAX_FFT_SIZE = (1 << FFTOrder::orderMAX);
constexpr int MAX_CHANNELS = 8;

class GPU_VSTAudioProcessor : public juce::AudioProcessor
{
public:
    GPU_VSTAudioProcessor();
    ~GPU_VSTAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

	unsigned int currentDeviceIdx;
    char *devNames[5];
    juce::String gpuName;

    void updateTANContext(unsigned int deviceIdx);
    bool isOpenCLDevice();

    juce::StatisticsAccumulator<float> processTimeTotal;

    std::vector<std::unique_ptr<SingleChannelSampleFifo>> mChannelFifo;

    amf::TANContext* getTANContext() { return pTANContext; };
    amf::TANFFT* getTANFFT();
    cl_kernel& getModulusKernel() { return modulusKernel; };

    int getChannelsNo() const;

    bool measurementFlag;

    cl_mem clInBuff[MAX_CHANNELS];
    cl_mem clOutBuff[MAX_CHANNELS];

private:
    int getOpenCLDevices(char *devNames[], unsigned int count, cl_device_id *device_id, cl_platform_id *platform_id);
    void createTANContext(unsigned int deviceIdx);

    amf::TANContext *pTANContext = nullptr;
    amf::TANFFT *pFFT = nullptr;

	cl_uint numOpenCLDevices;
    cl_device_id device_id[5] = { NULL };
    cl_platform_id platform_id[5] = { NULL };
    cl_kernel modulusKernel;

    char devName0[100] = { 0 };
    char devName1[100] = { 0 };
    char devName2[100] = { 0 };
    char devName3[100] = { 0 };
    char devName4[100] = { 0 };

    int mChannelsNo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GPU_VSTAudioProcessor)
};
