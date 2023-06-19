#pragma once

#include <JuceHeader.h>

#include "Fifo.h"
#include "AnalyzerPathGenerator.h"
#include "FFTDataGenerator.h"

struct PathProducer
{
    PathProducer(std::vector<std::unique_ptr<SingleChannelSampleFifo>>& scsf, GPU_VSTAudioProcessor& audioProcessor, FFTDataGenerator& FFTDataGenerator);

    void process(juce::Rectangle<float> fftBounds, double sampleRate);
    std::vector<juce::Path>& getPaths();

private:
    std::vector<std::unique_ptr<SingleChannelSampleFifo>>& mChannelFifo;
    juce::AudioBuffer<float> mBuffer;
    FFTDataGenerator& mFFTDataGenerator;
    AnalyzerPathGenerator mAnalyzerPathGenerator;
    std::vector<juce::Path> mChannelFFTPath;
    int mChannelsNo;
    GPU_VSTAudioProcessor& mAudioProcessor;
};
