#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SpectrumAnalyzer.h"

class GPU_VSTAudioProcessorEditor : public juce::AudioProcessorEditor,
    public juce::Timer
{
public:
    GPU_VSTAudioProcessorEditor(GPU_VSTAudioProcessor&);
    ~GPU_VSTAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    GPU_VSTAudioProcessor& mAudioProcessor;

    SpectrumAnalyzer mSpectrumAnalyzer;
    juce::ComboBox mChooserDevice;
    juce::ComboBox mChooserFFTFrameSize;
    juce::ToggleButton measurementButton;

    juce::Image mAptivImage;

    void chooserDeviceUpdated();
    void chooserFFTFrameSizeUpdated();

    void measurementButtonUpdated();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GPU_VSTAudioProcessorEditor)
};
