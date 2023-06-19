#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class ReverbGPUAudioProcessorEditor : public juce::AudioProcessorEditor,
    public juce::Timer
{
public:
    ReverbGPUAudioProcessorEditor(ReverbGPUAudioProcessor&);
    ~ReverbGPUAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    ReverbGPUAudioProcessor& audioProcessor;

    juce::LookAndFeel_V4 customLookAndFeel;

    juce::ComboBox chooserIR;
    juce::ComboBox chooserDevice;
    juce::ComboBox chooserDataFlow;

    juce::Slider dryWetSlider;
    juce::Image aptivImage;
    juce::ToggleButton measurementButton;
    juce::ToggleButton reservedCuButton;

    void chooserIRUpdated();
    void chooserDeviceUpdated();
    void chooserDataFlowUpdated();
    void measurementButtonUpdated();
    void reservedCuButtonUpdated();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbGPUAudioProcessorEditor)
};
