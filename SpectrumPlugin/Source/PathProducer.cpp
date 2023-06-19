#include "PathProducer.h"

PathProducer::PathProducer(std::vector<std::unique_ptr<SingleChannelSampleFifo>>& scsf,
    GPU_VSTAudioProcessor& audioProcessor,
    FFTDataGenerator& FFTDataGenerator) :
    mChannelFifo(scsf),
    mFFTDataGenerator(FFTDataGenerator),
    mAnalyzerPathGenerator(audioProcessor.getChannelsNo()),
    mChannelsNo(audioProcessor.getChannelsNo()),
    mAudioProcessor(audioProcessor)
{
    mFFTDataGenerator.changeOrder(FFTOrder::order8192);
    mBuffer.setSize(mChannelsNo, mFFTDataGenerator.getFFTSize());

    for (int i = 0; i < mChannelsNo; i++)
    {
        mChannelFFTPath.push_back(juce::Path());
    }
}

void PathProducer::process(juce::Rectangle<float> fftBounds, double sampleRate)
{
    juce::AudioBuffer<float> bufferToProcess;
    bool bufferUpdate = false;
    juce::ignoreUnused(bufferUpdate);

    for (int i = 0; i < mChannelsNo; i++)
    {
        while (mChannelFifo[i]->getNumCompleteBuffersAvailable() > 0)
        {
            if (mChannelFifo[i]->getAudioBuffer(bufferToProcess))
            {
                auto size = bufferToProcess.getNumSamples();

                juce::FloatVectorOperations::copy(mBuffer.getWritePointer(i, 0),
                    mBuffer.getReadPointer(i, size),
                    mBuffer.getNumSamples() - size);

                juce::FloatVectorOperations::copy(mBuffer.getWritePointer(i, mBuffer.getNumSamples() - size),
                    bufferToProcess.getReadPointer(0, 0),
                    size);
            }
        }
    }

    LARGE_INTEGER StartingTime, EndingTimeTotal;
    LARGE_INTEGER Frequency;

    QueryPerformanceFrequency(&Frequency);
    QueryPerformanceCounter(&StartingTime);

    mFFTDataGenerator.produceFFTDataForRendering(mBuffer, -48.f);

    QueryPerformanceCounter(&EndingTimeTotal);
    float timer = float(EndingTimeTotal.QuadPart - StartingTime.QuadPart);
    timer *= 1000000.0f / Frequency.QuadPart;
    mAudioProcessor.processTimeTotal.addValue(timer);

    const auto fftSize = mFFTDataGenerator.getFFTSize();
    const auto binWidth = float(sampleRate / fftSize);

    for (int i = 0; i < mChannelsNo; i++)
    {
        while (mFFTDataGenerator.getNumAvailableFFTDataBlocks(i) > 0)
        {
            std::vector<float> fftData;
            if (mFFTDataGenerator.getFFTData(i, fftData))
            {
                mAnalyzerPathGenerator.generatePath(i, fftData, fftBounds, fftSize, binWidth, -48.f);
            }
        }

        while (mAnalyzerPathGenerator.getNumPathsAvailable(i) > 0)
        {
            mAnalyzerPathGenerator.getPath(i, mChannelFFTPath[i]);
        }
    }
}

std::vector<juce::Path>& PathProducer::getPaths()
{
    return mChannelFFTPath;
}
