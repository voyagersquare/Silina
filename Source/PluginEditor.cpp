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

    //==========================================================================
    // Ageing helpers: liquid stain rings, scratches.

    // An irregular dried-liquid ring: faint tinted interior and a darker rim.
    void drawStain (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour colour, int seed, float strength = 1.0f)
    {
        using namespace juce;
        Path rim;
        const float wobblePhase = hash (seed, 1) * 10.0f;
        for (int i = 0; i <= 96; ++i)
        {
            const float a = MathConstants<float>::twoPi * (float) i / 96.0f;
            const float r = radius * (0.93f + 0.10f * valueNoise (a * 2.2f + wobblePhase, (float) seed));
            const Point<float> p (c.x + r * std::cos (a), c.y + r * 0.96f * std::sin (a));
            if (i == 0) rim.startNewSubPath (p); else rim.lineTo (p);
        }
        rim.closeSubPath();

        g.setColour (colour.withAlpha (0.10f * strength));
        g.fillPath (rim);
        g.setColour (colour.withAlpha (0.28f * strength));
        g.strokePath (rim, PathStrokeType (radius * 0.07f));
        g.setColour (colour.withAlpha (0.40f * strength));
        g.strokePath (rim, PathStrokeType (1.3f));

        // Partial second ring where the glass was put down again.
        Path second;
        const float start = hash (seed, 2) * MathConstants<float>::twoPi;
        const Point<float> c2 = c.translated (radius * 0.12f, -radius * 0.08f);
        second.addCentredArc (c2.x, c2.y, radius * 0.97f, radius * 0.95f, 0.0f, start, start + 3.6f, true);
        g.setColour (colour.withAlpha (0.22f * strength));
        g.strokePath (second, PathStrokeType (1.6f));
    }

    // Juice drops and a small splash.
    void drawSplash (juce::Graphics& g, juce::Point<float> c, float size, juce::Colour colour, int seed)
    {
        juce::Random r (seed);
        for (int i = 0; i < 9; ++i)
        {
            const float d = size * (0.2f + 1.4f * r.nextFloat());
            const float a = r.nextFloat() * juce::MathConstants<float>::twoPi;
            const float rr = size * (0.06f + 0.22f * r.nextFloat()) * (i == 0 ? 2.6f : 1.0f);
            const auto p = i == 0 ? c : c.translated (d * std::cos (a), d * std::sin (a));
            g.setColour (colour.withAlpha (0.16f + 0.12f * r.nextFloat()));
            g.fillEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 1.8f).withCentre (p));
            g.setColour (colour.withAlpha (0.30f));
            g.drawEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 1.8f).withCentre (p), 1.0f);
        }
    }

    void drawScratches (juce::Graphics& g, juce::Random& r, juce::Rectangle<float> area, int count,
                        float minLength, float maxLength, float angleCentre, float angleSpread,
                        juce::Colour light, juce::Colour dark, float alphaScale)
    {
        using namespace juce;
        for (int i = 0; i < count; ++i)
        {
            const Point<float> p (area.getX() + r.nextFloat() * area.getWidth(), area.getY() + r.nextFloat() * area.getHeight());
            const float len = minLength + (maxLength - minLength) * r.nextFloat() * r.nextFloat();
            const float a = angleCentre + (r.nextFloat() * 2.0f - 1.0f) * angleSpread;
            const Point<float> dir (std::cos (a), std::sin (a)), normal (-dir.y, dir.x);
            const float bend = (r.nextFloat() - 0.5f) * len * 0.25f;

            Path scratch;
            scratch.startNewSubPath (p);
            scratch.quadraticTo (p + dir * (len * 0.5f) + normal * bend, p + dir * len);
            const float width = 0.5f + 0.8f * r.nextFloat();
            const float alpha = (0.10f + 0.22f * r.nextFloat()) * alphaScale;

            g.setColour (dark.withAlpha (alpha * 0.6f));
            g.strokePath (scratch, PathStrokeType (width + 0.4f), AffineTransform::translation (0.6f, 0.8f));
            g.setColour (light.withAlpha (alpha));
            g.strokePath (scratch, PathStrokeType (width));
        }
    }

    // Juice stains, scratches and dents on the old desk.
    void ageDesk (juce::Image& image)
    {
        using namespace juce;
        Graphics g (image);
        const Colour juice (0xff5c1c0c), cola (0xff2a140a);

        drawStain (g, { 108.0f, 58.0f }, 46.0f, juice, 11, 2.0f);
        drawStain (g, { 1318.0f, 498.0f }, 40.0f, cola, 12, 1.8f);
        drawStain (g, { 640.0f, 790.0f }, 52.0f, juice, 13, 1.8f);
        drawStain (g, { 1250.0f, 790.0f }, 34.0f, juice, 14, 1.6f);
        drawStain (g, { 120.0f, 630.0f }, 44.0f, cola, 15, 1.8f);
        drawSplash (g, { 160.0f, 760.0f }, 26.0f, juice, 21);
        drawSplash (g, { 1350.0f, 30.0f }, 22.0f, juice, 22);
        drawSplash (g, { 420.0f, 40.0f }, 18.0f, cola, 23);

        Random r (4242);
        const Rectangle<float> all (0.0f, 0.0f, (float) image.getWidth(), (float) image.getHeight());
        drawScratches (g, r, all, 700, 12.0f, 240.0f, 0.0f, 0.35f, Colour (0xffe6c39a), Colour (0xff1a0d05), 1.7f);
        drawScratches (g, r, all, 160, 8.0f, 110.0f, 0.0f, 3.14f, Colour (0xffe6c39a), Colour (0xff1a0d05), 1.5f);

        for (int i = 0; i < 140; ++i)
        {
            const Point<float> p (r.nextFloat() * all.getWidth(), r.nextFloat() * all.getHeight());
            const float d = 1.0f + 2.5f * r.nextFloat();
            g.setColour (Colour (0xff140a04).withAlpha (0.25f + 0.2f * r.nextFloat()));
            g.fillEllipse (Rectangle<float> (d, d * 0.8f).withCentre (p));
        }

        // Worn varnish where hands rest: duller, lighter patches.
        for (auto c : { Point<float> (700.0f, 790.0f), Point<float> (90.0f, 420.0f) })
        {
            g.setGradientFill (ColourGradient (Colour (0x22e8c9a0), c, Colour (0x00e8c9a0), c.translated (220.0f, 0.0f), true));
            g.fillEllipse (Rectangle<float> (440.0f, 260.0f).withCentre (c));
        }
    }

    // Grime, yellowing, rubbed paint, stains and scratches of a much-played cassette.
    juce::Image makeCassetteWear()
    {
        using namespace juce;
        const int w = (int) casW, h = (int) casH;
        Image image (Image::ARGB, w, h, true);

        {
            Image::BitmapData data (image, Image::BitmapData::readWrite);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const float fx = (float) x, fy = (float) y;
                    const float coarse = fbm (fx * 0.018f, fy * 0.018f, 3);
                    const float fine = fbm (fx * 0.09f, fy * 0.09f, 3);

                    if (labelArea.contains (fx, fy))
                    {
                        // Yellowed, handled paper; darker towards its edges.
                        const float edge = jmin (fx - labelArea.getX(), labelArea.getRight() - fx,
                                                 fy - labelArea.getY(), labelArea.getBottom() - fy);
                        const float edgeWear = jlimit (0.0f, 1.0f, 1.0f - edge / 26.0f);
                        float alpha = 0.16f + 0.30f * coarse + 0.45f * edgeWear * (0.6f + 0.4f * fine);
                        Colour c (0xff9a6a1c);

                        // Orange print rubbed off along the stripe edges and at random spots.
                        if (stripeArea.contains (fx, fy) && ! windowArea.expanded (4.0f).contains (fx, fy))
                        {
                            const float rub = coarse * 0.7f + fine * 0.5f + edgeWear * 0.5f;
                            if (rub > 0.70f)
                            {
                                c = Colour (0xfff1e2c8);
                                alpha = jlimit (0.0f, 0.85f, (rub - 0.70f) * 4.0f);
                            }
                        }
                        data.setPixelColour (x, y, c.withAlpha (alpha));
                    }
                    else
                    {
                        // Dirt collected in the moulding, worst near the edges and corners.
                        const float edge = jmin (fx, fy, (float) w - fx, (float) h - fy);
                        const float edgeDirt = jlimit (0.0f, 1.0f, 1.0f - edge / 60.0f);
                        float alpha = 0.08f + 0.20f * coarse + 0.40f * edgeDirt * edgeDirt + 0.10f * fine;
                        Colour c (0xff2e2216);

                        // Abraded, whitish plastic right on the outer rim.
                        if (edge < 6.0f && fine > 0.55f)
                        {
                            c = Colour (0xffe9e2d6);
                            alpha = 0.25f * (fine - 0.55f) / 0.45f + 0.08f;
                        }
                        data.setPixelColour (x, y, c.withAlpha (jlimit (0.0f, 0.6f, alpha)));
                    }
                }
        }

        Graphics g (image);

        // Coffee/juice rings on the label.
        drawStain (g, { 112.0f, 70.0f }, 40.0f, Colour (0xff6b3a12), 31, 1.6f);
        drawStain (g, { 904.0f, 462.0f }, 30.0f, Colour (0xff6b3a12), 32, 1.4f);
        drawSplash (g, { 610.0f, 470.0f }, 12.0f, Colour (0xff6b3a12), 33);

        // Crease across the label corner and greasy fingerprints.
        g.setColour (Colour (0x30000000));
        g.drawLine (60.0f, 410.0f, 160.0f, 480.0f, 1.2f);
        g.setColour (Colour (0x50ffffff));
        g.drawLine (61.0f, 409.0f, 161.0f, 479.0f, 1.0f);
        for (auto p : { Point<float> (860.0f, 300.0f), Point<float> (180.0f, 300.0f), Point<float> (500.0f, 560.0f) })
        {
            for (float r = 4.0f; r < 22.0f; r += 2.6f)
            {
                g.setColour (Colour (0x10000000));
                g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.6f).withCentre (p), 0.8f);
            }
        }

        // Torn and lifted label corners.
        for (auto corner : { Point<float> (labelArea.getX(), labelArea.getY()), Point<float> (labelArea.getRight(), labelArea.getBottom()),
                             Point<float> (labelArea.getRight(), labelArea.getY()) })
        {
            Path tear;
            const float sx = corner.x < casW * 0.5f ? 1.0f : -1.0f, sy = corner.y < casH * 0.5f ? 1.0f : -1.0f;
            tear.startNewSubPath (corner);
            tear.lineTo (corner.translated (sx * 22.0f, 0.0f));
            tear.lineTo (corner.translated (sx * 12.0f, sy * 6.0f));
            tear.lineTo (corner.translated (sx * 7.0f, sy * 14.0f));
            tear.lineTo (corner.translated (0.0f, sy * 19.0f));
            tear.closeSubPath();
            g.setColour (Colour (0xffb5aa9c));
            g.fillPath (tear);
            g.setColour (Colour (0x60000000));
            g.strokePath (tear, PathStrokeType (0.8f));
        }

        // Scratches: many on the plastic, a few scuffs over the label and the window.
        Random r (777);
        const Colour light (0xfff6f1ea), dark (0xff1a1410);
        {
            Graphics::ScopedSaveState s (g);
            g.excludeClipRegion (labelArea.toNearestInt());
            drawScratches (g, r, { 0.0f, 0.0f, casW, casH }, 420, 6.0f, 160.0f, 0.0f, 3.14f, light, dark, 2.0f);
            drawScratches (g, r, { 165.0f, 494.0f, 670.0f, 146.0f }, 200, 4.0f, 50.0f, 0.0f, 0.4f, light, dark, 2.0f);
        }
        drawScratches (g, r, windowArea, 70, 6.0f, 110.0f, 0.0f, 3.14f, light, dark, 1.6f);
        drawScratches (g, r, labelArea, 110, 6.0f, 80.0f, 0.0f, 3.14f, Colour (0xffffffff), Colour (0xff5a4a3a), 1.1f);

        return image;
    }

    // A sheet of old, handled paper with hand-written notes.
    void drawNotePaper (juce::Graphics& g, juce::Rectangle<float> sheet, float rotation, juce::Colour paperColour,
                        const juce::StringArray& lines, bool ruled, juce::Colour inkColour, float textSize, int seed)
    {
        using namespace juce;
        Graphics::ScopedSaveState s (g);
        g.addTransform (AffineTransform::rotation (rotation, sheet.getCentreX(), sheet.getCentreY()));

        // Outline with one dog-eared corner folded over.
        const float ear = 26.0f + 16.0f * hash (seed, 7);
        const bool earTop = hash (seed, 8) > 0.5f;
        Path outline;
        if (earTop)
        {
            outline.startNewSubPath (sheet.getTopLeft());
            outline.lineTo (sheet.getRight() - ear, sheet.getY());
            outline.lineTo (sheet.getRight(), sheet.getY() + ear);
            outline.lineTo (sheet.getBottomRight());
            outline.lineTo (sheet.getBottomLeft());
        }
        else
        {
            outline.startNewSubPath (sheet.getTopLeft());
            outline.lineTo (sheet.getTopRight());
            outline.lineTo (sheet.getRight(), sheet.getBottom() - ear);
            outline.lineTo (sheet.getRight() - ear, sheet.getBottom());
            outline.lineTo (sheet.getBottomLeft());
        }
        outline.closeSubPath();
        DropShadow (Colour (0x66000000), 14, { 6, 8 }).drawForPath (g, outline);

        Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (outline);

        g.setGradientFill (ColourGradient (paperColour.brighter (0.03f), sheet.getX(), sheet.getY(),
                                           paperColour.darker (0.10f), sheet.getRight(), sheet.getBottom(), false));
        g.fillRect (sheet);

        // Yellowed, dirty edges.
        const Colour aged (0xff7a5a28);
        const float band = 34.0f;
        g.setGradientFill (ColourGradient (aged.withAlpha (0.28f), sheet.getX(), 0.0f, aged.withAlpha (0.0f), sheet.getX() + band, 0.0f, false));
        g.fillRect (sheet);
        g.setGradientFill (ColourGradient (aged.withAlpha (0.28f), sheet.getRight(), 0.0f, aged.withAlpha (0.0f), sheet.getRight() - band, 0.0f, false));
        g.fillRect (sheet);
        g.setGradientFill (ColourGradient (aged.withAlpha (0.24f), 0.0f, sheet.getY(), aged.withAlpha (0.0f), 0.0f, sheet.getY() + band, false));
        g.fillRect (sheet);
        g.setGradientFill (ColourGradient (aged.withAlpha (0.24f), 0.0f, sheet.getBottom(), aged.withAlpha (0.0f), 0.0f, sheet.getBottom() - band, false));
        g.fillRect (sheet);

        // Blotchy foxing.
        Random r (seed);
        for (int i = 0; i < 26; ++i)
        {
            const Point<float> p (sheet.getX() + r.nextFloat() * sheet.getWidth(), sheet.getY() + r.nextFloat() * sheet.getHeight());
            const float d = 3.0f + 26.0f * r.nextFloat() * r.nextFloat();
            g.setColour (aged.withAlpha (0.05f + 0.08f * r.nextFloat()));
            g.fillEllipse (Rectangle<float> (d, d * (0.7f + 0.5f * r.nextFloat())).withCentre (p));
        }

        if (ruled)
        {
            g.setColour (Colour (0x4a79a6d2));
            for (float y = sheet.getY() + 54.0f; y < sheet.getBottom() - 10.0f; y += 30.0f)
                g.drawHorizontalLine ((int) y, sheet.getX(), sheet.getRight());
            g.setColour (Colour (0x55d66a6a));
            g.drawVerticalLine ((int) (sheet.getX() + 40.0f), sheet.getY(), sheet.getBottom());
        }

        g.setColour (inkColour.withAlpha (0.82f));
        g.setFont (handFont (textSize));
        float y = sheet.getY() + (ruled ? 26.0f : 18.0f);
        for (auto& line : lines)
        {
            g.drawText (line, Rectangle<float> (sheet.getX() + (ruled ? 48.0f : 16.0f), y, sheet.getWidth() - 56.0f, 30.0f),
                        Justification::centredLeft, false);
            y += 30.0f;
        }

        // Stain on the sheet, and a fold line from being kept in a pocket.
        drawStain (g, { sheet.getX() + sheet.getWidth() * (0.3f + 0.5f * hash (seed, 3)),
                        sheet.getY() + sheet.getHeight() * (0.25f + 0.6f * hash (seed, 4)) },
                   22.0f + 18.0f * hash (seed, 5), Colour (0xff6b3a12), seed, 0.9f);

        const float foldY = sheet.getY() + sheet.getHeight() * (0.45f + 0.15f * hash (seed, 6));
        g.setColour (Colour (0x26000000));
        g.drawLine (sheet.getX(), foldY, sheet.getRight(), foldY + 3.0f, 1.4f);
        g.setColour (Colour (0x40ffffff));
        g.drawLine (sheet.getX(), foldY + 1.5f, sheet.getRight(), foldY + 4.5f, 1.2f);

        // Folded-over corner (back of the paper, shaded by the lamp).
        Path flap;
        if (earTop)
        {
            flap.startNewSubPath (sheet.getRight() - ear, sheet.getY());
            flap.lineTo (sheet.getRight(), sheet.getY() + ear);
            flap.lineTo (sheet.getRight() - ear, sheet.getY() + ear);
        }
        else
        {
            flap.startNewSubPath (sheet.getRight(), sheet.getBottom() - ear);
            flap.lineTo (sheet.getRight() - ear, sheet.getBottom());
            flap.lineTo (sheet.getRight() - ear, sheet.getBottom() - ear);
        }
        flap.closeSubPath();
        DropShadow (Colour (0x50000000), 5, { 2, 2 }).drawForPath (g, flap);
        g.setColour (paperColour.darker (0.18f));
        g.fillPath (flap);
        g.setColour (Colour (0x30000000));
        g.strokePath (flap, PathStrokeType (0.8f));
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

void CassetteLookAndFeel::drawDialKnob (juce::Graphics& g, juce::Point<float> c, float radius, float pos, int wearSeed)
{
    using namespace juce;
    const auto disc = [c] (float r) { return Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };

    // Soft shadow on the label.
    Path body;
    body.addEllipse (disc (radius));
    DropShadow (Colour (0x80000000), 12, { 6, 8 }).drawForPath (g, body);

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
        // Paint worn out of some engraved numbers.
        g.setColour (Colour (0xffe8e0cf).withAlpha (0.55f + 0.45f * hash (wearSeed, k)));
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

    // Fine scratches in the bakelite that turn with the knob, and a rim polished by fingers.
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (body);
        g.addTransform (AffineTransform::rotation (offset, c.x, c.y));
        Random r (wearSeed);
        drawScratches (g, r, disc (radius * 0.9f), 7, 3.0f, radius * 0.5f, 0.0f, 3.14f,
                       Colour (0xffffffff), Colour (0xff000000), 0.6f);
    }
    Path polished;
    polished.addCentredArc (c.x, c.y, radius - 1.5f, radius - 1.5f, 0.0f, -2.2f, -0.2f, true);
    g.setColour (Colour (0x28ffffff));
    g.strokePath (polished, PathStrokeType (2.0f));

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
    DropShadow (Colour (0x90000000), 6, { 3, 4 }).drawForPath (g, cap);
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

    drawDialKnob (g, c, radius, pos, slider.getName().hashCode());

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
    {
        sharedDesk = makeWoodDesk (logicalWidth, logicalHeight);
        ageDesk (sharedDesk);
    }
    desk = sharedDesk;

    static juce::Image sharedWear;
    if (! sharedWear.isValid())
        sharedWear = makeCassetteWear();
    wear = sharedWear;

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
    DropShadow (Colour (0x90000000), 40, { 22, 26 }).drawForPath (g, outline);
    DropShadow (Colour (0x70000000), 8, { 5, 6 }).drawForPath (g, outline);

    Graphics::ScopedSaveState save (g);
    g.addTransform (AffineTransform::translation (cassette.getX(), cassette.getY()));

    paintShell (g);
    paintLabel (g);
    paintWindow (g);
    paintBottom (g);
    g.drawImageAt (wear, 0, 0);
}

void CassetteDeck::paintOverChildren (juce::Graphics& g)
{
    using namespace juce;
    const auto bounds = getLocalBounds().toFloat();

    // Desk lamp above the upper left, like a reading lamp at night: a bright, warm pool of
    // light on the cassette and a fast fall-off into a dark room, where the desk and the
    // papers are only just visible.
    const auto light = cassette.getCentre().translated (-25.0f, -35.0f);
    const float stretch = 1.2f; // the pool is wider than it is tall

    Graphics::ScopedSaveState s (g);
    g.addTransform (AffineTransform::scale (stretch, 1.0f, light.x, light.y));
    const auto area = bounds.transformedBy (AffineTransform::scale (1.0f / stretch, 1.0f, light.x, light.y));

    ColourGradient glow (Colour (0x3cfff1d6), light, Colour (0x00fff1d6), light.translated (430.0f, 0.0f), true);
    glow.addColour (0.5, Colour (0x1cfff1d6));
    g.setGradientFill (glow);
    g.fillRect (area);

    ColourGradient falloff (Colour (0x00000000), light, Colour (0xee000000), light.translated (1000.0f, 0.0f), true);
    falloff.addColour (0.25, Colour (0x00000000));
    falloff.addColour (0.37, Colour (0x58000000));
    falloff.addColour (0.50, Colour (0xb0000000));
    falloff.addColour (0.68, Colour (0xdc000000));
    g.setGradientFill (falloff);
    g.fillRect (area);
}

void CassetteDeck::paintDesk (juce::Graphics& g)
{
    g.drawImageAt (desk, 0, 0);
}

void CassetteDeck::paintPapers (juce::Graphics& g)
{
    using namespace juce;
    const Colour paperWhite (0xffeae0c8), paperOld (0xffdccfae), sticky (0xffe2c160);

    // Left: a couple of session sheets, partly under the cassette.
    drawNotePaper (g, { -60.0f, 170.0f, 330.0f, 440.0f }, -0.16f, paperOld, {}, true, colours::pencil, 26.0f, 101);
    drawNotePaper (g, { -30.0f, 120.0f, 320.0f, 430.0f }, -0.07f, paperWhite,
                   { "", "Session - mix bus", "vox: +3 dB in", "tape 15 IPS", "bias ~50 %", "wow - just a bit!", "flutter 15 %" },
                   true, colours::pencil, 26.0f, 102);

    // Right: track list and a sticky note.
    drawNotePaper (g, { 1190.0f, 50.0f, 300.0f, 400.0f }, 0.12f, paperWhite,
                   { "", "SIDE A", "1. Intro", "2. Nocna voznja", "3. Kasetofon", "4. Sila (dub)", "" },
                   true, colours::penBlue, 26.0f, 103);
    drawNotePaper (g, { 1200.0f, 560.0f, 190.0f, 180.0f }, -0.10f, sticky,
                   { "mix petak!", "", "ne zaboravi", "master -1 dB" }, false, colours::ink, 26.0f, 104);

    // A small paper at the bottom left.
    drawNotePaper (g, { 30.0f, 660.0f, 240.0f, 200.0f }, 0.09f, paperWhite,
                   { "", "Trake:", "C60 x 3" }, true, colours::pencil, 24.0f, 105);
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

    DropShadow (Colour (0x48000000), 6, { 2, 2 }).drawForRectangle (g, labelArea.toNearestInt());
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
