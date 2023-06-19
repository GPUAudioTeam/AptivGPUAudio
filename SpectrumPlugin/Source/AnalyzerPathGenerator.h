#pragma once

#include <JuceHeader.h>
#include <vector>
#include "Fifo.h"

struct AnalyzerPathGenerator
{
    static constexpr int VISIBILITY_INTERVAL = 10;

    AnalyzerPathGenerator::AnalyzerPathGenerator(int channels);

    void generatePath(int channel,
        const std::vector<float>& renderData,
        juce::Rectangle<float> fftBounds,
        int fftSize,
        float binWidth,
        float negativeInfinity);

    int getNumPathsAvailable(int channel) const;
    bool getPath(int channel, juce::Path& path);

private:
    std::vector<std::unique_ptr<Fifo<juce::Path>>> mPathFifo;
};
