#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Biquad5GPUAudioProcessorEditor::Biquad5GPUAudioProcessorEditor (Biquad5GPUAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize(800, 200);

    customLookAndFeel.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(246, 64, 24));
    customLookAndFeel.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGBA(130, 130, 130, 100));
    customLookAndFeel.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGBA(246, 64, 24, 100));
    customLookAndFeel.setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGBA(10, 200, 200, 100));
    customLookAndFeel.setColour(juce::Slider::trackColourId, juce::Colour::fromRGBA(246, 64, 24, 100));


    // bass slider
    bassToneSlider.setSliderStyle(juce::Slider::SliderStyle:: LinearVertical);
    bassToneSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    bassToneSlider.setLookAndFeel(&customLookAndFeel);
    bassToneSlider.setRange(-10, 10, 0.1);
    bassToneSlider.setTextValueSuffix("dB");
    bassToneSlider.setValue(0);
    bassToneSlider.addListener(this);
    addAndMakeVisible(bassToneSlider);

    // low slider
    lowToneSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    lowToneSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    lowToneSlider.setLookAndFeel(&customLookAndFeel);
    lowToneSlider.setRange(-10, 10, 0.1);
    lowToneSlider.setTextValueSuffix("dB");
    lowToneSlider.setValue(0);
    lowToneSlider.addListener(this);
    addAndMakeVisible(lowToneSlider);

    // mid slider
    midToneSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    midToneSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    midToneSlider.setLookAndFeel(&customLookAndFeel);
    midToneSlider.setRange(-10, 10, 0.1);
    midToneSlider.setTextValueSuffix("dB");
    midToneSlider.setValue(0);
    midToneSlider.addListener(this);
    addAndMakeVisible(midToneSlider);

    // high slider
    highToneSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    highToneSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    highToneSlider.setLookAndFeel(&customLookAndFeel);
    highToneSlider.setRange(-10, 10, 0.1);
    highToneSlider.setTextValueSuffix("dB");
    highToneSlider.setValue(0);
    highToneSlider.addListener(this);
    addAndMakeVisible(highToneSlider);

    // treble slider
    trebleToneSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    trebleToneSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    trebleToneSlider.setLookAndFeel(&customLookAndFeel);
    trebleToneSlider.setRange(-10, 10, 0.1);
    trebleToneSlider.setTextValueSuffix("dB");
    trebleToneSlider.setValue(0);
    trebleToneSlider.addListener(this);
    addAndMakeVisible(trebleToneSlider);

    startTimer(1000);


    /* chooserDevice init */
    addAndMakeVisible(chooserDevice);

    for (int i = 0; i < NUM_OF_DEVICES; i++)
    {
        auto text = juce::String(p.devNames[i]);
        if (text.isNotEmpty())
        {
            chooserDevice.addItem(text, i + 1);
        }
    }

    chooserDevice.onChange = [this] { chooserDeviceUpdated(); };
    chooserDevice.setSelectedId(audioProcessor.deviceIdx + 1);

    /* chooserDataFlow init */
    addAndMakeVisible(chooserDataFlow);

    chooserDataFlow.addItem("Push-pull", 1);
    chooserDataFlow.addItem("Pull-push", 2);

    chooserDataFlow.onChange = [this] { chooserDataFlowUpdated(); };
    chooserDataFlow.setSelectedId(1);

    /* measurementButton init */
    addAndMakeVisible(measurementButton);
    measurementButton.setToggleState(audioProcessor.measurementFlag, juce::NotificationType::sendNotification);
    measurementButton.onClick = [this] { measurementButtonUpdated(); };

    /* CU reservation queue */
    addAndMakeVisible(reservedCuButton);
    reservedCuButton.setToggleState(audioProcessor.reservedCuFlag, juce::NotificationType::sendNotification);
    reservedCuButton.onClick = [this] { reservedCuButtonUpdated(); };

    /* background image init */
    aptivImage = juce::ImageFileFormat::loadFrom(BinaryData::aptiv_background_jpg, BinaryData::aptiv_background_jpgSize);

}

Biquad5GPUAudioProcessorEditor::~Biquad5GPUAudioProcessorEditor()
{
}

//==============================================================================
void Biquad5GPUAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.drawImageWithin(aptivImage, 0, 0, 800, 200, juce::RectanglePlacement(juce::RectanglePlacement::Flags::fillDestination));

    g.setColour(juce::Colour::fromRGBA(30, 30, 30, 180));
    g.fillRect(500, 10, 250, 180);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(15.0f, juce::Font::bold));

    g.drawText("Use CU reservation", 20, 50, 150, 30, juce::Justification::left, true);
    g.drawText("Show process time",  20, 80, 150, 30, juce::Justification::left, true);
    g.drawText("Device",             20, 120, 80, 30, juce::Justification::left, true);
    g.drawText("Data flow",          20, 160, 80, 30, juce::Justification::left, true);

    float frameLength = (float)audioProcessor.bufferSize / (float)audioProcessor.fs * 1000.0f;
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText(juce::String("CU Rsrvd: ") + juce::String(audioProcessor.reserved_compute_units) + " / Avail: " + juce::String(audioProcessor.max_rt_compute_units) + " / Total: " + juce::String(audioProcessor.total_compute_units), 540, 10, 250, 20, juce::Justification::centred, true);
    g.drawText(juce::String("Frame length: ") + juce::String(frameLength, 1) + " ms", 540, 40, 250, 20, juce::Justification::centred, true);

    g.drawText("Bass",   120+120-5,  5,  60, 30, juce::Justification::centred, true);
    g.drawText("Low",    120+180-5,  5,  60, 30, juce::Justification::centred, true);
    g.drawText("Mid",    120+240-5,  5,  60, 30, juce::Justification::centred, true);
    g.drawText("High",   120+300-5,  5,  60, 30, juce::Justification::centred, true);
    g.drawText("Treble", 120+360-5,  5,  60, 30, juce::Justification::centred, true);

    if (audioProcessor.measurementFlag)
    {
        float processTimeTotal = audioProcessor.processTimeTotal.getAverage();
        float initialPartTime = audioProcessor.initialPartTime.getAverage();
        float biquadTanTime = audioProcessor.biquadTanTime.getAverage();
        float finalPartTime = audioProcessor.finalPartTime.getAverage();

        if (processTimeTotal > frameLength)
        {
            g.setColour(juce::Colours::red);
        }

        g.drawText(juce::String("Total process time: ") + juce::String(processTimeTotal, 1) + " ms", 540, 60, 250, 20, juce::Justification::centred, true);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(13.0f, juce::Font::plain));

        g.drawText(juce::String("Initial part: ") + juce::String(initialPartTime, 3) + " ms", 540, 100, 250, 20, juce::Justification::centred, true);
        g.drawText(juce::String("Biquad process: ") + juce::String(biquadTanTime, 3) + " ms", 540, 120, 250, 20, juce::Justification::centred, true);

        if (audioProcessor.deviceIsOpenCL() && !audioProcessor.firstPullFlag)
        {
            g.drawText(juce::String("Final part: ") + juce::String(finalPartTime, 3) + " ms", 540, 140, 250, 20, juce::Justification::centred, true);
        }
        else
        {
            g.drawText(juce::String("Final part: -"), 540, 140, 250, 20, juce::Justification::centred, true);
        }
    }
}

void Biquad5GPUAudioProcessorEditor::resized()
{
    bassToneSlider.setBounds(  120+120, 40, 50, 140);
    lowToneSlider.setBounds(   120+180, 40, 50, 140);
    midToneSlider.setBounds(   120+240, 40, 50, 140);
    highToneSlider.setBounds(  120+300, 40, 50, 140);
    trebleToneSlider.setBounds(120+360, 40, 50, 140);

    chooserDevice.setBounds(100, 120, 120, 30);
    chooserDataFlow.setBounds(100, 160, 120, 30);
    reservedCuButton.setBounds(160, 50, 30, 30);
    measurementButton.setBounds(160, 80, 30, 30);
}

void Biquad5GPUAudioProcessorEditor::timerCallback()
{
    repaint();
}

void Biquad5GPUAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    (void)slider;
    // read settings and update params
    float gains[5];
    gains[0] = float(bassToneSlider.getValue());
    gains[1] = float(lowToneSlider.getValue());
    gains[2] = float(midToneSlider.getValue());
    gains[3] = float(highToneSlider.getValue());
    gains[4] = float(trebleToneSlider.getValue());
    audioProcessor.updateParams(gains);
}

void Biquad5GPUAudioProcessorEditor::chooserDeviceUpdated()
{
    audioProcessor.deviceIdx = chooserDevice.getSelectedId() - 1;
    audioProcessor.updatePlatformContext();

    if (audioProcessor.deviceIsOpenCL())
    {
        chooserDataFlow.setEnabled(1);
    }
    else
    {
        chooserDataFlow.setEnabled(0);
        chooserDataFlow.setSelectedId(1);
    }
}

void Biquad5GPUAudioProcessorEditor::chooserDataFlowUpdated()
{
    switch (chooserDataFlow.getSelectedId())
    {
        case 1: audioProcessor.firstPullFlag = false;  break;   //push-pull
        case 2: audioProcessor.firstPullFlag = true;   break;   //pull-push
    }

    audioProcessor.processTimeTotal.reset();
    audioProcessor.initialPartTime.reset();
    audioProcessor.biquadTanTime.reset();
    audioProcessor.finalPartTime.reset();
}

void Biquad5GPUAudioProcessorEditor::measurementButtonUpdated()
{
    audioProcessor.measurementFlag = measurementButton.getToggleState();

    audioProcessor.processTimeTotal.reset();
    audioProcessor.initialPartTime.reset();
    audioProcessor.biquadTanTime.reset();
    audioProcessor.finalPartTime.reset();
}

void Biquad5GPUAudioProcessorEditor::reservedCuButtonUpdated()
{
    audioProcessor.reservedCuFlag = reservedCuButton.getToggleState();
}
