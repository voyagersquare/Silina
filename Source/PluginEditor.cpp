#include "PluginEditor.h"
#include "SilaFonts.h"

namespace sila
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
    const juce::Colour pencil     { 0xff4a4744 };
    const juce::Colour shellLight { 0xffc9bfb4 };
    const juce::Colour shellDark  { 0xffa1958a };
    const juce::Colour shellEdge  { 0xff4f463f };
    const juce::Colour window     { 0xff151210 };
    const juce::Colour tape       { 0xff3a2a20 };
}

juce::Font handFont (float height)
{
    static const juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor (
        SilaFonts::CoveredByYourGrace_ttf, (size_t) SilaFonts::CoveredByYourGrace_ttfSize);
    return juce::Font (juce::FontOptions (typeface).withHeight (height));
}

namespace
{
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

    // Cassette-local coordinates (the cassette is 1000 x 640).
    constexpr float casW = 1000.0f, casH = 640.0f;
    const juce::Rectangle<float> labelArea   { 56.0f, 34.0f, 888.0f, 448.0f };
    const juce::Rectangle<float> stripeArea  { 56.0f, 256.0f, 888.0f, 150.0f };
    const juce::Rectangle<float> windowArea  { 152.0f, 270.0f, 696.0f, 122.0f };
    const juce::Rectangle<float> glassArea   { 392.0f, 281.0f, 216.0f, 100.0f };
    const juce::Point<float> reelLeft  { 292.0f, 331.0f };
    const juce::Point<float> reelRight { 708.0f, 331.0f };
    constexpr float wellRadius = 52.0f, hubRadius = 38.0f, fullPackRadius = 178.0f;
    constexpr float knobTop = 72.0f, knobHeight = 176.0f, knobLeft = 76.0f, knobWidth = 121.0f;
    constexpr float dialSweep = juce::MathConstants<float>::pi * 5.0f / 3.0f; // 300 degrees

    //==========================================================================
    // Procedural wooden desk: horizontal planks with warped grain and fibres.
    float hash (int x, int y) noexcept
    {
        auto h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) ((h ^ (h >> 16)) & 0xffffffu) / 16777215.0f;
    }

    float valueNoise (float x, float y) noexcept
    {
        const float fx = std::floor (x), fy = std::floor (y);
        const int xi = (int) fx, yi = (int) fy;
        float tx = x - fx, ty = y - fy;
        tx = tx * tx * (3.0f - 2.0f * tx);
        ty = ty * ty * (3.0f - 2.0f * ty);
        const float a = hash (xi, yi), b = hash (xi + 1, yi), c = hash (xi, yi + 1), d = hash (xi + 1, yi + 1);
        return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty;
    }

    float fbm (float x, float y, int octaves) noexcept
    {
        float sum = 0.0f, amp = 0.5f, norm = 0.0f;
        for (int i = 0; i < octaves; ++i)
        {
            sum += amp * valueNoise (x, y);
            norm += amp;
            x *= 2.03f; y *= 2.03f; amp *= 0.5f;
        }
        return sum / norm;
    }

    juce::Image makeWoodDesk (int width, int height)
    {
        juce::Image image (juce::Image::RGB, width, height, false);
        juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);

        constexpr int plankHeight = 205, plankOffset = 60;
        const float dark[3]  = { 84.0f, 46.0f, 24.0f };
        const float light[3] = { 172.0f, 112.0f, 64.0f };

        for (int y = 0; y < height; ++y)
        {
            const int py = y + plankOffset;
            const int plank = py / plankHeight;
            const int ly = py % plankHeight;
            const float tint = 0.88f + 0.22f * hash (plank, 17);
            const int joint = (int) (hash (plank, 3) * (float) width * 1.4f) - width / 5;

            for (int x = 0; x < width; ++x)
            {
                const int segment = x < joint ? 0 : 1; // planks are butted end to end
                const float fx = (float) x, fly = (float) ly;
                const float seed = (float) (plank * 2 + segment) * 13.7f;

                const float warp = fbm (fx * 0.0028f + seed, fly * 0.012f + seed, 3);
                const float grainCoord = fly * 0.55f + warp * 60.0f + fbm (fx * 0.0012f + seed, seed, 2) * 40.0f;
                float rings = 0.5f + 0.5f * std::sin (grainCoord * 0.40f);
                rings = rings * rings * rings;
                const float fibres = fbm (fx * 0.0045f + seed, (float) y * 0.55f, 2);

                float t = 0.42f * rings + 0.40f * fibres + 0.25f * warp - 0.05f;
                t = juce::jlimit (0.0f, 1.0f, t);

                float shade = tint;
                if (ly < 2) shade *= 0.35f;             // seam between planks
                else if (ly < 5) shade *= 0.80f;        // bevel
                else if (ly > plankHeight - 3) shade *= 1.10f;
                if (std::abs (x - joint) < 2) shade *= 0.45f;

                // Desk lamp from above, darker towards the corners.
                const float nx = fx / (float) width - 0.5f, ny = (float) y / (float) height - 0.38f;
                shade *= 1.10f - 0.55f * (nx * nx + ny * ny * 1.3f);

                const auto channel = [&] (int i)
                {
                    return (juce::uint8) juce::jlimit (0.0f, 255.0f, (dark[i] + (light[i] - dark[i]) * t) * shade);
                };
                data.setPixelColour (x, y, juce::Colour (channel (0), channel (1), channel (2)));
            }
        }
        return image;
    }

    // A sheet of ruled paper with hand-written notes.
    void drawNotePaper (juce::Graphics& g, juce::Rectangle<float> sheet, float rotation, juce::Colour paperColour,
                        const juce::StringArray& lines, bool ruled, juce::Colour inkColour, float textSize)
    {
        using namespace juce;
        Graphics::ScopedSaveState s (g);
        g.addTransform (AffineTransform::rotation (rotation, sheet.getCentreX(), sheet.getCentreY()));

        Path outline;
        outline.addRectangle (sheet);
        DropShadow (Colour (0x55000000), 14, { 3, 6 }).drawForPath (g, outline);

        g.setGradientFill (ColourGradient (paperColour.brighter (0.04f), sheet.getX(), sheet.getY(),
                                           paperColour.darker (0.06f), sheet.getRight(), sheet.getBottom(), false));
        g.fillRect (sheet);

        if (ruled)
        {
            g.setColour (Colour (0x5579a6d2));
            for (float y = sheet.getY() + 54.0f; y < sheet.getBottom() - 10.0f; y += 30.0f)
                g.drawHorizontalLine ((int) y, sheet.getX(), sheet.getRight());
            g.setColour (Colour (0x66d66a6a));
            g.drawVerticalLine ((int) (sheet.getX() + 40.0f), sheet.getY(), sheet.getBottom());
        }

        g.setColour (inkColour);
        g.setFont (handFont (textSize));
        float y = sheet.getY() + (ruled ? 26.0f : 18.0f);
        for (auto& line : lines)
        {
            g.drawText (line, Rectangle<float> (sheet.getX() + (ruled ? 48.0f : 16.0f), y, sheet.getWidth() - 56.0f, 30.0f),
                        Justification::centredLeft, false);
            y += 30.0f;
        }
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

void CassetteLookAndFeel::drawDialKnob (juce::Graphics& g, juce::Point<float> c, float radius, float pos)
{
    using namespace juce;
    const auto disc = [c] (float r) { return Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };

    // Soft shadow on the label.
    Path body;
    body.addEllipse (disc (radius));
    DropShadow (Colour (0x70000000), 12, { 2, 6 }).drawForPath (g, body);

    // Black bakelite skirt.
    g.setGradientFill (ColourGradient (Colour (0xff3a3835), c.x - radius * 0.3f, c.y - radius * 0.4f,
                                       Colour (0xff0b0a0a), c.x + radius, c.y + radius, true));
    g.fillEllipse (disc (radius));

    // Numbered scale rotates with the knob; the current value sits under the fixed index mark.
    const float offset = -pos * dialSweep;
    for (int t = 0; t <= 50; ++t)
    {
        const float a = offset + dialSweep * (float) t / 50.0f;
        const bool major = t % 5 == 0;
        const Point<float> dir (std::sin (a), -std::cos (a));
        g.setColour (Colour (0xffeeeae2).withAlpha (major ? 0.95f : 0.75f));
        g.drawLine (Line<float> (c + dir * (radius - 3.0f), c + dir * (radius - (major ? 11.0f : 7.0f))), major ? 2.2f : 1.2f);
    }

    g.setFont (handFont (radius * 0.44f));
    for (int k = 0; k <= 10; ++k)
    {
        const float a = offset + dialSweep * (float) k / 10.0f;
        Graphics::ScopedSaveState s (g);
        g.addTransform (AffineTransform::rotation (a, c.x, c.y));
        g.setColour (Colour (0xfff2eee6));
        g.drawText (String (k), Rectangle<float> (radius * 0.7f, radius * 0.42f).withCentre ({ c.x, c.y - radius * 0.64f }),
                    Justification::centred, false);
    }

    // Plastic gloss on the skirt.
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (body);
        g.setGradientFill (ColourGradient (Colour (0x40ffffff), c.x - radius * 0.35f, c.y - radius * 0.75f,
                                           Colour (0x00ffffff), c.x, c.y, true));
        g.fillEllipse (Rectangle<float> (radius * 1.5f, radius * 0.9f).withCentre ({ c.x - radius * 0.2f, c.y - radius * 0.5f }));
    }
    g.setColour (Colour (0x30ffffff));
    g.drawEllipse (disc (radius - 0.8f), 1.2f);

    // Fluted centre cap.
    const float capR = radius * 0.40f;
    Path cap;
    constexpr int flutes = 28;
    for (int i = 0; i <= 180; ++i)
    {
        const float a = MathConstants<float>::twoPi * (float) i / 180.0f + offset;
        const float r = capR + 1.3f * std::cos (a * (float) flutes);
        const Point<float> p (c.x + r * std::sin (a), c.y - r * std::cos (a));
        if (i == 0) cap.startNewSubPath (p); else cap.lineTo (p);
    }
    cap.closeSubPath();
    DropShadow (Colour (0x90000000), 6, { 1, 3 }).drawForPath (g, cap);
    g.setGradientFill (ColourGradient (Colour (0xff45423f), c.x - capR * 0.6f, c.y - capR * 0.7f,
                                       Colour (0xff080707), c.x + capR, c.y + capR, true));
    g.fillPath (cap);

    const float faceR = capR * 0.80f;
    g.setGradientFill (ColourGradient (Colour (0xff2a2826), c.x, c.y - faceR, Colour (0xff111010), c.x, c.y + faceR, false));
    g.fillEllipse (disc (faceR));
    g.setGradientFill (ColourGradient (Colour (0x55ffffff), c.x - faceR * 0.4f, c.y - faceR * 0.6f,
                                       Colour (0x00ffffff), c.x, c.y, true));
    g.fillEllipse (Rectangle<float> (faceR * 1.2f, faceR * 0.7f).withCentre ({ c.x - faceR * 0.2f, c.y - faceR * 0.4f }));
}

void CassetteLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                            float, float, juce::Slider& slider)
{
    using namespace juce;
    const auto bounds = Rectangle<int> (x, y, width, height).toFloat();
    const float labelH = 30.0f, valueH = 30.0f;

    g.setColour (colours::ink);
    g.setFont (handFont (25.0f));
    g.drawText (slider.getName(), bounds.withHeight (labelH), Justification::centred, false);

    g.setColour (colours::penBlue);
    g.setFont (handFont (24.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()),
                bounds.withTop (bounds.getBottom() - valueH), Justification::centred, false);

    const auto knobArea = bounds.withTrimmedTop (labelH).withTrimmedBottom (valueH);
    const float radius = jmin (knobArea.getWidth(), knobArea.getHeight()) * 0.5f - 8.0f;
    const auto c = knobArea.getCentre().translated (0.0f, 2.0f);

    // Fixed index mark printed on the label above the dial.
    g.setColour (colours::ink);
    g.drawLine (c.x, c.y - radius - 9.0f, c.x, c.y - radius - 2.0f, 2.4f);

    drawDialKnob (g, c, radius, pos);

    if (slider.hasKeyboardFocus (true))
    {
        g.setColour (colours::penBlue.withAlpha (0.5f));
        g.drawEllipse (Rectangle<float> (radius * 2 + 6, radius * 2 + 6).withCentre (c), 1.0f);
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
    g.setFont (handFont (22.0f));
    g.drawText (button.getButtonText(), button.getLocalBounds().translated (0, 1), juce::Justification::centred, false);
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
    g.setFont (handFont (22.0f));
    g.drawText (button.getButtonText(), r.withTrimmedLeft (2.0f).translated (0.0f, 1.0f), juce::Justification::centredLeft, false);
}

//==============================================================================
CassetteDeck::CassetteDeck (SilaAudioProcessor& p) : processor (p)
{
    setLookAndFeel (&lookAndFeel);
    cassette = { 200.0f, 86.0f, casW, casH };
    setOpaque (true);

    // The desk texture is generated once and shared by every editor instance.
    static juce::Image sharedDesk;
    if (! sharedDesk.isValid())
        sharedDesk = makeWoodDesk (logicalWidth, logicalHeight);
    desk = sharedDesk;

    auto& state = processor.getState();
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.paramId = knobSpecs[i].id;
        k.slider.setName (knobSpecs[i].label);
        k.slider.setComponentID (k.paramId);
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
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
        speedButtons[(size_t) i].setBounds (juce::Rectangle<float> (196.0f + 52.0f * (float) i, 426.0f, 48.0f, 32.0f)
                                                .translated (origin.x, origin.y).toNearestInt());

    autoGain.setBounds (juce::Rectangle<float> (356.0f, 424.0f, 150.0f, 36.0f).translated (origin.x, origin.y).toNearestInt());
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

float CassetteDeck::packRadius (bool left) const noexcept
{
    // Tape area is conserved: r^2 = hub^2 + share * (full^2 - hub^2).
    const float share = left ? 1.0f - tapePosition : tapePosition;
    return std::sqrt (hubRadius * hubRadius + share * (fullPackRadius * fullPackRadius - hubRadius * hubRadius));
}

void CassetteDeck::advanceAnimation (float seconds)
{
    // Constant tape speed past the heads: the smaller pack spins faster, like in a real deck.
    const float tapeSpeeds[] = { 140.0f, 260.0f, 480.0f }; // display units per second
    const float v = tapeSpeeds[juce::jlimit (0, 2, processor.getSpeedIndex())];

    reelAngleLeft  = std::fmod (reelAngleLeft  + seconds * v / packRadius (true),  juce::MathConstants<float>::twoPi);
    reelAngleRight = std::fmod (reelAngleRight + seconds * v / packRadius (false), juce::MathConstants<float>::twoPi);

    // The take-up reel slowly grows (one side lasts about half an hour).
    tapePosition += seconds / 1800.0f * (v / 260.0f);
    if (tapePosition > 0.97f)
        tapePosition = 0.03f;
}

void CassetteDeck::timerCallback()
{
    advanceAnimation (1.0f / 30.0f);
    refreshSpeedButtons();
    repaint (windowArea.translated (cassette.getX(), cassette.getY()).expanded (4).toNearestInt());
}

//==============================================================================
void CassetteDeck::paint (juce::Graphics& g)
{
    using namespace juce;

    paintDesk (g);
    paintPapers (g);

    // The cassette lies on the desk.
    Path outline;
    outline.addRoundedRectangle (cassette, 22.0f);
    DropShadow (Colour (0x80000000), 34, { 10, 18 }).drawForPath (g, outline);
    DropShadow (Colour (0x60000000), 6, { 2, 3 }).drawForPath (g, outline);

    Graphics::ScopedSaveState save (g);
    g.addTransform (AffineTransform::translation (cassette.getX(), cassette.getY()));

    paintShell (g);
    paintLabel (g);
    paintWindow (g);
    paintBottom (g);
}

void CassetteDeck::paintDesk (juce::Graphics& g)
{
    g.drawImageAt (desk, 0, 0);
}

void CassetteDeck::paintPapers (juce::Graphics& g)
{
    using namespace juce;
    const Colour paperWhite (0xfff3efe4), paperOld (0xffe8dfc9), sticky (0xfff3d36b);

    // Left: a couple of session sheets, partly under the cassette.
    drawNotePaper (g, { -60.0f, 170.0f, 330.0f, 440.0f }, -0.16f, paperOld, {}, true, colours::pencil, 26.0f);
    drawNotePaper (g, { -30.0f, 120.0f, 320.0f, 430.0f }, -0.07f, paperWhite,
                   { "", "Session - mix bus", "vox: +3 dB in", "tape 15 IPS", "bias ~50 %", "wow - just a bit!", "flutter 15 %" },
                   true, colours::pencil, 26.0f);

    // Right: track list and a sticky note.
    drawNotePaper (g, { 1190.0f, 50.0f, 300.0f, 400.0f }, 0.12f, paperWhite,
                   { "", "SIDE A", "1. Intro", "2. Nocna voznja", "3. Kasetofon", "4. Sila (dub)", "" },
                   true, colours::penBlue, 26.0f);
    drawNotePaper (g, { 1200.0f, 560.0f, 190.0f, 180.0f }, -0.10f, sticky,
                   { "mix petak!", "", "ne zaboravi", "master -1 dB" }, false, colours::ink, 26.0f);

    // A small paper at the bottom left.
    drawNotePaper (g, { 30.0f, 660.0f, 240.0f, 200.0f }, 0.09f, paperWhite,
                   { "", "Trake:", "C60 x 3" }, true, colours::pencil, 24.0f);
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

        g.setColour (Colour (0x22ffffff));
        g.fillRect (Rectangle<float> (0.0f, 0.0f, casW, 14.0f));
        g.setColour (Colour (0x30000000));
        g.drawHorizontalLine (14, 0.0f, casW);

        g.setColour (Colour (0xe0231e1a));
        g.fillRoundedRectangle (Rectangle<float> (30.0f, 250.0f, casW - 60.0f, 166.0f), 8.0f);
        g.setColour (Colour (0x55231e1a));
        g.fillRoundedRectangle (Rectangle<float> (36.0f, 110.0f, 16.0f, 120.0f), 4.0f);
        g.fillRoundedRectangle (Rectangle<float> (casW - 52.0f, 110.0f, 16.0f, 120.0f), 4.0f);

        g.setGradientFill (ColourGradient (Colour (0x30ffffff), 0.0f, 0.0f, Colour (0x00ffffff), casW * 0.35f, casH * 0.6f, false));
        g.fillPath (shell);
    }

    g.setColour (colours::shellEdge);
    g.strokePath (shell, PathStrokeType (3.0f));
    g.setColour (Colour (0x55ffffff));
    Path inner;
    inner.addRoundedRectangle (body.reduced (4.0f), 19.0f);
    g.strokePath (inner, PathStrokeType (1.2f));

    paintScrew (g, { 30.0f, 30.0f }, 11.0f, 0.3f);
    paintScrew (g, { casW - 30.0f, 30.0f }, 11.0f, 1.1f);
    paintScrew (g, { 30.0f, casH - 30.0f }, 11.0f, 0.7f);
    paintScrew (g, { casW - 30.0f, casH - 30.0f }, 11.0f, 1.4f);
}

void CassetteDeck::paintLabel (juce::Graphics& g)
{
    using namespace juce;

    DropShadow (Colour (0x40000000), 6, { 0, 1 }).drawForRectangle (g, labelArea.toNearestInt());
    g.setGradientFill (ColourGradient (colours::paper, 0.0f, labelArea.getY(), colours::paperShade, 0.0f, labelArea.getBottom(), false));
    g.fillRoundedRectangle (labelArea, 5.0f);

    g.setColour (Colour (0xff6d6862));
    g.setFont (handFont (20.0f));
    g.drawText ("INDEX", Rectangle<float> (80.0f, 38.0f, 200.0f, 24.0f), Justification::centredLeft, false);
    g.setColour (colours::rule);
    g.drawHorizontalLine (64, 76.0f, 924.0f);
    g.drawHorizontalLine (250, 76.0f, 924.0f);
    g.setColour (colours::rule.withAlpha (0.45f));
    for (int i = 1; i < 7; ++i)
        g.drawVerticalLine ((int) (knobLeft + knobWidth * (float) i), 72.0f, 244.0f);

    g.setGradientFill (ColourGradient (colours::orange.brighter (0.08f), 0.0f, stripeArea.getY(),
                                       colours::orangeDeep, 0.0f, stripeArea.getBottom(), false));
    g.fillRect (stripeArea);
    g.setColour (Colour (0x22000000));
    g.drawHorizontalLine ((int) stripeArea.getY(), stripeArea.getX(), stripeArea.getRight());
    g.drawHorizontalLine ((int) stripeArea.getBottom() - 1, stripeArea.getX(), stripeArea.getRight());

    g.setColour (colours::ink);
    g.setFont (handFont (84.0f));
    g.drawText ("A", Rectangle<float> (66.0f, 290.0f, 80.0f, 84.0f), Justification::centred, false);

    g.setFont (handFont (22.0f));
    g.drawText ("TAPE SPEED", Rectangle<float> (80.0f, 426.0f, 116.0f, 32.0f), Justification::centredLeft, false);
    g.setFont (handFont (18.0f));
    g.drawText ("IPS", Rectangle<float> (196.0f, 457.0f, 152.0f, 20.0f), Justification::centred, false);
    g.drawText ("TYPE S (SATURATION) POSITION", Rectangle<float> (556.0f, 455.0f, 362.0f, 22.0f), Justification::centredRight, false);

    g.setColour (colours::orangeDeep);
    g.setFont (handFont (78.0f));
    g.drawText ("SILA", Rectangle<float> (620.0f, 404.0f, 300.0f, 60.0f), Justification::centredRight, false);
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

void CassetteDeck::paintTapePack (juce::Graphics& g, juce::Point<float> c, float radius, float angle)
{
    using namespace juce;
    const auto disc = Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);

    g.setGradientFill (ColourGradient (colours::tape.brighter (0.25f), c.x, c.y - radius,
                                       colours::tape.darker (0.35f), c.x, c.y + radius, false));
    g.fillEllipse (disc);

    // Winding rings.
    for (float r = hubRadius + 4.0f; r < radius; r += 5.0f)
    {
        g.setColour (Colour (0xff000000).withAlpha (0.10f + 0.10f * hash ((int) r, 5)));
        g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 0.8f);
    }

    // Uneven sheen on the pack that turns with the reel, so the spinning is visible.
    for (int i = 0; i < 5; ++i)
    {
        const float a = angle + (float) i * 1.31f;
        Path wedge;
        wedge.addPieSegment (disc, a, a + 0.18f + 0.06f * (float) i, (hubRadius + 2.0f) / radius);
        g.setColour (Colour (0xffffffff).withAlpha (0.035f + 0.012f * (float) i));
        g.fillPath (wedge);
    }

    g.setColour (Colour (0x50ffffff));
    g.drawEllipse (disc, 1.0f);
}

void CassetteDeck::paintHub (juce::Graphics& g, juce::Point<float> c, float radius, float angle)
{
    using namespace juce;
    const float hubR = radius, holeR = hubR * 0.58f;

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

    for (int i = 0; i < 3; ++i)
    {
        const float a = angle + MathConstants<float>::pi / 6.0f + (float) i * MathConstants<float>::twoPi / 3.0f;
        const Point<float> dir (std::sin (a), -std::cos (a));
        g.setColour (Colour (0x30000000));
        g.drawLine (Line<float> (c + dir * (holeR + 3.0f), c + dir * (hubR - 3.0f)), 2.0f);
    }
}

void CassetteDeck::paintWindow (juce::Graphics& g)
{
    using namespace juce;

    g.setGradientFill (ColourGradient (Colour (0xff221d1a), 0.0f, windowArea.getY(), colours::window, 0.0f, windowArea.getBottom(), false));
    g.fillRoundedRectangle (windowArea, 12.0f);
    g.setColour (Colour (0x40ffffff));
    g.drawRoundedRectangle (windowArea.reduced (1.0f), 12.0f, 1.0f);

    // Everything seen through the clear parts: the centre glass and the two hub wells.
    Path clear;
    clear.addRoundedRectangle (glassArea, 5.0f);
    clear.addEllipse (Rectangle<float> (wellRadius * 1.84f, wellRadius * 1.84f).withCentre (reelLeft));
    clear.addEllipse (Rectangle<float> (wellRadius * 1.84f, wellRadius * 1.84f).withCentre (reelRight));

    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (clear);

        g.setGradientFill (ColourGradient (Colour (0xffb3aca4), 0.0f, glassArea.getY(), Colour (0xff76706a), 0.0f, glassArea.getBottom(), false));
        g.fillRect (windowArea);

        paintTapePack (g, reelLeft, packRadius (true), reelAngleLeft);
        paintTapePack (g, reelRight, packRadius (false), reelAngleRight);

        // Printed length scale on the glass.
        g.setColour (Colour (0x90ffffff));
        g.drawHorizontalLine ((int) reelLeft.y, glassArea.getX() + 8.0f, glassArea.getRight() - 8.0f);
        for (float x = glassArea.getX() + 12.0f; x < glassArea.getRight() - 8.0f; x += 8.0f)
            g.drawVerticalLine ((int) x, reelLeft.y - 4.0f, reelLeft.y + 4.0f);

        g.setGradientFill (ColourGradient (Colour (0x38ffffff), glassArea.getX(), glassArea.getY(),
                                           Colour (0x00ffffff), glassArea.getCentreX(), glassArea.getBottom(), false));
        g.fillRect (windowArea);
    }

    g.setColour (Colour (0xff0b0a09));
    g.drawRoundedRectangle (glassArea, 5.0f, 2.0f);
    for (auto c : { reelLeft, reelRight })
    {
        g.setColour (Colour (0xff35302b));
        g.drawEllipse (Rectangle<float> (wellRadius * 1.9f, wellRadius * 1.9f).withCentre (c), 4.0f);
    }

    paintHub (g, reelLeft, hubRadius, reelAngleLeft);
    paintHub (g, reelRight, hubRadius, reelAngleRight);
}

void CassetteDeck::paintBottom (juce::Graphics& g)
{
    using namespace juce;

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

} // namespace sila

//==============================================================================
SilaAudioProcessorEditor::SilaAudioProcessorEditor (SilaAudioProcessor& p)
    : AudioProcessorEditor (p), deck (p)
{
    addAndMakeVisible (deck);

    const int w = sila::CassetteDeck::logicalWidth, h = sila::CassetteDeck::logicalHeight;
    setResizable (true, true);
    setResizeLimits (w / 2, h / 2, w * 3 / 2, h * 3 / 2);
    if (auto* aspect = getConstrainer())
        aspect->setFixedAspectRatio ((double) w / (double) h);
    setSize (w * 8 / 10, h * 8 / 10);
}

void SilaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff5a3a22));
}

void SilaAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) sila::CassetteDeck::logicalWidth;
    deck.setBounds (0, 0, sila::CassetteDeck::logicalWidth, sila::CassetteDeck::logicalHeight);
    deck.setTransform (juce::AffineTransform::scale (scale));
}
