#include "PluginEditor.h"

namespace drip::ui
{

// ============================================================================
//  helpers
// ============================================================================
juce::Colour severityColour (Severity s)
{
    switch (s)
    {
        case Severity::Critical: return red();
        case Severity::Warn:     return orange();
        case Severity::Good:     return green();
        case Severity::Info:
        default:                 return cyan();
    }
}

juce::String severityLabel (Severity s)
{
    switch (s)
    {
        case Severity::Critical: return "FIX NOW";
        case Severity::Warn:     return "HEADS UP";
        case Severity::Good:     return "CLEAN";
        case Severity::Info:
        default:                 return "TIP";
    }
}

juce::Font font (float px, bool bold)
{
    return juce::Font (juce::FontOptions (juce::jmax (6.0f, px), bold ? juce::Font::bold : juce::Font::plain));
}

float dbToProp (float db)
{
    return juce::jlimit (0.0f, 1.0f, (db - meterMinDb) / (meterMaxDb - meterMinDb));
}

void drawGlow (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float corner, float strength)
{
    for (int i = 3; i >= 1; --i)
    {
        const float grow = (float) i * 2.5f;
        g.setColour (c.withAlpha (strength * 0.10f * (4 - i)));
        g.drawRoundedRectangle (r.expanded (grow), corner + grow, 2.0f);
    }
}

void drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float peakDb, float rmsDb, float holdDb,
                float s, bool showScale, bool dimmed)
{
    const float corner = 4.0f * s;

    juce::Rectangle<float> scaleArea;
    if (showScale)
        scaleArea = r.removeFromLeft (24.0f * s);

    // well
    g.setColour (juce::Colour (0xff05050a));
    g.fillRoundedRectangle (r, corner);

    // tick lines
    const float ticks[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f };
    for (auto t : ticks)
    {
        const float y = r.getBottom() - dbToProp (t) * r.getHeight();
        g.setColour (t == 0.0f ? red().withAlpha (0.55f) : juce::Colours::white.withAlpha (t == -6.0f || t == -18.0f ? 0.13f : 0.06f));
        g.drawHorizontalLine ((int) y, r.getX() + 1.0f, r.getRight() - 1.0f);

        if (showScale)
        {
            g.setColour (t == 0.0f ? red() : dim());
            g.setFont (font (9.5f * s, t == 0.0f));
            g.drawText (juce::String ((int) t), scaleArea.withY (y - 6.0f * s).withHeight (12.0f * s).reduced (2.0f * s, 0),
                        juce::Justification::centredRight, false);
        }
    }

    // gradient tied to dB position so the colour means something
    juce::ColourGradient grad (cyan(), 0.0f, r.getBottom(), juce::Colour (0xffff1f2e), 0.0f, r.getY(), false);
    grad.addColour (dbToProp (-30.0f), juce::Colour (0xff00b3ff));
    grad.addColour (dbToProp (-18.0f), violet());
    grad.addColour (dbToProp (-8.0f),  pink());
    grad.addColour (dbToProp (-1.0f),  juce::Colour (0xffff5a1f));

    auto inner = r.reduced (2.0f * s);
    const float alphaMul = dimmed ? 0.35f : 1.0f;

    auto barFor = [&inner] (float db)
    {
        const float h = dbToProp (db) * inner.getHeight();
        return inner.withTop (inner.getBottom() - h);
    };

    if (peakDb > meterMinDb)
    {
        auto pr = barFor (peakDb);
        g.setGradientFill (grad);
        g.setOpacity (0.38f * alphaMul);
        g.fillRoundedRectangle (pr, 2.0f * s);
    }
    if (rmsDb > meterMinDb)
    {
        auto rr = barFor (rmsDb);
        g.setGradientFill (grad);
        g.setOpacity (0.95f * alphaMul);
        g.fillRoundedRectangle (rr, 2.0f * s);

        // glossy highlight
        g.setOpacity (1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f * alphaMul));
        g.fillRect (rr.withWidth (rr.getWidth() * 0.3f));
    }
    g.setOpacity (1.0f);

    if (holdDb > meterMinDb)
    {
        const float y = inner.getBottom() - dbToProp (holdDb) * inner.getHeight();
        g.setColour ((holdDb > 0.0f ? red() : juce::Colours::white).withAlpha (0.9f * alphaMul));
        g.fillRect (inner.getX(), y - 1.0f * s, inner.getWidth(), 2.0f * s);
    }

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

// ============================================================================
//  LookAndFeel
// ============================================================================
DripLookAndFeel::DripLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, panelHi());
    setColour (juce::ComboBox::textColourId, text());
    setColour (juce::ComboBox::outlineColourId, violet());
    setColour (juce::ComboBox::arrowColourId, cyan());
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff12101f));
    setColour (juce::PopupMenu::textColourId, text());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, violet().withAlpha (0.6f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, panelHi());
    setColour (juce::TextEditor::textColourId, text());
    setColour (juce::TextEditor::outlineColourId, violet().withAlpha (0.6f));
    setColour (juce::TextEditor::focusedOutlineColourId, cyan());
    setColour (juce::TextEditor::highlightColourId, pink().withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, cyan());
    setColour (juce::TextButton::buttonColourId, pink());
    setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ScrollBar::thumbColourId, violet());
    setColour (juce::ResizableWindow::backgroundColourId, bg0());
}

juce::Font DripLookAndFeel::getComboBoxFont (juce::ComboBox&)      { return font (14.0f * scale, true); }
juce::Font DripLookAndFeel::getPopupMenuFont()                      { return font (15.0f * scale); }
juce::Font DripLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return font (juce::jmin (13.0f * scale, (float) h * 0.45f), true);
}

void DripLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                    int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (1.0f);
    const float corner = r.getHeight() * 0.3f;
    g.setColour (panelHi());
    g.fillRoundedRectangle (r, corner);

    juce::ColourGradient outline (violet(), r.getX(), 0, cyan(), r.getRight(), 0, false);
    g.setGradientFill (outline);
    g.drawRoundedRectangle (r, corner, box.hasKeyboardFocus (true) ? 2.0f : 1.4f);

    // arrow
    const float ah = r.getHeight() * 0.22f;
    const float cx = r.getRight() - r.getHeight() * 0.55f, cy = r.getCentreY();
    juce::Path p;
    p.addTriangle (cx - ah, cy - ah * 0.5f, cx + ah, cy - ah * 0.5f, cx, cy + ah * 0.7f);
    g.setColour (cyan());
    g.fillPath (p);
}

void DripLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                            bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = r.getHeight() * 0.5f;
    juce::ColourGradient grad (pink(), r.getX(), r.getY(), violet(), r.getRight(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.setOpacity (down ? 0.7f : (over ? 1.0f : 0.88f));
    g.fillRoundedRectangle (r, corner);
    g.setOpacity (1.0f);
    if (over)
        drawGlow (g, r, pink(), corner, 0.8f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void DripLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                                     bool vertical, int thumbStart, int thumbSize, bool over, bool)
{
    juce::Rectangle<float> thumb = vertical
        ? juce::Rectangle<float> ((float) x + 2, (float) thumbStart, (float) width - 4, (float) thumbSize)
        : juce::Rectangle<float> ((float) thumbStart, (float) y + 2, (float) thumbSize, (float) height - 4);
    g.setColour (violet().withAlpha (over ? 0.9f : 0.55f));
    g.fillRoundedRectangle (thumb, juce::jmin (thumb.getWidth(), thumb.getHeight()) * 0.5f);
}

// ============================================================================
//  Board
// ============================================================================
void Board::setData (const std::vector<TrackSnap>& snaps, float scale, int availableWidth, int height)
{
    tracks = snaps;
    s = scale;

    const double now = juce::Time::getMillisecondCounterHiRes();
    for (auto& t : tracks)
    {
        const int i = t.slotIndex;
        if (! juce::isPositiveAndBelow (i, SharedBus::maxSlots)) continue;
        if (t.peakDb >= holdDb[i] || now - holdTime[i] > 1500.0)
        {
            if (t.peakDb >= holdDb[i]) { holdDb[i] = t.peakDb; holdTime[i] = now; }
            else holdDb[i] = juce::jmax (t.peakDb, holdDb[i] - 1.2f); // fall after hold
        }
    }

    const float gap = 8.0f * s;
    const int n = juce::jmax (1, (int) tracks.size());
    const float minW = 64.0f * s, maxW = 104.0f * s;
    stripW = juce::jlimit (minW, maxW, ((float) availableWidth - gap) / (float) n - gap);
    const int w = juce::jmax (availableWidth, (int) std::ceil (gap + (float) n * (stripW + gap)));
    if (getWidth() != w || getHeight() != height)
        setSize (w, height);
    repaint();
}

void Board::paint (juce::Graphics& g)
{
    ledRects.clear();

    if (tracks.empty())
    {
        g.setColour (dim());
        g.setFont (font (15.0f * s, true));
        g.drawFittedText ("No tracks linked yet.\nAdd DRIPMIX to your Kick, 808, Vocals, Hats, Melody and Master.",
                          getLocalBounds().reduced (20), juce::Justification::centred, 4);
        return;
    }

    const float gap = 8.0f * s;
    float x = gap;
    for (auto& t : tracks)
    {
        juce::Rectangle<float> r (x, gap, stripW, (float) getHeight() - 2.0f * gap);
        const float hold = juce::isPositiveAndBelow (t.slotIndex, SharedBus::maxSlots) ? holdDb[t.slotIndex] : -100.0f;
        drawStrip (g, r, t, hold);
        x += stripW + gap;
    }
}

void Board::drawStrip (juce::Graphics& g, juce::Rectangle<float> r, const TrackSnap& t, float hold)
{
    const float corner = 10.0f * s;
    const auto rc = roleColour (t.role);
    const bool clipped = t.maxPeakDb > 0.0f;
    const bool dimmed = ! t.active;

    // body
    juce::ColourGradient body (panelHi(), r.getX(), r.getY(), panel(), r.getX(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, corner);

    if (t.isSelf)
        drawGlow (g, r, cyan(), corner, 1.0f);
    if (clipped)
        drawGlow (g, r, red(), corner, 0.6f + 0.4f * (float) std::sin (juce::Time::getMillisecondCounterHiRes() * 0.012));

    g.setColour (t.isSelf ? cyan() : juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r, corner, t.isSelf ? 1.6f : 1.0f);

    auto inner = r.reduced (6.0f * s);

    // role colour cap
    {
        auto cap = inner.removeFromTop (4.0f * s);
        juce::ColourGradient cg (rc, cap.getX(), 0, t.colour.isTransparent() ? rc : t.colour, cap.getRight(), 0, false);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (cap, 2.0f * s);
    }
    inner.removeFromTop (4.0f * s);

    // role tag + name
    g.setColour (rc.withAlpha (dimmed ? 0.5f : 1.0f));
    g.setFont (font (11.0f * s, true));
    g.drawText (roleShortName (t.role), inner.removeFromTop (14.0f * s), juce::Justification::centred, true);

    const juce::String nm = t.name.isNotEmpty() ? t.name : ("Track " + juce::String (t.slotIndex + 1));
    g.setColour (text().withAlpha (dimmed ? 0.45f : 0.95f));
    g.setFont (font (12.0f * s));
    g.drawText (nm, inner.removeFromTop (16.0f * s), juce::Justification::centred, true);
    inner.removeFromTop (4.0f * s);

    // clip LED (click to reset)
    auto led = inner.removeFromTop (20.0f * s);
    ledRects.push_back (led);
    if (clipped)
    {
        g.setColour (red());
        g.fillRoundedRectangle (led, 5.0f * s);
        drawGlow (g, led, red(), 5.0f * s, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (font (11.0f * s, true));
        g.drawText ("CLIP +" + juce::String (t.maxPeakDb, 1), led, juce::Justification::centred, true);
    }
    else
    {
        g.setColour (juce::Colour (0xff1d1a2e));
        g.fillRoundedRectangle (led, 5.0f * s);
        const bool hot = t.maxPeakDb > -3.0f;
        g.setColour (t.maxPeakDb <= -99.0f ? dim() : (hot ? orange() : green()));
        g.setFont (font (11.0f * s, true));
        g.drawText (t.maxPeakDb <= -99.0f ? "--" : juce::String (t.maxPeakDb, 1), led, juce::Justification::centred, true);
    }
    inner.removeFromTop (6.0f * s);

    // readout at the bottom
    auto readout = inner.removeFromBottom (30.0f * s);
    inner.removeFromBottom (4.0f * s);

    // meter
    const float meterW = juce::jmin (inner.getWidth(), 34.0f * s);
    auto meter = inner.withSizeKeepingCentre (meterW, inner.getHeight());
    drawMeter (g, meter, t.peakDb, t.rmsDb, hold, s, false, dimmed);

    // readouts
    g.setFont (font (13.0f * s, true));
    g.setColour (dimmed ? dim() : text());
    const juce::String lufs = t.shortLufs > -70.0f ? juce::String (t.shortLufs, 1) : "--";
    g.drawText (lufs, readout.removeFromTop (16.0f * s), juce::Justification::centred, false);
    g.setFont (font (9.5f * s));
    g.setColour (dim());
    g.drawText ("LUFS-S", readout, juce::Justification::centredTop, false);
}

void Board::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    for (size_t i = 0; i < ledRects.size() && i < tracks.size(); ++i)
    {
        if (ledRects[i].expanded (4.0f).contains (p))
        {
            if (auto* sl = SharedBus::get().slot (tracks[i].slotIndex))
                sl->resetRequest = true;
            holdDb[tracks[i].slotIndex] = -100.0f;
            return;
        }
    }
}

// ============================================================================
//  CoachList
// ============================================================================
void CoachList::setTips (const std::vector<Tip>& tips, int selfSlot, float scale, int width)
{
    s = scale;
    cards.clear();

    const float pad = 12.0f * s;
    const float textW = (float) width - pad * 2.0f - 8.0f * s - 4.0f;
    float total = 4.0f * s;

    for (auto& tip : tips)
    {
        Card c;
        c.tip = tip;
        c.self = (tip.slotIndex >= 0 && tip.slotIndex == selfSlot);

        juce::AttributedString title;
        title.append (tip.title, font (14.5f * s, true), text());
        title.setWordWrap (juce::AttributedString::byWord);
        c.titleLayout.createLayout (title, textW);

        juce::AttributedString detail;
        detail.append (tip.detail, font (12.5f * s), text().withAlpha (0.72f));
        detail.setWordWrap (juce::AttributedString::byWord);
        detail.setLineSpacing (2.0f * s);
        c.detailLayout.createLayout (detail, textW);

        c.height = pad + 14.0f * s + 4.0f * s + c.titleLayout.getHeight() + 4.0f * s + c.detailLayout.getHeight() + pad;
        total += c.height + 8.0f * s;
        cards.push_back (std::move (c));
    }

    setSize (width, (int) std::ceil (total));
    repaint();
}

void CoachList::paint (juce::Graphics& g)
{
    float y = 4.0f * s;
    const float pad = 12.0f * s;

    for (auto& c : cards)
    {
        juce::Rectangle<float> r (2.0f, y, (float) getWidth() - 4.0f, c.height);
        const auto sc = severityColour (c.tip.severity);

        juce::ColourGradient bgGrad (sc.withAlpha (0.13f), r.getX(), r.getY(), panel().withAlpha (0.9f), r.getRight(), r.getBottom(), false);
        g.setGradientFill (bgGrad);
        g.fillRoundedRectangle (r, 10.0f * s);
        g.setColour (sc.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 10.0f * s, 1.0f);

        // accent bar
        g.setColour (sc);
        g.fillRoundedRectangle (r.withWidth (4.0f * s).reduced (0, 8.0f * s), 2.0f * s);

        auto content = r.reduced (pad, pad).withTrimmedLeft (8.0f * s);
        auto top = content.removeFromTop (14.0f * s);

        // severity chip
        g.setFont (font (10.0f * s, true));
        const auto label = severityLabel (c.tip.severity);
        const float chipW = (float) juce::GlyphArrangement::getStringWidthInt (font (10.0f * s, true), label) + 12.0f * s;
        auto chip = top.removeFromLeft (chipW);
        g.setColour (sc.withAlpha (0.2f));
        g.fillRoundedRectangle (chip, 7.0f * s);
        g.setColour (sc);
        g.drawText (label, chip, juce::Justification::centred, false);

        if (c.self)
        {
            top.removeFromLeft (6.0f * s);
            const juce::String me ("THIS TRACK");
            const float w = (float) juce::GlyphArrangement::getStringWidthInt (font (10.0f * s, true), me) + 12.0f * s;
            auto chip2 = top.removeFromLeft (w);
            g.setColour (cyan().withAlpha (0.2f));
            g.fillRoundedRectangle (chip2, 7.0f * s);
            g.setColour (cyan());
            g.drawText (me, chip2, juce::Justification::centred, false);
        }

        content.removeFromTop (4.0f * s);
        c.titleLayout.draw (g, content.removeFromTop (c.titleLayout.getHeight()));
        content.removeFromTop (4.0f * s);
        c.detailLayout.draw (g, content.removeFromTop (c.detailLayout.getHeight()));

        y += c.height + 8.0f * s;
    }
}

// ============================================================================
//  SelfPanel
// ============================================================================
void SelfPanel::setData (const TrackSnap* self, int newScore, int linkedCount, float scale)
{
    s = scale;
    hasSnap = self != nullptr;
    if (self != nullptr)
    {
        snap = *self;
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (snap.peakDb >= hold) { hold = snap.peakDb; holdTime = now; }
        else if (now - holdTime > 1500.0) hold = juce::jmax (snap.peakDb, hold - 1.2f);
    }
    score = newScore;
    linked = linkedCount;
    shownScore += 0.15f * ((float) score - shownScore);
    repaint();
}

void SelfPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float corner = 14.0f * s;

    juce::ColourGradient body (panelHi(), r.getX(), r.getY(), panel(), r.getRight(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r, corner, 1.0f);

    auto inner = r.reduced (14.0f * s);

    g.setColour (dim());
    g.setFont (font (11.0f * s, true));
    g.drawText ("THIS TRACK", inner.removeFromTop (14.0f * s), juce::Justification::centredLeft, false);

    if (! hasSnap)
    {
        g.setColour (text());
        g.setFont (font (13.0f * s));
        g.drawFittedText ("Too many DRIPMIX instances (max 64).", inner.toNearestInt(), juce::Justification::centred, 3);
        return;
    }

    const auto rc = roleColour (snap.role);
    const juce::String nm = snap.name.isNotEmpty() ? snap.name : ("Track " + juce::String (snap.slotIndex + 1));
    g.setColour (text());
    g.setFont (font (20.0f * s, true));
    g.drawText (nm, inner.removeFromTop (26.0f * s), juce::Justification::centredLeft, true);
    g.setColour (rc);
    g.setFont (font (12.0f * s, true));
    g.drawText (snap.role == Role::Auto ? "ROLE: PICK ONE ABOVE" : roleShortName (snap.role),
                inner.removeFromTop (16.0f * s), juce::Justification::centredLeft, true);
    inner.removeFromTop (10.0f * s);

    // ---- score gauge ----
    {
        const float gs = juce::jmin (inner.getWidth(), 130.0f * s);
        auto gauge = inner.removeFromTop (gs).withSizeKeepingCentre (gs, gs).reduced (6.0f * s);
        const float start = juce::MathConstants<float>::pi * 1.25f;
        const float end   = juce::MathConstants<float>::pi * 2.75f;
        const float thick = 9.0f * s;
        const auto c = gauge.getCentre();
        const float rad = gauge.getWidth() * 0.5f - thick;

        juce::Path bgArc;
        bgArc.addCentredArc (c.x, c.y, rad, rad, 0.0f, start, end, true);
        g.setColour (juce::Colour (0xff221e38));
        g.strokePath (bgArc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float frac = juce::jlimit (0.0f, 1.0f, shownScore / 100.0f);
        const auto scoreCol = shownScore >= 80.0f ? green() : (shownScore >= 55.0f ? orange() : red());
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rad, rad, 0.0f, start, start + (end - start) * juce::jmax (0.001f, frac), true);
        juce::ColourGradient ag (violet(), gauge.getX(), gauge.getBottom(), scoreCol, gauge.getRight(), gauge.getY(), false);
        g.setGradientFill (ag);
        g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (scoreCol.withAlpha (0.18f));
        g.strokePath (arc, juce::PathStrokeType (thick * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (text());
        g.setFont (font (34.0f * s, true));
        g.drawText (juce::String (juce::roundToInt (shownScore)), gauge.withTrimmedBottom (gauge.getHeight() * 0.12f),
                    juce::Justification::centred, false);
        g.setColour (dim());
        g.setFont (font (10.0f * s, true));
        g.drawText (snap.role == Role::Master ? "MIX SCORE" : "DRIP SCORE",
                    gauge.withTrimmedTop (gauge.getHeight() * 0.62f), juce::Justification::centredTop, false);
    }
    inner.removeFromTop (8.0f * s);

    // ---- stats + big meter ----
    auto statsArea = inner.removeFromBottom (juce::jmin (inner.getHeight() * 0.5f, 150.0f * s));
    auto meterArea = inner;

    {
        auto m = meterArea.withSizeKeepingCentre (juce::jmin (meterArea.getWidth(), 86.0f * s), meterArea.getHeight());
        drawMeter (g, m, snap.peakDb, snap.rmsDb, hold, s, true, ! snap.active);
    }

    struct Stat { juce::String label, value; juce::Colour col; };
    auto fmt = [] (float v) { return v > -70.0f ? juce::String (v, 1) : juce::String ("--"); };
    const bool clipped = snap.maxPeakDb > 0.0f;
    const Stat stats[] = {
        { "MAX PEAK", snap.maxPeakDb <= -99.0f ? "--" : juce::String (snap.maxPeakDb, 1) + " dB",
                     clipped ? red() : (snap.maxPeakDb > -3.0f ? orange() : green()) },
        { "RMS",      fmt (snap.rmsDb) + " dB", text() },
        { "LUFS-S",   fmt (snap.shortLufs), cyan() },
        { "PUNCH",    snap.crestDb > 0.1f ? juce::String (snap.crestDb, 1) + " dB" : "--", pink() },
        { "SUB",      juce::String (juce::roundToInt (snap.lowRatio * 100.0f)) + "%", violet() },
        { "WIDTH",    snap.channels < 2 ? juce::String ("MONO") : juce::String (snap.correlation, 2), juce::Colour (0xff5d8bff) },
    };

    const float rowH = statsArea.getHeight() / 3.0f;
    for (int i = 0; i < 6; ++i)
    {
        auto cell = juce::Rectangle<float> (statsArea.getX() + (float) (i % 2) * statsArea.getWidth() * 0.5f,
                                            statsArea.getY() + (float) (i / 2) * rowH,
                                            statsArea.getWidth() * 0.5f, rowH).reduced (3.0f * s);
        g.setColour (juce::Colour (0xff0a0a14));
        g.fillRoundedRectangle (cell, 6.0f * s);
        g.setColour (dim());
        g.setFont (font (9.0f * s, true));
        g.drawText (stats[i].label, cell.reduced (6.0f * s, 3.0f * s).removeFromTop (cell.getHeight() * 0.38f),
                    juce::Justification::centredLeft, false);
        g.setColour (stats[i].col);
        g.setFont (font (juce::jmin (15.0f * s, cell.getHeight() * 0.45f), true));
        g.drawText (stats[i].value, cell.reduced (6.0f * s, 2.0f * s).removeFromBottom (cell.getHeight() * 0.58f),
                    juce::Justification::centredLeft, false);
    }
}

} // namespace drip::ui

// ============================================================================
//  Editor
// ============================================================================
using namespace drip;

DripMixEditor::DripMixEditor (DripMixProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    roleBox.addItemList (roleNames(), 1);
    roleBox.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (roleBox);
    roleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "role", roleBox);

    nameEditor.setText (proc.getCustomName(), false);
    nameEditor.setSelectAllWhenFocused (true);
    nameEditor.setInputRestrictions (40);
    auto commitName = [this] { proc.setCustomName (nameEditor.getText()); };
    nameEditor.onReturnKey = [this, commitName] { commitName(); unfocusAllComponents(); };
    nameEditor.onFocusLost = commitName;
    addAndMakeVisible (nameEditor);

    resetButton.onClick = [] { SharedBus::get().resetAllClips(); };
    addAndMakeVisible (resetButton);

    addAndMakeVisible (selfPanel);

    boardView.setViewedComponent (&board, false);
    boardView.setScrollBarsShown (false, true);
    boardView.setScrollBarThickness (8);
    addAndMakeVisible (boardView);

    coachView.setViewedComponent (&coachList, false);
    coachView.setScrollBarsShown (true, false);
    coachView.setScrollBarThickness (8);
    addAndMakeVisible (coachView);

    setResizable (true, true);
    setResizeLimits (760, 440, 3000, 2000);
    if (auto* c = getConstrainer())
        c->setMinimumOnscreenAmounts (60, 60, 60, 60);
    setSize (proc.savedEditorWidth, proc.savedEditorHeight);

    timerCallback();
    startTimerHz (30);
}

DripMixEditor::~DripMixEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

float DripMixEditor::scale() const
{
    return juce::jlimit (0.7f, 2.2f, juce::jmin ((float) getWidth() / 1180.0f, (float) getHeight() / 680.0f));
}

void DripMixEditor::resized()
{
    const float s = scale();
    lnf.scale = s;
    proc.savedEditorWidth = getWidth();
    proc.savedEditorHeight = getHeight();

    auto r = getLocalBounds();
    headerArea = r.removeFromTop ((int) (78.0f * s));
    footerArea = r.removeFromBottom ((int) (24.0f * s));
    r.reduce ((int) (12.0f * s), (int) (6.0f * s));

    const int gap = (int) (12.0f * s);
    selfArea  = r.removeFromLeft ((int) (r.getWidth() * 0.22f));
    r.removeFromLeft (gap);
    coachArea = r.removeFromRight ((int) (r.getWidth() * 0.36f));
    r.removeFromRight (gap);
    boardArea = r;

    selfPanel.setBounds (selfArea);
    boardView.setBounds (boardArea.withTrimmedTop ((int) (26.0f * s)));
    coachView.setBounds (coachArea.withTrimmedTop ((int) (26.0f * s)));

    // header controls (right side)
    auto h = headerArea.reduced ((int) (16.0f * s), 0).withTrimmedBottom ((int) (14.0f * s));
    const int ctrlH = (int) (30.0f * s);
    auto row = h.withSizeKeepingCentre (h.getWidth(), ctrlH);
    resetButton.setBounds (row.removeFromRight ((int) (120.0f * s)));
    row.removeFromRight ((int) (10.0f * s));
    roleBox.setBounds (row.removeFromRight ((int) (200.0f * s)));
    row.removeFromRight ((int) (10.0f * s));
    nameEditor.setBounds (row.removeFromRight ((int) (170.0f * s)));
    nameEditor.applyFontToAllText (drip::ui::font (14.0f * s, true));
    nameEditor.setIndents ((int) (8.0f * s), juce::jmax (0, (ctrlH - (int) (16.0f * s)) / 2));

    lastTipsKey.clear(); // force a re-layout of the coach cards
    timerCallback();
}

void DripMixEditor::timerCallback()
{
    const float s = scale();
    snaps = snapshotBus (proc.getSlotIndex());

    linkedCount = (int) snaps.size();
    hasMaster = false;
    const TrackSnap* self = nullptr;
    for (auto& t : snaps)
    {
        if (t.isSelf) self = &t;
        if (t.role == Role::Master) hasMaster = true;
    }

    board.setData (snaps, s, boardView.getMaximumVisibleWidth() > 0 ? boardView.getMaximumVisibleWidth() : boardArea.getWidth(),
                   juce::jmax (100, boardView.getHeight() - 10));

    if (frame++ % 15 == 0 || lastTipsKey.isEmpty())
        refreshAdvice();

    // score: tips about this track (or the whole mix when on the master)
    int score = 100;
    const bool onMaster = self != nullptr && self->role == Role::Master;
    for (auto& tip : tips)
    {
        if (! onMaster && tip.slotIndex != proc.getSlotIndex()) continue;
        if (tip.severity == Severity::Critical) score -= 35;
        else if (tip.severity == Severity::Warn) score -= 15;
        else if (tip.severity == Severity::Info) score -= 4;
    }
    selfPanel.setData (self, juce::jlimit (0, 100, score), linkedCount, s);

    // name hint
    const auto hostName = proc.getHostTrackName();
    nameEditor.setTextToShowWhenEmpty (hostName.isNotEmpty() ? hostName : "Track name...", drip::ui::dim());

    repaint (headerArea);
    repaint (footerArea);
    repaint (boardArea.withHeight ((int) (24.0f * s)));
    repaint (coachArea.withHeight ((int) (24.0f * s)));
}

void DripMixEditor::refreshAdvice()
{
    tips = buildAdvice (snaps);

    juce::String key;
    for (auto& t : tips)
        key << (int) t.severity << t.title << "|";
    key << coachView.getWidth();

    if (key != lastTipsKey)
    {
        lastTipsKey = key;
        const int w = juce::jmax (100, coachView.getMaximumVisibleWidth() > 0 ? coachView.getMaximumVisibleWidth() : coachArea.getWidth());
        coachList.setTips (tips, proc.getSlotIndex(), scale(), w);
    }
}

void DripMixEditor::paint (juce::Graphics& g)
{
    using namespace drip::ui;
    const float s = scale();
    const auto bounds = getLocalBounds().toFloat();
    const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;

    // ---- background ----
    {
        juce::ColourGradient bg (bg1(), 0, 0, bg0(), bounds.getWidth() * 0.6f, bounds.getHeight(), false);
        g.setGradientFill (bg);
        g.fillAll();

        // neon grid floor
        g.setColour (violet().withAlpha (0.05f));
        const float step = 36.0f * s;
        for (float x = 0; x < bounds.getWidth(); x += step)
            g.drawVerticalLine ((int) x, 0, bounds.getHeight());
        for (float y = 0; y < bounds.getHeight(); y += step)
            g.drawHorizontalLine ((int) y, 0, bounds.getWidth());

        // soft colour blobs
        juce::ColourGradient blob1 (pink().withAlpha (0.10f), bounds.getRight() * 0.85f, bounds.getBottom() * 0.9f,
                                    juce::Colours::transparentBlack, bounds.getRight() * 0.45f, bounds.getBottom() * 0.4f, true);
        g.setGradientFill (blob1);
        g.fillRect (bounds);
        juce::ColourGradient blob2 (cyan().withAlpha (0.07f), bounds.getWidth() * 0.1f, bounds.getHeight() * 0.7f,
                                    juce::Colours::transparentBlack, bounds.getWidth() * 0.45f, bounds.getHeight() * 0.2f, true);
        g.setGradientFill (blob2);
        g.fillRect (bounds);
    }

    // ---- header with drips ----
    {
        auto h = headerArea.toFloat();
        const float dripBase = h.getBottom() - 16.0f * s;
        const float hue = (float) std::fmod (t * 0.03, 1.0);

        juce::Path header;
        header.startNewSubPath (0, 0);
        header.lineTo (h.getRight(), 0);
        header.lineTo (h.getRight(), dripBase);

        // drips across the bottom edge, right to left
        juce::Random rng (1337);
        float x = h.getRight();
        while (x > 0)
        {
            const float w = (18.0f + rng.nextFloat() * 34.0f) * s;
            const float len = (rng.nextFloat() < 0.45f ? (8.0f + rng.nextFloat() * 22.0f) : 0.0f) * s;
            const float wobble = (float) std::sin (t * 1.6 + x * 0.05) * 2.0f * s;
            const float x0 = x, x1 = juce::jmax (0.0f, x - w);
            if (len > 0)
            {
                const float mid = (x0 + x1) * 0.5f;
                const float rr = juce::jmin (w * 0.32f, 9.0f * s);
                header.lineTo (mid + rr, dripBase);
                header.lineTo (mid + rr * 0.8f, dripBase + len + wobble - rr);
                header.cubicTo (mid + rr * 0.8f, dripBase + len + wobble + rr * 0.6f,
                                mid - rr * 0.8f, dripBase + len + wobble + rr * 0.6f,
                                mid - rr * 0.8f, dripBase + len + wobble - rr);
                header.lineTo (mid - rr, dripBase);
            }
            header.lineTo (x1, dripBase);
            x = x1;
        }
        header.closeSubPath();

        juce::ColourGradient hg (juce::Colour::fromHSV (0.78f + hue * 0.1f, 0.85f, 0.55f, 1.0f), 0, 0,
                                 juce::Colour::fromHSV (0.92f + hue * 0.05f, 0.9f, 0.60f, 1.0f), h.getRight(), h.getBottom(), false);
        hg.addColour (0.55, juce::Colour (0xff3a1270));
        g.setGradientFill (hg);
        g.fillPath (header);
        g.setColour (pink().withAlpha (0.5f));
        g.strokePath (header, juce::PathStrokeType (1.2f));

        // logo
        auto logo = h.reduced (18.0f * s, 0).withTrimmedBottom (16.0f * s);
        const juce::String word ("DRIPMIX");
        auto logoFont = font (34.0f * s, true).withExtraKerningFactor (0.06f);
        g.setFont (logoFont);
        auto logoRect = logo.removeFromLeft (220.0f * s);
        g.setColour (cyan().withAlpha (0.35f));
        g.drawText (word, logoRect.translated (2.0f * s, 2.0f * s), juce::Justification::centredLeft, false);
        g.setColour (juce::Colours::white);
        g.drawText (word, logoRect, juce::Justification::centredLeft, false);

        g.setFont (font (11.0f * s, true));
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawText ("TRAP MIX COACH  //  v1.0", logo.removeFromLeft (200.0f * s), juce::Justification::centredLeft, false);
    }

    // ---- section titles ----
    auto sectionTitle = [&] (juce::Rectangle<int> area, const juce::String& title, const juce::String& right, juce::Colour c)
    {
        auto tr = area.removeFromTop ((int) (22.0f * s)).toFloat();
        g.setColour (c);
        g.fillRoundedRectangle (tr.removeFromLeft (4.0f * s).reduced (0, 4.0f * s), 2.0f * s);
        tr.removeFromLeft (8.0f * s);
        g.setColour (text());
        g.setFont (font (13.0f * s, true));
        g.drawText (title, tr, juce::Justification::centredLeft, false);
        g.setColour (dim());
        g.setFont (font (11.0f * s));
        g.drawText (right, tr, juce::Justification::centredRight, false);
    };

    sectionTitle (boardArea, "THE BOARD", juce::String (linkedCount) + " linked  |  click a clip light to reset", violet());

    int crit = 0, warn = 0;
    for (auto& tp : tips) { if (tp.severity == Severity::Critical) ++crit; else if (tp.severity == Severity::Warn) ++warn; }
    sectionTitle (coachArea, "MIX COACH",
                  crit + warn == 0 ? juce::String ("all clean") : (juce::String (crit) + " fix  |  " + juce::String (warn) + " heads up"),
                  crit > 0 ? red() : (warn > 0 ? orange() : green()));

    // ---- footer ----
    {
        auto f = footerArea.toFloat().reduced (14.0f * s, 0);
        g.setFont (font (10.5f * s));
        g.setColour (dim());
        g.drawText (hasMaster ? "Master linked - headroom + loudness checks ON"
                              : "Tip: put DRIPMIX on your Master to unlock headroom + loudness checks",
                    f, juce::Justification::centredLeft, true);
        g.drawText ("Levels are guides, trust your ears", f, juce::Justification::centredRight, true);
    }
}
