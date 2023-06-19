#include "PluginProcessor.h"
#include "PluginEditor.h"

GPU_VSTAudioProcessorEditor::GPU_VSTAudioProcessorEditor(GPU_VSTAudioProcessor& p)
    : AudioProcessorEditor(&p),
      mAudioProcessor(p),
      mSpectrumAnalyzer(mAudioProcessor)
{
    addAndMakeVisible(mSpectrumAnalyzer);
    addAndMakeVisible(mChooserDevice);
    addAndMakeVisible(mChooserFFTFrameSize);

    for (int i = 0; i < 5; i++)
    {
        auto text = juce::String(p.devNames[i]);
        if (text.isNotEmpty())
        {
            mChooserDevice.addItem(text, i + 1);
        }
    }

    mChooserDevice.onChange = [this] { chooserDeviceUpdated(); };
    mChooserDevice.setSelectedId(1);

    mChooserFFTFrameSize.addItem("2048", 1);
    mChooserFFTFrameSize.addItem("4096", 2);
    mChooserFFTFrameSize.addItem("8192", 3);

    mChooserFFTFrameSize.onChange = [this] { chooserFFTFrameSizeUpdated(); };
    mChooserFFTFrameSize.setSelectedId(3);

    /* measurementButton init */
    addAndMakeVisible(measurementButton);
    measurementButton.setToggleState(mAudioProcessor.measurementFlag, juce::NotificationType::sendNotification);
    measurementButton.onClick = [this] { measurementButtonUpdated(); };

    setSize(800, 200);

    startTimer(1000);

    // background image init
    mAptivImage = juce::ImageFileFormat::loadFrom(BinaryData::aptiv_background_jpg, BinaryData::aptiv_background_jpgSize);
}

GPU_VSTAudioProcessorEditor::~GPU_VSTAudioProcessorEditor()
{
}

void GPU_VSTAudioProcessorEditor::paint(juce::Graphics& g)
{
    using namespace juce;

    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));

    g.drawImageWithin(mAptivImage, 0, 0, 800, 200, juce::RectanglePlacement(juce::RectanglePlacement::Flags::fillDestination &
        juce::RectanglePlacement::Flags::xLeft));

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawText("Device: ",             20,  70, 80, 30, juce::Justification::left, true);
    g.drawText("Frame size for FFT: ", 20, 110, 150, 30, juce::Justification::left, true);
    g.drawText("Show process time", 20, 150, 150, 30, juce::Justification::left, true);
    if (mAudioProcessor.measurementFlag)
    {
        float processTimeTotal = mAudioProcessor.processTimeTotal.getAverage() / 1000.0f;
        g.drawText(juce::String("Process time: ") + juce::String(processTimeTotal, 1) + " ms", 20, 180, 250, 20, juce::Justification::centred, true);
    }
}

void GPU_VSTAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromLeft(300);
    mSpectrumAnalyzer.setBounds(bounds);

    mChooserDevice.setBounds(90, 70, 200, 30);
    mChooserFFTFrameSize.setBounds(170, 110, 80, 30);
    measurementButton.setBounds(150, 150, 30, 30);
}

void GPU_VSTAudioProcessorEditor::chooserDeviceUpdated()
{
    mAudioProcessor.updateTANContext(mChooserDevice.getSelectedId() - 1);
}

void GPU_VSTAudioProcessorEditor::chooserFFTFrameSizeUpdated()
{
    mSpectrumAnalyzer.changeFFTFrameSize(mChooserFFTFrameSize.getSelectedId() - 1);
}

void GPU_VSTAudioProcessorEditor::measurementButtonUpdated()
{
    mAudioProcessor.measurementFlag = measurementButton.getToggleState();

    mAudioProcessor.processTimeTotal.reset();
}

void GPU_VSTAudioProcessorEditor::timerCallback()
{
    repaint();
}