#pragma once

#include "SharedBus.h"
#include <vector>

// ============================================================================
//  Advisor: the "mix coach". Looks at every active DRIPMIX track and turns the
//  numbers into trap-specific advice. Runs on the message thread.
// ============================================================================
namespace drip
{

struct TrackSnap
{
    int slotIndex = -1;
    juce::String name;
    Role role = Role::Auto;
    juce::Colour colour;
    float peakDb = -100, rmsDb = -100, momentaryLufs = -100, shortLufs = -100, maxPeakDb = -100;
    float correlation = 1, lowRatio = 0, lowSideRatio = 0, crestDb = 0;
    int clipCount = 0;
    int channels = 2;
    bool active = false;   // made sound in the last few seconds
    bool isSelf = false;   // the instance whose window is showing
};

enum class Severity : int { Good = 0, Info = 1, Warn = 2, Critical = 3 };

struct Tip
{
    Severity severity = Severity::Info;
    juce::String title;
    juce::String detail;
    int slotIndex = -1;          // which track it's about (-1 = whole mix)
    juce::Colour accent;
};

// Grab a consistent picture of every registered instance.
std::vector<TrackSnap> snapshotBus (int selfSlot);

// Turn the picture into advice, most important first.
std::vector<Tip> buildAdvice (const std::vector<TrackSnap>& tracks);

} // namespace drip
