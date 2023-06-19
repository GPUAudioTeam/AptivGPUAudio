#include "AnalyzerPathGenerator.h"

AnalyzerPathGenerator::AnalyzerPathGenerator(int channels)
{
    for (int i = 0; i < channels; i++)
    {
        mPathFifo.push_back(std::unique_ptr<Fifo<juce::Path>>(new Fifo<juce::Path>));
    }
}

void AnalyzerPathGenerator::generatePath(int channel,
    const std::vector<float>& renderData,
    juce::Rectangle<float> fftBounds,
    int fftSize,
    float binWidth,
    float negativeInfinity)
{
    auto top = 0.f; // this is relative 'top' - will get adjusted in applyTransform()
    auto bottom = fftBounds.getHeight();
    auto width = fftBounds.getWidth();

    int numBins = (int)fftSize / 2;

    juce::Path p;
    p.preallocateSpace(3 * (int)fftBounds.getWidth());

    auto map = [bottom, top, negativeInfinity](float v)
    {
        return juce::jmap(v, negativeInfinity, 0.f, bottom, top);
    };

    auto y = map(renderData[0]);

    if (std::isnan(y) || std::isinf(y) || y>=bottom)
    {
        y = bottom + VISIBILITY_INTERVAL;
    }

    p.startNewSubPath(0, y);

    const int pathResolution = 1;
    for (int binNum = 1; binNum < numBins; binNum += pathResolution)
    {
        y = map(renderData[binNum]);
        if (y >= bottom) y = bottom + VISIBILITY_INTERVAL;
        if (!std::isnan(y) && !std::isinf(y))
        {
            auto binFreq = binNum * binWidth;
            auto normalizedBinX = juce::mapFromLog10(binFreq, 20.f, 20000.f);
            float binX = std::floor(normalizedBinX * width);
            p.lineTo(binX, y);
        }
    }

    mPathFifo[channel]->push(p);
}

int AnalyzerPathGenerator::getNumPathsAvailable(int channel) const
{
    return mPathFifo[channel]->getNumAvailableForReading();
}

bool AnalyzerPathGenerator::getPath(int channel, juce::Path& path)
{
    return mPathFifo[channel]->pull(path);
}
