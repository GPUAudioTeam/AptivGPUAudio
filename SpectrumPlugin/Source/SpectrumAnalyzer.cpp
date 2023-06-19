#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer(GPU_VSTAudioProcessor& p) :
    mAudioProcessor(p),
    mFFTDataGenerator(mAudioProcessor),
    mPathProducer(mAudioProcessor.mChannelFifo, mAudioProcessor, mFFTDataGenerator)
{
    startTimerHz(60);
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    using namespace juce;

    g.fillAll(Colour::fromRGBA(40, 40, 40, 60));
    drawBackgroundGrid(g);

    auto responseArea = getAnalysisArea();

    for (auto& path : mPathProducer.getPaths())
    {
        path.applyTransform(AffineTransform().translation(float(responseArea.getX()), float(responseArea.getY())));
        g.setColour(mColour.getColour());
        g.strokePath(path, PathStrokeType(1.f));
    }

    Path border;
    border.setUsingNonZeroWinding(false);
    border.addRoundedRectangle(getRenderArea(), 4);
    border.addRectangle(getLocalBounds());
    g.setColour(Colour::fromRGBA(40, 40, 40, 60));
    g.fillPath(border);
    drawTextLabels(g);

    g.setColour(Colour(246u, 64u, 24u));
    g.drawRoundedRectangle(getRenderArea().toFloat(), 4.f, 1.f);
}

std::vector<float> SpectrumAnalyzer::getFrequencies()
{
    return std::vector<float>
    {
        20,
        50,
        100,
        200,
        500,
        1000,
        2000,
        5000,
        10000,
        20000
    };
}

std::vector<float> SpectrumAnalyzer::getGains()
{
    return std::vector<float>
    {
        -48,
        -36,
        -24,
        -12,
          0
    };
}

std::vector<float> SpectrumAnalyzer::getXs(const std::vector<float> &freqs, float left, float width)
{
    std::vector<float> xs;
    for (auto f : freqs)
    {
        auto normX = juce::mapFromLog10(f, 20.f, 20000.f);
        xs.push_back(left + width * normX);
    }

    return xs;
}

void SpectrumAnalyzer::drawBackgroundGrid(juce::Graphics &g)
{
    using namespace juce;
    auto freqs = getFrequencies();

    auto renderArea = getAnalysisArea();
    auto left = float(renderArea.getX());
    auto right = float(renderArea.getRight());
    auto top = float(renderArea.getY());
    auto bottom = float(renderArea.getBottom());
    auto width = float(renderArea.getWidth());

    auto xs = getXs(freqs, left, width);

    g.setColour(Colours::dimgrey);
    for (auto x : xs)
    {
        g.drawVerticalLine(int(x), top, bottom);
    }

    auto gain = getGains();

    for (auto gDb : gain)
    {
        auto y = jmap(gDb, -48.f, 0.f, bottom, top);

        g.setColour(Colours::darkgrey);
        g.drawHorizontalLine(int(y), left, right);
    }
}

void SpectrumAnalyzer::drawTextLabels(juce::Graphics &g)
{
    using namespace juce;

    g.setColour(Colours::lightgrey);
    const int fontHeight = 10;
    g.setFont(fontHeight);

    auto renderArea = getAnalysisArea();
    auto left = renderArea.getX();

    auto top = renderArea.getY();
    auto bottom = renderArea.getBottom();
    auto width = renderArea.getWidth();

    auto freqs = getFrequencies();
    auto xs = getXs(freqs, float(left), float(width));

    for (int i = 0; i < freqs.size(); ++i)
    {
        auto f = freqs[i];
        auto x = xs[i];

        bool addK = false;
        String str;
        if (f > 999.f)
        {
            addK = true;
            f /= 1000.f;
        }

        str << f;
        if (addK)
        {
            str << "k";
        }
        str << "Hz";

        auto textWidth = g.getCurrentFont().getStringWidth(str);

        juce::Rectangle<int> r;

        r.setSize(textWidth, fontHeight);
        r.setCentre(int(x), 0);
        r.setY(1);

        g.drawFittedText(str, r, juce::Justification::centred, 1);
    }

    auto gain = getGains();

    for (auto gDb : gain)
    {
        auto y = jmap(gDb, -48.f, 0.f, float(bottom), float(top));

        String str;
        str << gDb;

        auto textWidth = g.getCurrentFont().getStringWidth(str);

        juce::Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setX(getWidth() - textWidth);
        r.setCentre(r.getCentreX(), int(y));
        g.setColour(Colours::lightgrey);
        g.drawFittedText(str, r, juce::Justification::centredLeft, 1);

        str.clear();
        str << gDb;

        r.setX(1);
        textWidth = g.getCurrentFont().getStringWidth(str);
        r.setSize(textWidth, fontHeight);
        g.setColour(Colours::lightgrey);
        g.drawFittedText(str, r, juce::Justification::centredLeft, 1);
    }
}

void SpectrumAnalyzer::resized()
{
}

void SpectrumAnalyzer::timerCallback()
{
    auto fftBounds = getAnalysisArea().toFloat();
    auto sampleRate = mAudioProcessor.getSampleRate();

    mPathProducer.process(fftBounds, sampleRate);

    repaint();
}

juce::Rectangle<int> SpectrumAnalyzer::getRenderArea()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop(12);
    bounds.removeFromBottom(2);
    bounds.removeFromLeft(20);
    bounds.removeFromRight(20);

    return bounds;
}

juce::Rectangle<int> SpectrumAnalyzer::getAnalysisArea()
{
    auto bounds = getRenderArea();
    bounds.removeFromTop(4);
    bounds.removeFromBottom(4);

    return bounds;
}

void SpectrumAnalyzer::changeFFTFrameSize(int optId)
{
    FFTOrder option = FFTOrder::order2048;

    switch (optId)
    {
    case 0:
        option = FFTOrder::order2048;
        break;
    case 1:
        option = FFTOrder::order4096;
        break;
    case 2:
        option = FFTOrder::order8192;
        break;
    default:
        break;
    }

    mFFTDataGenerator.changeOrder(option);
}
