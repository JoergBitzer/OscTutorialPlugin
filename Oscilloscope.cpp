#include "Oscilloscope.h"
#include "PluginSettings.h"
#include "VerySimpleSynth.h"
#include "oscillators/Oscillator.h"

namespace
{
constexpr int kOneCycleId = 1; // combo box ids (the number of periods)
constexpr int kTwoCyclesId = 2;
constexpr int kIdealPointsPerCycle = 2000;
constexpr float kQuartersPerCycle = 4.0f;
constexpr float kTraceThickness = 2.0f;
constexpr float kIdealThickness = 1.0f;
constexpr float kDotDiameter = 5.0f;
constexpr float kGridAlpha = 0.18f;
constexpr float kAxisAlpha = 0.4f;
constexpr float kIdealAlpha = 0.55f;
constexpr float kDashLength = 4.0f;
constexpr float kPlotDarker = 0.3f; // the plot area is a bit darker than the window
} // namespace

Oscilloscope::Oscilloscope(ScopeCapture& capture, juce::AudioProcessorValueTreeState& apvts)
: m_capture(capture), m_apvts(apvts)
{
    m_title.setText("Oscilloscope (oscillator before the envelope)", juce::dontSendNotification);
    addAndMakeVisible(m_title);

    m_cycles.addItem("1 period", kOneCycleId);
    m_cycles.addItem("2 periods", kTwoCyclesId);
    m_cycles.setSelectedId(kTwoCyclesId, juce::dontSendNotification);
    m_cycles.onChange = [this]() { m_capture.setCycles(m_cycles.getSelectedId()); };
    m_cycles.setTooltip("Number of periods shown. The recording starts at the beginning of a period (phase 0).");
    addAndMakeVisible(m_cycles);
    m_capture.setCycles(m_cycles.getSelectedId());

    m_showIdeal.setToggleState(true, juce::dontSendNotification);
    m_showIdeal.setTooltip("The ideal (continuous) waveform as a thin line, to compare with the samples of the model.");
    m_showIdeal.onClick = [this]() { repaint(); };
    addAndMakeVisible(m_showIdeal);

    m_showSamples.setToggleState(true, juce::dontSendNotification);
    m_showSamples.setTooltip("Dots at the samples (if they are far enough apart, i.e. for high notes).");
    m_showSamples.onClick = [this]() { repaint(); };
    addAndMakeVisible(m_showSamples);

    startTimerHz(kRefreshRateHz);
}

Oscilloscope::~Oscilloscope()
{
    stopTimer();
}

void Oscilloscope::timerCallback()
{
    if (m_capture.copyLatest(m_frame))
    {
        m_hasFrame = true;
        repaint();
    }
}

float Oscilloscope::scaleFactor() const
{
    // the oscilloscope is as wide as the plugin window
    return static_cast<float>(getWidth()) / g_minGuiSize_x;
}

juce::Rectangle<float> Oscilloscope::plotArea() const
{
    const float scale = scaleFactor();
    auto area = getLocalBounds().toFloat();
    area.removeFromTop(g_choiceHeight * scale + g_guiMargin * scale); // the controls
    return area.reduced(g_guiMargin * scale);
}

void Oscilloscope::resized()
{
    const float scale = scaleFactor();
    auto row = getLocalBounds().removeFromTop(static_cast<int>(g_choiceHeight * scale));
    row.removeFromLeft(static_cast<int>(g_guiMargin * scale));
    const int controlWidth = static_cast<int>(g_scopeControlWidth * scale);
    m_showSamples.setBounds(row.removeFromRight(controlWidth));
    m_showIdeal.setBounds(row.removeFromRight(controlWidth));
    m_cycles.setBounds(row.removeFromRight(controlWidth).reduced(static_cast<int>(g_guiMargin * scale), 0));
    m_title.setFont(juce::FontOptions(g_labelFontSize * scale));
    m_title.setBounds(row);
}

void Oscilloscope::paint(juce::Graphics& g)
{
    const auto area = plotArea();
    g.setColour(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).darker(kPlotDarker));
    g.fillRect(area);

    int cycles = m_cycles.getSelectedId();
    if (m_hasFrame)
    {
        cycles = m_frame.cycles;
    }
    paintGrid(g, area, cycles);
    if (m_showIdeal.getToggleState())
    {
        paintIdeal(g, area, cycles);
    }
    if (m_hasFrame)
    {
        paintTrace(g, area);
    }
    paintInfo(g, area);
}

void Oscilloscope::paintGrid(juce::Graphics& g, juce::Rectangle<float> area, int cycles)
{
    const auto textColour = getLookAndFeel().findColour(juce::Label::textColourId);
    const float centreY = area.getCentreY();
    const float unitHeight = 0.5f * area.getHeight() / kValueRange; // pixels for the value 1

    // horizontal: 0 (axis), +-1 (dashed: the ideal peak), +-0.5
    g.setColour(textColour.withAlpha(kAxisAlpha));
    g.drawHorizontalLine(static_cast<int>(centreY), area.getX(), area.getRight());
    const float dashes[] = { kDashLength, kDashLength };
    for (const float value : { 1.0f, -1.0f })
    {
        const float y = centreY - value * unitHeight;
        g.drawDashedLine(juce::Line<float>(area.getX(), y, area.getRight(), y), dashes, 2);
    }
    g.setColour(textColour.withAlpha(kGridAlpha));
    for (const float value : { 0.5f, -0.5f })
    {
        g.drawHorizontalLine(static_cast<int>(centreY - value * unitHeight), area.getX(), area.getRight());
    }
    // vertical: quarter periods (thin), periods (stronger)
    const int numQuarters = static_cast<int>(kQuartersPerCycle) * cycles;
    for (int quarter = 1; quarter < numQuarters; ++quarter)
    {
        const float x = area.getX() + area.getWidth() * quarter / numQuarters;
        float alpha = kGridAlpha;
        if (quarter % static_cast<int>(kQuartersPerCycle) == 0)
        {
            alpha = kAxisAlpha;
        }
        g.setColour(textColour.withAlpha(alpha));
        g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
    }
}

void Oscilloscope::paintIdeal(juce::Graphics& g, juce::Rectangle<float> area, int cycles)
{
    const auto waveform = static_cast<vss::Waveform>(
        juce::roundToInt(m_apvts.getRawParameterValue(g_paramWaveform.ID)->load()));
    const float centreY = area.getCentreY();
    const float unitHeight = 0.5f * area.getHeight() / kValueRange;
    const int numPoints = kIdealPointsPerCycle * cycles;
    juce::Path path;
    for (int i = 0; i <= numPoints; ++i)
    {
        const double position = static_cast<double>(cycles) * i / numPoints; // in periods
        const float x = area.getX() + area.getWidth() * static_cast<float>(position / cycles);
        const float y = centreY - static_cast<float>(vss::naiveWaveform(waveform, position)) * unitHeight;
        if (i == 0)
        {
            path.startNewSubPath(x, y);
        }
        else
        {
            path.lineTo(x, y);
        }
    }
    g.setColour(getLookAndFeel().findColour(juce::Label::textColourId).withAlpha(kIdealAlpha));
    g.strokePath(path, juce::PathStrokeType(kIdealThickness * scaleFactor()));
}

void Oscilloscope::paintTrace(juce::Graphics& g, juce::Rectangle<float> area)
{
    const float centreY = area.getCentreY();
    const float unitHeight = 0.5f * area.getHeight() / kValueRange;
    const float cycles = static_cast<float>(m_frame.cycles);
    const auto toPoint = [&](int i)
    {
        const auto index = static_cast<size_t>(i);
        const float x = area.getX() + area.getWidth() * m_frame.x[index] / cycles;
        const float value = juce::jlimit(-kValueRange, kValueRange, m_frame.y[index]);
        return juce::Point<float>(x, centreY - value * unitHeight);
    };

    juce::Path path;
    for (int i = 0; i < m_frame.numPoints; ++i)
    {
        if (i == 0)
        {
            path.startNewSubPath(toPoint(i));
        }
        else
        {
            path.lineTo(toPoint(i));
        }
    }
    const auto traceColour = getLookAndFeel().findColour(juce::Slider::rotarySliderFillColourId);
    g.saveState();
    g.reduceClipRegion(area.toNearestInt());
    g.setColour(traceColour);
    g.strokePath(path, juce::PathStrokeType(kTraceThickness * scaleFactor()));

    const float pixelsPerSample = area.getWidth() / (cycles * static_cast<float>(m_frame.samplesPerCycle));
    if (m_showSamples.getToggleState() && pixelsPerSample >= kMinDotSpacing)
    {
        const float diameter = kDotDiameter * scaleFactor();
        for (int i = 0; i < m_frame.numPoints; ++i)
        {
            const auto point = toPoint(i);
            g.fillEllipse(point.x - 0.5f * diameter, point.y - 0.5f * diameter, diameter, diameter);
        }
    }
    g.restoreState();
}

void Oscilloscope::paintInfo(juce::Graphics& g, juce::Rectangle<float> area)
{
    const float scale = scaleFactor();
    g.setColour(getLookAndFeel().findColour(juce::Label::textColourId));
    g.setFont(g_versionFontSize * scale);
    juce::String text("Play a note");
    if (m_hasFrame && m_frame.numPoints > 0)
    {
        const auto [minimum, maximum] = std::minmax_element(m_frame.y.begin(), m_frame.y.end());
        text = juce::String(m_frame.samplesPerCycle, 1) + " samples per period    max " + juce::String(*maximum, 3)
             + "    min " + juce::String(*minimum, 3);
    }
    g.drawText(text, area.reduced(g_guiMargin * scale), juce::Justification::topLeft);
}
