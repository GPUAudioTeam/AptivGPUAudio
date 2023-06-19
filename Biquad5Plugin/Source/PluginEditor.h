#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class Biquad5GPUAudioProcessorEditor  : public juce::AudioProcessorEditor,
    public juce::Timer, public juce::Slider::Listener
{
public:
    Biquad5GPUAudioProcessorEditor (Biquad5GPUAudioProcessor&);
    ~Biquad5GPUAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void sliderValueChanged (juce::Slider* slider) override;
    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Biquad5GPUAudioProcessor& audioProcessor;

    juce::LookAndFeel_V4 customLookAndFeel;

    // Tone controls
    juce::Slider bassToneSlider;
    juce::Slider lowToneSlider;
    juce::Slider midToneSlider;
    juce::Slider highToneSlider;
    juce::Slider trebleToneSlider;

    juce::ComboBox chooserDevice;
    juce::ComboBox chooserDataFlow;

    juce::Image aptivImage;
    juce::ToggleButton measurementButton;
    juce::ToggleButton reservedCuButton;

    void chooserDeviceUpdated();
    void chooserDataFlowUpdated();
    void measurementButtonUpdated();
    void reservedCuButtonUpdated();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Biquad5GPUAudioProcessorEditor)
};
