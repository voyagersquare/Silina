#include "PluginEditor.h"

namespace silina
{

namespace colours
{
    const juce::Colour ink        { 0xff1d1a17 };
    const juce::Colour paper      { 0xfff4f0e7 };
    const juce::Colour paperShade { 0xffe9e3d7 };
    const juce::Colour rule       { 0xffbdb6aa };
    const juce::Colour orange     { 0xffee6a22 };
    const juce::Colour orangeDeep { 0xffdc5418 };
    const juce::Colour penBlue    { 0xff22408f };
    const juce::Colour shellLight { 0xffc9bfb4 };
    const juce::Colour shellDark  { 0xffa1958a };
    const juce::Colour shellEdge  { 0xff4f463f };
    const juce::Colour window     { 0xff151210 };
}

namespace
{
    juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    struct KnobSpec { const char* id; const char* label; };
    constexpr KnobSpec knobSpecs[] = {
        { ids::input,    "INPUT" },
        { ids::bias,     "BIAS" },
        { ids::wow,      "WOW" },
        { ids::flutter,  "FLUTTER" },
        { ids::emphasis, "EMPHASIS" },
        { ids::mix,      "MIX" },
        { ids::output,   "OUTPUT" },
    };

    // Layout in cassette-local coordinates (cassette is 1000 x 640).
    constexpr float casW = 1000.0f, casH = 640.0f;
    const juce::Rectangle<float> labelArea   { 56.0f, 34.0f, 888.0f, 448.0f };
    const juce::Rectangle<float> stripeArea  { 56.0f, 256.0f, 888.0f, 150.0f };
    const juce::Rectangle<float> windowArea  { 152.0f, 270.0f, 696.0f, 122.0f };
    const juce::Rectangle<float> glassArea   { 392.0f, 281.0f, 216.0f, 100.0f };
    const juce::Point<float> reelLeft  { 292.0f, 331.0f };
    const juce::Point<float> reelRight { 708.0f, 331.0f };
    const juce::Rectangle<float> arrowArea   { 452.0f, 428.0f, 66.0f, 32.0f };
    constexpr float knobTop = 76.0f, knobHeight = 168.0f, knobLeft = 76.0f, knobWidth = 121.0f;

    juce::Path speedArrow (juce::Rectangle<float> r)
    {
        juce::Path p;
        const float shaft = r.getHeight() * 0.36f;
        const float headX = r.getX() + r.getWidth() * 0.55f;
        p.startNewSubPath (r.getX(), r.getCentreY() - shaft * 0.5f);
        p.lineTo (headX, r.getCentreY() - shaft * 0.5f);
        p.lineTo (headX, r.getY());
        p.lineTo (r.getRight(), r.getCentreY());
        p.lineTo (headX, r.getBottom());
        p.lineTo (headX, r.getCentreY() + shaft * 0.5f);
        p.lineTo (r.getX(), r.getCentreY() + shaft * 0.5f);
        p.closeSubPath();
        return p;
    }
}

//==============================================================================
CassetteLookAndFeel::CassetteLookAndFeel()
{
    setColour (juce::TextButton::buttonColourId, colours::paper);
    setColour (juce::TextButton::buttonOnColourId, colours::ink);
    setColour (juce::TextButton::textColourOffId, colours::ink);
    setColour (juce::TextButton::textColourOnId, colours::paper);
    setColour (juce::ToggleButton::textColourId, colours::ink);
}

void CassetteLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                            float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;
    const auto bounds = Rectangle<int> (x, y, width, height).toFloat();
    const float labelH = 22.0f, valueH = 26.0f;

    // Printed parameter name.
    g.setColour (colours::ink);
    g.setFont (font (14.5f, true));
    g.drawText (slider.getName(), bounds.withHeight (labelH), Justification::centred, false);

    // Value written in pen.
    g.setColour (colours::penBlue);
    g.setFont (font (16.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()),
                bounds.withTop (bounds.getBottom() - valueH), Justification::centred, false);

    const auto knobArea = bounds.withTrimmedTop (labelH).withTrimmedBottom (valueH);
    const float radius = jmin (knobArea.getWidth(), knobArea.getHeight()) * 0.5f - 4.0f;
    const auto c = knobArea.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Value track.
    const float trackR = radius - 3.0f;
    Path track;
    track.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::rule.withAlpha (0.7f));
    g.strokePath (track, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));

    Path value;
    value.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, startAngle, angle, true);
    g.setColour (slider.isEnabled() ? colours::orange : colours::rule);
    g.strokePath (value, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // Dark well around the hub, like the cassette window ring.
    const float wellR = radius * 0.80f;
    g.setColour (Colour (0xff2a2622));
    g.fillEllipse (Rectangle<float> (wellR * 2, wellR * 2).withCentre (c));
    g.setGradientFill (ColourGradient (Colour (0xffdcd8d2), c.x, c.y - wellR, Colour (0xff8f8a84), c.x, c.y + wellR, false));
    g.fillEllipse (Rectangle<float> (wellR * 1.86f, wellR * 1.86f).withCentre (c));

    // White hub with inward teeth, rotating with the value.
    const float hubR = wellR * 0.80f, holeR = hubR * 0.56f;
    g.setGradientFill (ColourGradient (Colours::white, c.x - hubR * 0.4f, c.y - hubR * 0.6f,
                                       Colour (0xffcfcac3), c.x + hubR * 0.6f, c.y + hubR, true));
    g.fillEllipse (Rectangle<float> (hubR * 2, hubR * 2).withCentre (c));
    g.setColour (Colour (0x40000000));
    g.drawEllipse (Rectangle<float> (hubR * 2, hubR * 2).withCentre (c), 1.0f);

    g.setColour (Colour (0xff4a4540));
    g.fillEllipse (Rectangle<float> (holeR * 2, holeR * 2).withCentre (c));

    for (int i = 0; i < 6; ++i)
    {
        Path tooth;
        const float tw = holeR * 0.42f, tl = holeR * 0.42f;
        tooth.addRoundedRectangle (-tw * 0.5f, -holeR - 1.0f, tw, tl, 1.5f);
        tooth.applyTransform (AffineTransform::rotation (angle + (float) i * MathConstants<float>::pi / 3.0f).translated (c));
        g.setColour (Colour (0xffeeebe6));
        g.fillPath (tooth);
    }

    // Orange index mark on the hub rim.
    const auto dir = Point<float> (std::sin (angle), -std::cos (angle));
    g.setColour (colours::orangeDeep);
    g.drawLine (Line<float> (c + dir * (holeR + 3.0f), c + dir * (hubR - 2.0f)), 4.0f);

    if (slider.hasKeyboardFocus (true))
    {
        g.setColour (colours::penBlue.withAlpha (0.5f));
        g.drawEllipse (Rectangle<float> (radius * 2 + 2, radius * 2 + 2).withCentre (c), 1.0f);
    }
}

void CassetteLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                bool highlighted, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = button.getToggleState();
    g.setColour (on ? colours::ink : colours::paper.brighter (highlighted ? 0.05f : 0.0f));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (colours::ink.withAlpha (down ? 1.0f : 0.85f));
    g.drawRoundedRectangle (r, 3.0f, 1.4f);
}

void CassetteLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setColour (button.getToggleState() ? colours::paper : colours::ink);
    g.setFont (font (14.0f, true));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

void CassetteLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    auto r = button.getLocalBounds().toFloat();
    const auto box = r.removeFromLeft (r.getHeight()).reduced (7.0f);
    g.setColour (colours::paper.brighter (highlighted ? 0.05f : 0.0f));
    g.fillRect (box);
    g.setColour (colours::ink);
    g.drawRect (box, 1.4f);

    if (button.getToggleState())
    {
        juce::Path tick;
        tick.startNewSubPath (box.getX() + box.getWidth() * 0.18f, box.getCentreY());
        tick.lineTo (box.getX() + box.getWidth() * 0.42f, box.getBottom() - box.getHeight() * 0.18f);
        tick.lineTo (box.getRight() + box.getWidth() * 0.10f, box.getY() - box.getHeight() * 0.12f);
        g.setColour (colours::penBlue);
        g.strokePath (tick, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (colours::ink);
    g.setFont (font (13.5f, true));
    g.drawText (button.getButtonText(), r.withTrimmedLeft (2.0f), juce::Justification::centredLeft, false);
}

//==============================================================================
CassetteDeck::CassetteDeck (SilinaAudioProcessor& p) : processor (p)
{
    setLookAndFeel (&lookAndFeel);
    cassette = { 20.0f, 18.0f, casW, casH };

    auto& state = processor.getState();
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.paramId = knobSpecs[i].id;
        k.slider.setName (knobSpecs[i].label);
        k.slider.setComponentID (k.paramId);
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.slider.setMouseDragSensitivity (220);
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, k.paramId, k.slider);

        if (auto* param = state.getParameter (k.paramId))
            k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

        k.slider.setTitle (knobSpecs[i].label);
        addAndMakeVisible (k.slider);
    }

    const char* speedNames[] = { "7.5", "15", "30" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = speedButtons[(size_t) i];
        b.setButtonText (speedNames[i]);
        b.setClickingTogglesState (false);
        b.setTitle (juce::String (speedNames[i]) + " IPS");
        b.onClick = [this, i] { setSpeed (i); };
        addAndMakeVisible (b);
    }

    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, ids::autoGain, autoGain);
    autoGain.setTitle ("Auto gain");
    addAndMakeVisible (autoGain);

    refreshSpeedButtons();
    setSize (logicalWidth, logicalHeight);
    startTimerHz (30);
}

CassetteDeck::~CassetteDeck()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::Slider* CassetteDeck::findKnob (const juce::String& paramId) noexcept
{
    for (auto& k : knobs)
        if (k.paramId == paramId)
            return &k.slider;
    return nullptr;
}

void CassetteDeck::resized()
{
    const auto origin = cassette.getPosition();
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i].slider.setBounds (juce::Rectangle<float> (knobLeft + knobWidth * (float) i, knobTop, knobWidth, knobHeight)
                                       .translated (origin.x, origin.y).toNearestInt());

    for (int i = 0; i < 3; ++i)
        speedButtons[(size_t) i].setBounds (juce::Rectangle<float> (176.0f + 50.0f * (float) i, 428.0f, 46.0f, 30.0f)
                                                .translated (origin.x, origin.y).toNearestInt());

    autoGain.setBounds (juce::Rectangle<float> (318.0f, 426.0f, 118.0f, 34.0f).translated (origin.x, origin.y).toNearestInt());
}

void CassetteDeck::setSpeed (int index)
{
    if (auto* param = processor.getState().getParameter (ids::speed))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
        param->endChangeGesture();
    }
    refreshSpeedButtons();
}

void CassetteDeck::refreshSpeedButtons()
{
    const int speed = processor.getSpeedIndex();
    for (int i = 0; i < 3; ++i)
        speedButtons[(size_t) i].setToggleState (i == speed, juce::dontSendNotification);
}

void CassetteDeck::advanceAnimation (float seconds)
{
    const bool playing = processor.getEngine().getOutputPeak() > 1.0e-4f;
    const float follow = 1.0f - std::exp (-seconds / 0.25f);
    activity += ((playing ? 1.0f : 0.0f) - activity) * follow;

    const float speedRates[] = { 1.6f, 3.2f, 6.4f }; // radians per second
    reelAngle += seconds * speedRates[juce::jlimit (0, 2, processor.getSpeedIndex())] * activity;
    if (reelAngle > juce::MathConstants<float>::twoPi)
        reelAngle -= juce::MathConstants<float>::twoPi;

    const float target = processor.getEngine().getSaturationMeter();
    saturation += (target - saturation) * (1.0f - std::exp (-seconds / 0.08f));
}

void CassetteDeck::timerCallback()
{
    const float previousAngle = reelAngle, previousSat = saturation;
    advanceAnimation (1.0f / 30.0f);
    refreshSpeedButtons();

    if (std::abs (reelAngle - previousAngle) > 1.0e-4f)
        repaint (windowArea.translated (cassette.getX(), cassette.getY()).expanded (4).toNearestInt());
    if (std::abs (saturation - previousSat) > 1.0e-3f)
        repaint (arrowArea.translated (cassette.getX(), cassette.getY()).expanded (16).toNearestInt());
}

//==============================================================================
void CassetteDeck::paint (juce::Graphics& g)
{
    using namespace juce;

    // Studio backdrop.
    g.setGradientFill (ColourGradient (Colour (0xfff3f1ed), (float) getWidth() * 0.5f, (float) getHeight() * 0.4f,
                                       Colour (0xffd6d2cb), 0.0f, (float) getHeight(), true));
    g.fillAll();

    Path outline;
    outline.addRoundedRectangle (cassette, 22.0f);
    DropShadow (Colour (0x66000000), 30, { 0, 14 }).drawForPath (g, outline);

    Graphics::ScopedSaveState save (g);
    g.addTransform (AffineTransform::translation (cassette.getX(), cassette.getY()));

    paintShell (g);
    paintLabel (g);
    paintWindow (g);
    paintArrow (g);
    paintBottom (g);
}

void CassetteDeck::paintShell (juce::Graphics& g)
{
    using namespace juce;
    const Rectangle<float> body { 0.0f, 0.0f, casW, casH };
    Path shell;
    shell.addRoundedRectangle (body, 22.0f);

    g.setGradientFill (ColourGradient (colours::shellLight, 0.0f, 0.0f, colours::shellDark, 0.0f, casH, false));
    g.fillPath (shell);

    {
        // Moulded grid texture of the clear shell.
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (shell);
        for (float x = 4.0f; x < casW; x += 7.0f)
        {
            g.setColour (Colour (0x12000000)); g.drawVerticalLine ((int) x, 0.0f, casH);
            g.setColour (Colour (0x10ffffff)); g.drawVerticalLine ((int) x + 1, 0.0f, casH);
        }
        for (float y = 4.0f; y < casH; y += 7.0f)
        {
            g.setColour (Colour (0x12000000)); g.drawHorizontalLine ((int) y, 0.0f, casW);
            g.setColour (Colour (0x10ffffff)); g.drawHorizontalLine ((int) y + 1, 0.0f, casW);
        }

        // Top lip and side rails.
        g.setColour (Colour (0x22ffffff));
        g.fillRect (Rectangle<float> (0.0f, 0.0f, casW, 14.0f));
        g.setColour (Colour (0x30000000));
        g.drawHorizontalLine (14, 0.0f, casW);

        // Dark internals visible through the shell beside the label.
        g.setColour (Colour (0xe0231e1a));
        g.fillRoundedRectangle (Rectangle<float> (30.0f, 250.0f, casW - 60.0f, 166.0f), 8.0f);
        g.setColour (Colour (0x55231e1a));
        g.fillRoundedRectangle (Rectangle<float> (36.0f, 110.0f, 16.0f, 120.0f), 4.0f);
        g.fillRoundedRectangle (Rectangle<float> (casW - 52.0f, 110.0f, 16.0f, 120.0f), 4.0f);

        // Shell reflections.
        g.setGradientFill (ColourGradient (Colour (0x30ffffff), 0.0f, 0.0f, Colour (0x00ffffff), casW * 0.35f, casH * 0.6f, false));
        g.fillPath (shell);
    }

    g.setColour (colours::shellEdge);
    g.strokePath (shell, PathStrokeType (3.0f));
    g.setColour (Colour (0x55ffffff));
    Path inner;
    inner.addRoundedRectangle (body.reduced (4.0f), 19.0f);
    g.strokePath (inner, PathStrokeType (1.2f));

    // Corner screws.
    paintScrew (g, { 30.0f, 30.0f }, 11.0f, 0.3f);
    paintScrew (g, { casW - 30.0f, 30.0f }, 11.0f, 1.1f);
    paintScrew (g, { 30.0f, casH - 30.0f }, 11.0f, 0.7f);
    paintScrew (g, { casW - 30.0f, casH - 30.0f }, 11.0f, 1.4f);
}

void CassetteDeck::paintLabel (juce::Graphics& g)
{
    using namespace juce;

    // Paper label.
    DropShadow (Colour (0x40000000), 6, { 0, 1 }).drawForRectangle (g, labelArea.toNearestInt());
    g.setGradientFill (ColourGradient (colours::paper, 0.0f, labelArea.getY(), colours::paperShade, 0.0f, labelArea.getBottom(), false));
    g.fillRoundedRectangle (labelArea, 5.0f);

    // INDEX header and ruled writing lines.
    g.setColour (Colour (0xff6d6862));
    g.setFont (font (12.5f));
    g.drawText ("I N D E X", Rectangle<float> (80.0f, 42.0f, 200.0f, 18.0f), Justification::centredLeft, false);
    g.setColour (colours::rule);
    g.drawHorizontalLine (64, 76.0f, 924.0f);
    g.drawHorizontalLine (248, 76.0f, 924.0f);
    g.setColour (colours::rule.withAlpha (0.45f));
    for (int i = 1; i < 7; ++i)
        g.drawVerticalLine ((int) (knobLeft + knobWidth * (float) i), 74.0f, 240.0f);

    // Orange stripe.
    g.setGradientFill (ColourGradient (colours::orange.brighter (0.08f), 0.0f, stripeArea.getY(),
                                       colours::orangeDeep, 0.0f, stripeArea.getBottom(), false));
    g.fillRect (stripeArea);
    g.setColour (Colour (0x22000000));
    g.drawHorizontalLine ((int) stripeArea.getY(), stripeArea.getX(), stripeArea.getRight());
    g.drawHorizontalLine ((int) stripeArea.getBottom() - 1, stripeArea.getX(), stripeArea.getRight());

    // Side A.
    g.setColour (colours::ink);
    g.setFont (font (58.0f, true));
    g.drawText ("A", Rectangle<float> (70.0f, 296.0f, 70.0f, 72.0f), Justification::centred, false);

    // Lower strip: speed selector caption, model line and the wordmark.
    g.setFont (font (12.5f, true));
    g.drawText ("TAPE SPEED", Rectangle<float> (80.0f, 428.0f, 92.0f, 30.0f), Justification::centredLeft, false);
    g.setFont (font (11.5f));
    g.drawText ("IPS", Rectangle<float> (176.0f, 458.0f, 146.0f, 16.0f), Justification::centred, false);
    g.drawText ("TYPE S (SATURATION) POSITION", Rectangle<float> (556.0f, 460.0f, 360.0f, 16.0f), Justification::centredRight, false);

    g.setColour (colours::orangeDeep);
    g.setFont (font (50.0f, true));
    g.drawText ("SILINA", Rectangle<float> (600.0f, 410.0f, 316.0f, 54.0f), Justification::centredRight, false);
}

void CassetteDeck::paintScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    using namespace juce;
    g.setGradientFill (ColourGradient (Colour (0xff5a554f), c.x - r * 0.5f, c.y - r * 0.6f, Colour (0xff0f0d0c), c.x + r, c.y + r, true));
    g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c));
    g.setColour (Colour (0xff050404));
    g.drawEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c), 1.2f);

    Path cross;
    cross.addRectangle (-r * 0.62f, -r * 0.13f, r * 1.24f, r * 0.26f);
    cross.addRectangle (-r * 0.13f, -r * 0.62f, r * 0.26f, r * 1.24f);
    cross.applyTransform (AffineTransform::rotation (angle).translated (c));
    g.setColour (Colour (0xff000000));
    g.fillPath (cross);
    g.setColour (Colour (0x30ffffff));
    g.drawEllipse (Rectangle<float> (r * 1.4f, r * 1.4f).withCentre (c.translated (-0.6f, -0.6f)), 0.8f);
}

void CassetteDeck::paintHub (juce::Graphics& g, juce::Point<float> c, float radius, float angle)
{
    using namespace juce;

    // Clear window ring around the hub.
    g.setColour (Colour (0xff35302b));
    g.fillEllipse (Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c));
    g.setGradientFill (ColourGradient (Colour (0xffe4e1dc), c.x, c.y - radius, Colour (0xff9a958f), c.x, c.y + radius, false));
    g.fillEllipse (Rectangle<float> (radius * 1.84f, radius * 1.84f).withCentre (c));

    // White hub.
    const float hubR = radius * 0.74f, holeR = hubR * 0.58f;
    g.setGradientFill (ColourGradient (Colours::white, c.x - hubR * 0.4f, c.y - hubR * 0.5f,
                                       Colour (0xffd2cdc6), c.x + hubR * 0.6f, c.y + hubR, true));
    g.fillEllipse (Rectangle<float> (hubR * 2.0f, hubR * 2.0f).withCentre (c));
    g.setColour (Colour (0x50000000));
    g.drawEllipse (Rectangle<float> (hubR * 2.0f, hubR * 2.0f).withCentre (c), 1.0f);

    g.setColour (Colour (0xff5b5650));
    g.fillEllipse (Rectangle<float> (holeR * 2.0f, holeR * 2.0f).withCentre (c));

    for (int i = 0; i < 6; ++i)
    {
        Path tooth;
        const float tw = holeR * 0.36f, tl = holeR * 0.46f;
        tooth.addRoundedRectangle (-tw * 0.5f, -holeR - 1.0f, tw, tl, 1.5f);
        tooth.applyTransform (AffineTransform::rotation (angle + (float) i * MathConstants<float>::pi / 3.0f).translated (c));
        g.setColour (Colour (0xfff2efea));
        g.fillPath (tooth);
    }

    // Moulded spokes on the white hub (make rotation visible).
    for (int i = 0; i < 3; ++i)
    {
        const float a = angle + MathConstants<float>::pi / 6.0f + (float) i * MathConstants<float>::twoPi / 3.0f;
        const Point<float> dir (std::sin (a), -std::cos (a));
        g.setColour (Colour (0x26000000));
        g.drawLine (Line<float> (c + dir * (holeR + 3.0f), c + dir * (hubR - 3.0f)), 2.0f);
    }
}

void CassetteDeck::paintWindow (juce::Graphics& g)
{
    using namespace juce;

    // Black printed window surround.
    g.setGradientFill (ColourGradient (Colour (0xff221d1a), 0.0f, windowArea.getY(), colours::window, 0.0f, windowArea.getBottom(), false));
    g.fillRoundedRectangle (windowArea, 12.0f);
    g.setColour (Colour (0x40ffffff));
    g.drawRoundedRectangle (windowArea.reduced (1.0f), 12.0f, 1.0f);

    // Centre glass with the tape packs behind it.
    {
        Graphics::ScopedSaveState s (g);
        Path glass;
        glass.addRoundedRectangle (glassArea, 5.0f);
        g.reduceClipRegion (glass);

        g.setGradientFill (ColourGradient (Colour (0xffa59d95), 0.0f, glassArea.getY(), Colour (0xff6d665f), 0.0f, glassArea.getBottom(), false));
        g.fillRect (glassArea);

        const float leftPack = 226.0f, rightPack = 172.0f;
        g.setColour (Colour (0xff2b211b));
        g.fillEllipse (Rectangle<float> (leftPack * 2, leftPack * 2).withCentre (reelLeft));
        g.fillEllipse (Rectangle<float> (rightPack * 2, rightPack * 2).withCentre (reelRight));
        g.setColour (Colour (0x40ffffff));
        g.drawEllipse (Rectangle<float> (leftPack * 2, leftPack * 2).withCentre (reelLeft), 1.0f);
        g.drawEllipse (Rectangle<float> (rightPack * 2, rightPack * 2).withCentre (reelRight), 1.0f);

        // Printed length scale.
        g.setColour (Colour (0x90ffffff));
        g.drawHorizontalLine ((int) reelLeft.y, glassArea.getX() + 8.0f, glassArea.getRight() - 8.0f);
        for (float x = glassArea.getX() + 12.0f; x < glassArea.getRight() - 8.0f; x += 8.0f)
            g.drawVerticalLine ((int) x, reelLeft.y - 4.0f, reelLeft.y + 4.0f);

        // Glass sheen.
        g.setGradientFill (ColourGradient (Colour (0x38ffffff), glassArea.getX(), glassArea.getY(),
                                           Colour (0x00ffffff), glassArea.getCentreX(), glassArea.getBottom(), false));
        g.fillRect (glassArea);
    }
    g.setColour (Colour (0xff0b0a09));
    g.drawRoundedRectangle (glassArea, 5.0f, 2.0f);

    paintHub (g, reelLeft, 52.0f, reelAngle);
    paintHub (g, reelRight, 52.0f, reelAngle + 0.4f);
}

void CassetteDeck::paintArrow (juce::Graphics& g)
{
    using namespace juce;
    const auto arrow = speedArrow (arrowArea);

    if (saturation > 0.01f)
    {
        const auto glow = arrowArea.expanded (10.0f + 8.0f * saturation, 10.0f);
        const auto hot = Colour (0xffff6a10).withAlpha (jlimit (0.0f, 0.6f, saturation * 0.65f));
        g.setGradientFill (ColourGradient (hot, glow.getCentreX(), glow.getCentreY(),
                                           hot.withAlpha (0.0f), glow.getRight(), glow.getCentreY(), true));
        g.fillEllipse (glow);
    }

    g.setColour (colours::orange.interpolatedWith (Colour (0xffd21f0e), jlimit (0.0f, 1.0f, saturation)));
    g.fillPath (arrow);
}

void CassetteDeck::paintBottom (juce::Graphics& g)
{
    using namespace juce;

    // Raised head-access section.
    Path trapezoid;
    trapezoid.startNewSubPath (205.0f, 494.0f);
    trapezoid.lineTo (795.0f, 494.0f);
    trapezoid.lineTo (835.0f, casH - 2.0f);
    trapezoid.lineTo (165.0f, casH - 2.0f);
    trapezoid.closeSubPath();

    g.setGradientFill (ColourGradient (Colour (0xffb3a79b), 0.0f, 494.0f, Colour (0xff8a7d72), 0.0f, casH, false));
    g.fillPath (trapezoid);
    g.setColour (colours::shellEdge);
    g.strokePath (trapezoid, PathStrokeType (2.0f));
    g.setColour (Colour (0x50ffffff));
    g.drawLine (208.0f, 497.0f, 792.0f, 497.0f, 1.2f);

    // Guide rollers at the bottom corners.
    for (auto c : { Point<float> (96.0f, 576.0f), Point<float> (casW - 96.0f, 576.0f) })
    {
        g.setColour (Colour (0xffe6dcc4));
        g.fillEllipse (Rectangle<float> (52.0f, 52.0f).withCentre (c));
        g.setColour (Colour (0xffc9b994));
        g.drawEllipse (Rectangle<float> (40.0f, 40.0f).withCentre (c), 2.0f);
        g.setColour (Colour (0xfff5efe2));
        g.fillEllipse (Rectangle<float> (22.0f, 22.0f).withCentre (c));
        g.setColour (Colour (0x60000000));
        g.drawEllipse (Rectangle<float> (52.0f, 52.0f).withCentre (c), 1.2f);
    }

    // Openings in the head section.
    auto hole = [&g] (Rectangle<float> r, float corner)
    {
        g.setColour (Colour (0xff2b2622));
        g.fillRoundedRectangle (r, corner);
        g.setColour (Colour (0x50ffffff));
        g.drawRoundedRectangle (r.translated (0.0f, 1.0f), corner, 1.0f);
    };
    hole ({ 236.0f, 584.0f, 36.0f, 30.0f }, 3.0f);
    hole ({ casW - 272.0f, 584.0f, 36.0f, 30.0f }, 3.0f);
    hole ({ 316.0f, 572.0f, 30.0f, 30.0f }, 15.0f);
    hole ({ casW - 346.0f, 572.0f, 30.0f, 30.0f }, 15.0f);
    hole ({ 404.0f, 588.0f, 34.0f, 26.0f }, 3.0f);
    hole ({ casW - 438.0f, 588.0f, 34.0f, 26.0f }, 3.0f);
    hole ({ 484.0f, 590.0f, 32.0f, 24.0f }, 3.0f);

    paintScrew (g, { casW * 0.5f, 530.0f }, 12.0f, 0.9f);
}

} // namespace silina

//==============================================================================
SilinaAudioProcessorEditor::SilinaAudioProcessorEditor (SilinaAudioProcessor& p)
    : AudioProcessorEditor (p), deck (p)
{
    addAndMakeVisible (deck);

    const int w = silina::CassetteDeck::logicalWidth, h = silina::CassetteDeck::logicalHeight;
    setResizable (true, true);
    setResizeLimits (w * 6 / 10, h * 6 / 10, w * 16 / 10, h * 16 / 10);
    if (auto* aspect = getConstrainer())
        aspect->setFixedAspectRatio ((double) w / (double) h);
    setSize (w * 9 / 10, h * 9 / 10);
}

void SilinaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xffd6d2cb));
}

void SilinaAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) silina::CassetteDeck::logicalWidth;
    deck.setBounds (0, 0, silina::CassetteDeck::logicalWidth, silina::CassetteDeck::logicalHeight);
    deck.setTransform (juce::AffineTransform::scale (scale));
}
