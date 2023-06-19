#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

struct FFTDataGenerator
{
    FFTDataGenerator::FFTDataGenerator(GPU_VSTAudioProcessor& audioProcessor);
    FFTDataGenerator::~FFTDataGenerator();

    void produceFFTDataForRendering(const juce::AudioBuffer<float>& audioData, const float negativeInfinity);
    void changeOrder(FFTOrder newOrder);

    int getFFTSize() const;
    int getNumAvailableFFTDataBlocks(int channel) const;
    bool getFFTData(int channel, std::vector<float>& fftData);

private:
    FFTOrder mFFTOrder;
    std::vector<std::vector<float>> mFFTData;
    GPU_VSTAudioProcessor& mAudioProcessor;
    std::vector<std::unique_ptr<Fifo<std::vector<float>>>> mFFTDataFifo;
    int mChannelsNo;
};
