#include <cmath>
#include "Spectrum.h"
#include "PluginSettings.h"

namespace
{
constexpr float kGridAlpha = 0.18f;
constexpr float kHarmonicAlpha = 0.35f;
constexpr float kPlotDarker = 0.3f;
constexpr float kTraceThickness = 1.5f;
constexpr float kLevelGridStepDb = 20.0f;
constexpr double kFrequencyGridStepHz = 2000.0;
constexpr int kFrequencyLabelEvery = 2;    // a label at every 2nd grid line (4 kHz)
constexpr float kMinHarmonicSpacingPx = 3.0f;
constexpr float kAxisLabelWidth = 50.0f;   // in pixels at the smallest window size
constexpr float kAxisLabelHeight = 14.0f;
constexpr float kTinyPower = 1e-30f;       // avoids log(0)
constexpr double kHzPerKHz = 1000.0;
} // namespace

Spectrum::Spectrum(SpectrumCapture& capture)
: m_capture(capture)
{
    m_history.reserve(static_cast<size_t>(kFftSize + SpectrumCapture::kCapacity));
    m_fftData.assign(static_cast<size_t>(2 * kFftSize), 0.0f);
    m_levelsDb.assign(static_cast<size_t>(kFftSize / 2 + 1), kMinLevelDb);

    // the window's gain: a sine of amplitude 1 gives the peak |X| = amplitude * sum(w) / 2
    std::vector<float> ones(static_cast<size_t>(kFftSize), 1.0f);
    m_window.multiplyWithWindowingTable(ones.data(), ones.size());
    float sum = 0.0f;
    for (const float w : ones)
    {
        sum += w;
    }
    m_windowGain = 0.5f * sum;

    m_showHarmonics.setToggleState(true, juce::dontSendNotification);
    m_showHarmonics.setTooltip("Thin lines at the harmonics of the fundamental: every peak between them is aliasing.");
    m_showHarmonics.onClick = [this]() { repaint(); };
    addAndMakeVisible(m_showHarmonics);

    startTimerHz(kRefreshRateHz);
}

Spectrum::~Spectrum()
{
    stopTimer();
}

void Spectrum::timerCallback()
{
    // read always (also when this page is hidden), so the FIFO does not run full
    m_newSamples += m_capture.pull(m_history);
    if (m_history.size() > static_cast<size_t>(kFftSize))
    {
        m_history.erase(m_history.begin(), m_history.end() - kFftSize);
    }
    if (m_newSamples == 0 || m_history.size() < static_cast<size_t>(kFftSize))
    {
        return; // no note, or not enough samples yet: keep the last spectrum
    }
    m_newSamples = 0;
    m_sampleRate = m_capture.getSampleRate();
    m_fundamental = m_capture.getFundamentalFrequency();
    computeSpectrum();
    if (isShowing())
    {
        repaint();
    }
}

void Spectrum::computeSpectrum()
{
    std::fill(m_fftData.begin(), m_fftData.end(), 0.0f);
    std::copy(m_history.begin(), m_history.end(), m_fftData.begin());
    m_window.multiplyWithWindowingTable(m_fftData.data(), static_cast<size_t>(kFftSize));
    m_fft.performFrequencyOnlyForwardTransform(m_fftData.data());

    const int numBins = kFftSize / 2 + 1;
    const double binWidth = m_sampleRate / kFftSize;
    double totalPower = 0.0;
    double outsidePower = 0.0;
    for (int bin = 0; bin < numBins; ++bin)
    {
        const float magnitude = m_fftData[static_cast<size_t>(bin)] / m_windowGain;
        const float power = magnitude * magnitude;
        m_levelsDb[static_cast<size_t>(bin)] = 10.0f * std::log10(power + kTinyPower);
        totalPower += power;
        // distance to the nearest harmonic (or DC) in bins
        bool isHarmonic = bin <= kHarmonicHalfWidthBins;
        if (m_fundamental > 0.0)
        {
            const double harmonicNumber = std::round(bin * binWidth / m_fundamental);
            const double distanceInBins = std::abs(bin * binWidth - harmonicNumber * m_fundamental) / binWidth;
            if (distanceInBins <= kHarmonicHalfWidthBins)
            {
                isHarmonic = true;
            }
        }
        if (!isHarmonic)
        {
            outsidePower += power;
        }
    }
    // the measurement needs space between the harmonics (f0 >= about 70 Hz at 48 kHz)
    const double harmonicSpacingInBins = m_fundamental / binWidth;
    m_outsideHarmonicsValid = harmonicSpacingInBins > 2.0 * kHarmonicHalfWidthBins + 1.0 && totalPower > 0.0;
    if (m_outsideHarmonicsValid)
    {
        m_outsideHarmonicsDb = static_cast<float>(10.0 * std::log10(outsidePower / totalPower + kTinyPower));
    }
    m_hasSpectrum = true;
}

float Spectrum::scaleFactor() const
{
    return static_cast<float>(getWidth()) / g_minGuiSize_x;
}

juce::Rectangle<float> Spectrum::plotArea() const
{
    const float scale = scaleFactor();
    auto area = getLocalBounds().toFloat();
    area.removeFromTop(g_choiceHeight * scale + g_guiMargin * scale); // the controls
    area = area.reduced(g_guiMargin * scale);
    area.removeFromLeft(kAxisLabelWidth * scale);    // level labels
    area.removeFromBottom(kAxisLabelHeight * scale); // frequency labels
    return area;
}

void Spectrum::resized()
{
    const float scale = scaleFactor();
    auto row = getLocalBounds().removeFromTop(static_cast<int>(g_choiceHeight * scale));
    m_showHarmonics.setBounds(row.removeFromRight(static_cast<int>(g_scopeControlWidth * scale)));
}

float Spectrum::frequencyToX(double frequency, juce::Rectangle<float> area) const
{
    return area.getX() + area.getWidth() * static_cast<float>(frequency / (0.5 * m_sampleRate));
}

float Spectrum::levelToY(float levelDb, juce::Rectangle<float> area) const
{
    const float clipped = juce::jlimit(kMinLevelDb, kMaxLevelDb, levelDb);
    return area.getY() + area.getHeight() * (kMaxLevelDb - clipped) / (kMaxLevelDb - kMinLevelDb);
}

void Spectrum::paint(juce::Graphics& g)
{
    const auto area = plotArea();
    g.setColour(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).darker(kPlotDarker));
    g.fillRect(area);
    paintGrid(g, area);
    if (m_hasSpectrum)
    {
        if (m_showHarmonics.getToggleState())
        {
            paintHarmonics(g, area);
        }
        paintSpectrum(g, area);
    }
    paintInfo(g);
}

void Spectrum::paintGrid(juce::Graphics& g, juce::Rectangle<float> area)
{
    const float scale = scaleFactor();
    const auto textColour = getLookAndFeel().findColour(juce::Label::textColourId);
    g.setFont(g_versionFontSize * scale);
    // levels: every 20 dB
    for (float level = kMaxLevelDb; level >= kMinLevelDb; level -= kLevelGridStepDb)
    {
        const float y = levelToY(level, area);
        g.setColour(textColour.withAlpha(kGridAlpha));
        g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());
        g.setColour(textColour);
        const juce::Rectangle<float> label(area.getX() - kAxisLabelWidth * scale, y - 0.5f * kAxisLabelHeight * scale,
                                           kAxisLabelWidth * scale - g_guiMargin * scale, kAxisLabelHeight * scale);
        g.drawText(juce::String(static_cast<int>(level)) + " dB", label, juce::Justification::centredRight);
    }
    // frequencies: every 2 kHz, labels every 4 kHz
    int line = 0;
    for (double frequency = 0.0; frequency <= 0.5 * m_sampleRate; frequency += kFrequencyGridStepHz)
    {
        const float x = frequencyToX(frequency, area);
        g.setColour(textColour.withAlpha(kGridAlpha));
        g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
        if (line % kFrequencyLabelEvery == 0)
        {
            g.setColour(textColour);
            juce::Rectangle<float> label(x - kAxisLabelWidth * scale, area.getBottom(),
                                         2.0f * kAxisLabelWidth * scale, kAxisLabelHeight * scale);
            // the last label (fs/2) would stick out: right-aligned at the edge of the plot instead
            auto justification = juce::Justification::centred;
            if (label.getRight() > static_cast<float>(getWidth()))
            {
                label = label.withRightX(area.getRight());
                justification = juce::Justification::centredRight;
            }
            g.drawText(juce::String(static_cast<int>(frequency / kHzPerKHz)) + " kHz", label, justification);
        }
        ++line;
    }
}

void Spectrum::paintHarmonics(juce::Graphics& g, juce::Rectangle<float> area)
{
    if (m_fundamental <= 0.0)
    {
        return;
    }
    const float spacing = frequencyToX(m_fundamental, area) - area.getX();
    if (spacing < kMinHarmonicSpacingPx)
    {
        return; // too dense to be useful
    }
    g.setColour(getLookAndFeel().findColour(juce::Label::textColourId).withAlpha(kHarmonicAlpha));
    for (double frequency = m_fundamental; frequency < 0.5 * m_sampleRate; frequency += m_fundamental)
    {
        g.drawVerticalLine(static_cast<int>(frequencyToX(frequency, area)), area.getY(), area.getBottom());
    }
}

void Spectrum::paintSpectrum(juce::Graphics& g, juce::Rectangle<float> area)
{
    // one point per pixel column: the highest bin in the column (peaks are not lost)
    const int numBins = kFftSize / 2 + 1;
    const int width = static_cast<int>(area.getWidth());
    juce::Path path;
    for (int column = 0; column < width; ++column)
    {
        const int firstBin = column * (numBins - 1) / width;
        int lastBin = (column + 1) * (numBins - 1) / width;
        if (lastBin < firstBin)
        {
            lastBin = firstBin;
        }
        float peak = kMinLevelDb;
        for (int bin = firstBin; bin <= lastBin; ++bin)
        {
            peak = std::max(peak, m_levelsDb[static_cast<size_t>(bin)]);
        }
        const float x = area.getX() + static_cast<float>(column);
        const float y = levelToY(peak, area);
        if (column == 0)
        {
            path.startNewSubPath(x, y);
        }
        else
        {
            path.lineTo(x, y);
        }
    }
    g.saveState();
    g.reduceClipRegion(area.toNearestInt());
    g.setColour(getLookAndFeel().findColour(juce::Slider::rotarySliderFillColourId));
    g.strokePath(path, juce::PathStrokeType(kTraceThickness * scaleFactor()));
    g.restoreState();
}

void Spectrum::paintInfo(juce::Graphics& g)
{
    const float scale = scaleFactor();
    g.setColour(getLookAndFeel().findColour(juce::Label::textColourId));
    g.setFont(g_versionFontSize * scale);
    juce::String text("Oscillator before the envelope. Play a note");
    if (m_hasSpectrum)
    {
        text = "Oscillator before the envelope    f0 " + juce::String(m_fundamental, 1) + " Hz    outside the harmonics ";
        if (m_outsideHarmonicsValid)
        {
            text += juce::String(m_outsideHarmonicsDb, 1) + " dB";
        }
        else
        {
            text += "-- (f0 too low for this measurement)";
        }
    }
    const auto infoArea = getLocalBounds().toFloat().removeFromTop(g_choiceHeight * scale)
                              .withTrimmedLeft(g_pageButtonsWidth * scale + g_guiMargin * scale)
                              .withTrimmedRight(g_scopeControlWidth * scale);
    g.drawText(text, infoArea, juce::Justification::centredLeft);
}
