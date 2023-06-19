#pragma once

#include <JuceHeader.h>

#include <vector>
#include <complex>

#include "PluginProcessor.h"
#include "Fifo.h"
#include "PathProducer.h"
#include "FFTDataGenerator.h"

class PathColour
{
public:
    PathColour::PathColour() : index(0),
        mColours{ juce::Colour(245u, 245u, 0u),   // yellow
                  juce::Colour(228u, 165u, 97u)   // orange
    }
    {}

    juce::Colour getColour()
    {
        if (index >= mColours.size())
        {
            index = 0;
        }

        return mColours[index++];
    }

private:
    std::vector<juce::Colour> mColours;
    unsigned index;
};

struct SpectrumAnalyzer : juce::Component,
                          juce::Timer
{
    SpectrumAnalyzer(GPU_VSTAudioProcessor&);
    ~SpectrumAnalyzer();

    void timerCallback() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void changeFFTFrameSize(int optId);

private:
    GPU_VSTAudioProcessor& mAudioProcessor;

    void drawBackgroundGrid(juce::Graphics& g);
    void drawTextLabels(juce::Graphics& g);

    std::vector<float> getFrequencies();
    std::vector<float> getGains();
    std::vector<float> getXs(const std::vector<float>& freqs, float left, float width);
    juce::Rectangle<int> getRenderArea();
    juce::Rectangle<int> getAnalysisArea();

    FFTDataGenerator mFFTDataGenerator;
    PathProducer mPathProducer;

    PathColour mColour;
};
