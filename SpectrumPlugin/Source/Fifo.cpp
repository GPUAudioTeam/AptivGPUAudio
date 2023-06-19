#include "Fifo.h"

SingleChannelSampleFifo::SingleChannelSampleFifo(int ch)
    : channelToUse(ch)
{
    prepared.set(false);
}

void SingleChannelSampleFifo::update(const juce::AudioBuffer<float>& buffer)
{
    jassert(prepared.get());
    jassert(buffer.getNumChannels() > channelToUse);
    auto* channelPtr = buffer.getReadPointer(channelToUse);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        pushNextSampleIntoFifo(channelPtr[i]);
    }
}

void SingleChannelSampleFifo::prepare(int bufferSize)
{
    prepared.set(false);
    size.set(bufferSize);
    bufferToFill.setSize(1, bufferSize, false, true, true);
    audioBufferFifo.prepare(1, bufferSize);
    fifoIndex = 0;
    prepared.set(true);
}

int SingleChannelSampleFifo::getNumCompleteBuffersAvailable() const
{
    return audioBufferFifo.getNumAvailableForReading();
}

bool SingleChannelSampleFifo::isPrepared() const
{
    return prepared.get();
}

int SingleChannelSampleFifo::getSize() const
{
    return size.get();
}

bool SingleChannelSampleFifo::getAudioBuffer(juce::AudioBuffer<float>& buf)
{
    return audioBufferFifo.pull(buf);
}

void SingleChannelSampleFifo::pushNextSampleIntoFifo(float sample)
{
    if (fifoIndex == bufferToFill.getNumSamples())
    {
        auto ok = audioBufferFifo.push(bufferToFill);
        juce::ignoreUnused(ok);
        fifoIndex = 0;
    }

    bufferToFill.setSample(0, fifoIndex, sample);
    ++fifoIndex;
}
