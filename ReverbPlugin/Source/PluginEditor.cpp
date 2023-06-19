#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ReverbGPUAudioProcessorEditor::ReverbGPUAudioProcessorEditor(ReverbGPUAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(800, 200);

    customLookAndFeel.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(246, 64, 24));
    customLookAndFeel.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGBA(130, 130, 130, 100));
    customLookAndFeel.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGBA(246, 64, 24, 100));

    startTimer(1000);

    /* chooserIR init */
    addAndMakeVisible(chooserIR);

    chooserIR.addItem("Church", 1);
    chooserIR.addItem("Concert hall", 2);
    chooserIR.addItem("Jazz club", 3);

    chooserIR.onChange = [this] { chooserIRUpdated(); };
    chooserIR.setSelectedId(audioProcessor.impulseResponseIdx + 1);

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

    /* dryWet init */
    dryWetSlider.setSliderStyle(juce::Slider::SliderStyle::Rotary);
    dryWetSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    dryWetSlider.setLookAndFeel(&customLookAndFeel);
    dryWetSlider.setRange(0, 100, 1);
    dryWetSlider.setTextValueSuffix(" %");
    dryWetSlider.setValue(50);
    dryWetSlider.getValueObject().referTo(audioProcessor.dryWet);
    addAndMakeVisible(dryWetSlider);

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

ReverbGPUAudioProcessorEditor::~ReverbGPUAudioProcessorEditor()
{
}

//==============================================================================
void ReverbGPUAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.drawImageWithin(aptivImage, 0, 0, 800, 200, juce::RectanglePlacement(juce::RectanglePlacement::Flags::fillDestination));

    g.setColour(juce::Colour::fromRGBA(30, 30, 30, 180));
    g.fillRect(500, 10, 250, 180);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(15.0f, juce::Font::bold));

    g.drawText("IR type", 20, 80, 80, 30, juce::Justification::left, true);
    g.drawText("Device", 20, 120, 80, 30, juce::Justification::left, true);
    g.drawText("Data flow", 20, 160, 80, 30, juce::Justification::left, true);

    g.drawText("Use CU reservation", 300, 10, 150, 30, juce::Justification::left, true);
    g.drawText("Show process time", 300, 40, 150, 30, juce::Justification::left, true);
    g.drawText("Dry/Wet", 350, 70, 80, 30, juce::Justification::centred, true);

    float frameLength = (float)audioProcessor.bufferSize / (float)audioProcessor.fs * 1000.0f;

    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText(juce::String("Rsrvd CU: ") + juce::String(audioProcessor.reserved_compute_units) + " / Avail CU: " + juce::String(audioProcessor.max_rt_compute_units) + " / Total CU: " + juce::String(audioProcessor.total_compute_units), 500, 10, 250, 20, juce::Justification::centred, true);
    g.drawText(juce::String("Frame length: ") + juce::String(frameLength, 1) + " ms", 500, 40, 250, 20, juce::Justification::centred, true);

    if (audioProcessor.measurementFlag)
    {
        float processTimeTotal = audioProcessor.processTimeTotal.getAverage();
        float initialPartTime = audioProcessor.initialPartTime.getAverage();
        float reverbTanTime = audioProcessor.reverbTanTime.getAverage();
        float mixKernelTime = audioProcessor.mixKernelTime.getAverage();
        float finalPartTime = audioProcessor.finalPartTime.getAverage();

        if (processTimeTotal > frameLength)
        {
            g.setColour(juce::Colours::red);
        }

        g.drawText(juce::String("Total process time: ") + juce::String(processTimeTotal, 1) + " ms", 500, 60, 250, 20, juce::Justification::centred, true);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(13.0f, juce::Font::plain));

        g.drawText(juce::String("Initial part: ") + juce::String(initialPartTime, 3) + " ms", 500, 100, 250, 20, juce::Justification::centred, true);
        g.drawText(juce::String("Reverb process: ") + juce::String(reverbTanTime, 3) + " ms", 500, 120, 250, 20, juce::Justification::centred, true);

        if (audioProcessor.deviceIsOpenCL() && !audioProcessor.firstPullFlag)
        {
            g.drawText(juce::String("Dry/wet process: ") + juce::String(mixKernelTime, 3) + " ms", 500, 140, 250, 20, juce::Justification::centred, true);
            g.drawText(juce::String("Final part: ") + juce::String(finalPartTime, 3) + " ms", 500, 160, 250, 20, juce::Justification::centred, true);
        }
        else
        {
            g.drawText(juce::String("Dry/wet process: -"), 500, 140, 250, 20, juce::Justification::centred, true);
            g.drawText(juce::String("Final part: -"), 500, 160, 250, 20, juce::Justification::centred, true);
        }
    }

}

void ReverbGPUAudioProcessorEditor::resized()
{
    chooserIR.setBounds(100, 80, 200, 30);
    chooserDevice.setBounds(100, 120, 200, 30);
    chooserDataFlow.setBounds(100, 160, 200, 30);
    reservedCuButton.setBounds(450, 10, 30, 30);
    measurementButton.setBounds(450, 40, 30, 30);
    dryWetSlider.setBounds(350, 100, 80, 80);
}

void ReverbGPUAudioProcessorEditor::timerCallback()
{
    repaint();
}

void ReverbGPUAudioProcessorEditor::chooserIRUpdated()
{
    switch (chooserIR.getSelectedId())
    {
        case 1: audioProcessor.impulseResponseFile = juce::MemoryBlock(BinaryData::Church_wav, BinaryData::Church_wavSize);             break;
        case 2: audioProcessor.impulseResponseFile = juce::MemoryBlock(BinaryData::ConcertHall_wav, BinaryData::ConcertHall_wavSize);   break;
        case 3: audioProcessor.impulseResponseFile = juce::MemoryBlock(BinaryData::JazzClub_wav, BinaryData::JazzClub_wavSize);         break;
    }

    audioProcessor.impulseResponseIdx = chooserIR.getSelectedId() - 1;
    audioProcessor.updateIR();
}

void ReverbGPUAudioProcessorEditor::chooserDeviceUpdated()
{
    audioProcessor.deviceIdx = chooserDevice.getSelectedId() - 1;
    audioProcessor.updateTANContext();
    chooserIRUpdated();

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

void ReverbGPUAudioProcessorEditor::chooserDataFlowUpdated()
{
    switch (chooserDataFlow.getSelectedId())
    {
        case 1: audioProcessor.firstPullFlag = false;  break;   //push-pull
        case 2: audioProcessor.firstPullFlag = true;   break;   //pull-push
    }

    audioProcessor.processTimeTotal.reset();
    audioProcessor.initialPartTime.reset();
    audioProcessor.reverbTanTime.reset();
    audioProcessor.mixKernelTime.reset();
    audioProcessor.finalPartTime.reset();
}

void ReverbGPUAudioProcessorEditor::measurementButtonUpdated()
{
    audioProcessor.measurementFlag = measurementButton.getToggleState();

    audioProcessor.processTimeTotal.reset();
    audioProcessor.initialPartTime.reset();
    audioProcessor.reverbTanTime.reset();
    audioProcessor.mixKernelTime.reset();
    audioProcessor.finalPartTime.reset();
}

void ReverbGPUAudioProcessorEditor::reservedCuButtonUpdated()
{
    audioProcessor.reservedCuFlag = reservedCuButton.getToggleState();
}
