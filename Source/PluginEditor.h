#pragma once

#include "PluginProcessor.h"
#include "Advisor.h"

namespace drip::ui
{
    // ---- palette -----------------------------------------------------------
    inline juce::Colour bg0()     { return juce::Colour (0xff06060c); }
    inline juce::Colour bg1()     { return juce::Colour (0xff140a26); }
    inline juce::Colour panel()   { return juce::Colour (0xff0f0f1b); }
    inline juce::Colour panelHi() { return juce::Colour (0xff1b1830); }
    inline juce::Colour text()    { return juce::Colour (0xfff4f0ff); }
    inline juce::Colour dim()     { return juce::Colour (0xff8d88a8); }
    inline juce::Colour cyan()    { return juce::Colour (0xff00e5ff); }
    inline juce::Colour violet()  { return juce::Colour (0xff8a4dff); }
    inline juce::Colour pink()    { return juce::Colour (0xffff2e88); }
    inline juce::Colour red()     { return juce::Colour (0xffff3348); }
    inline juce::Colour orange()  { return juce::Colour (0xffffa31a); }
    inline juce::Colour green()   { return juce::Colour (0xff3dffa0); }

    juce::Colour severityColour (Severity s);
    juce::String severityLabel (Severity s);
    juce::Font font (float px, bool bold = false);

    // meter scale: -54 dB .. +6 dB
    constexpr float meterMinDb = -54.0f, meterMaxDb = 6.0f;
    float dbToProp (float db);

    void drawMeter (juce::Graphics&, juce::Rectangle<float> r, float peakDb, float rmsDb, float holdDb,
                    float scale, bool showScale, bool dimmed);
    void drawGlow (juce::Graphics&, juce::Rectangle<float> r, juce::Colour c, float corner, float strength);

    // ---- look and feel -----------------------------------------------------
    class DripLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        DripLookAndFeel();
        float scale = 1.0f;

        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getPopupMenuFont() override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                                   bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                            bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                            bool isMouseOver, bool isMouseDown) override;
    };

    // ---- the mixer board: one strip per DRIPMIX instance -------------------
    class Board final : public juce::Component
    {
    public:
        Board() { std::fill (std::begin (holdDb), std::end (holdDb), -100.0f); }
        void setData (const std::vector<TrackSnap>& snaps, float scale, int availableWidth, int height);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        float stripWidth() const { return stripW; }

    private:
        void drawStrip (juce::Graphics&, juce::Rectangle<float> r, const TrackSnap& t, float hold);

        std::vector<TrackSnap> tracks;
        std::vector<juce::Rectangle<float>> ledRects;
        float holdDb[SharedBus::maxSlots] {};
        double holdTime[SharedBus::maxSlots] {};
        float s = 1.0f, stripW = 80.0f;
    };

    // ---- the coach: list of tip cards --------------------------------------
    class CoachList final : public juce::Component
    {
    public:
        void setTips (const std::vector<Tip>& tips, int selfSlot, float scale, int width);
        void paint (juce::Graphics&) override;

    private:
        struct Card { Tip tip; juce::TextLayout titleLayout, detailLayout; float height = 0; bool self = false; };
        std::vector<Card> cards;
        float s = 1.0f;
    };

    // ---- big "this track" panel --------------------------------------------
    class SelfPanel final : public juce::Component
    {
    public:
        void setData (const TrackSnap* self, int score, int linkedCount, float scale);
        void paint (juce::Graphics&) override;

    private:
        TrackSnap snap;
        bool hasSnap = false;
        int score = 100, linked = 0;
        float s = 1.0f, hold = -100.0f;
        double holdTime = 0;
        float shownScore = 100.0f;
    };
}

// ============================================================================
class DripMixEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit DripMixEditor (DripMixProcessor&);
    ~DripMixEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshAdvice();
    float scale() const;

    DripMixProcessor& proc;
    drip::ui::DripLookAndFeel lnf;

    juce::ComboBox roleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roleAttachment;
    juce::TextEditor nameEditor;
    juce::TextButton resetButton { "RESET CLIPS" };

    drip::ui::SelfPanel selfPanel;
    drip::ui::Board board;
    juce::Viewport boardView;
    drip::ui::CoachList coachList;
    juce::Viewport coachView;

    std::vector<drip::TrackSnap> snaps;
    std::vector<drip::Tip> tips;
    juce::String lastTipsKey;
    int frame = 0;
    int linkedCount = 0;
    bool hasMaster = false;

    juce::Rectangle<int> headerArea, selfArea, boardArea, coachArea, footerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DripMixEditor)
};
