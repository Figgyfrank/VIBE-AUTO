#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>

// ============================================================================
//  SharedBus: every DRIPMIX instance loaded in the same Ableton process
//  registers a slot here. That's how one plugin window can show the meters
//  of every track that has DRIPMIX on it. Audio threads only write atomics.
// ============================================================================
namespace drip
{

enum class Role : int
{
    Auto = 0,
    Kick,
    Bass808,
    SnareClap,
    HiHats,
    Perc,
    Melody,
    PadKeys,
    LeadVocal,
    AdLibs,
    FX,
    Bus,
    Master,
    NumRoles
};

inline const juce::StringArray& roleNames()
{
    static const juce::StringArray names { "Auto (from track name)", "Kick", "808 / Bass", "Snare / Clap",
                                           "Hi-Hats", "Perc", "Melody / Lead", "Pads / Keys",
                                           "Lead Vocal", "Ad-libs / BGV", "FX / Risers", "Bus / Group", "Master" };
    return names;
}

inline juce::String roleShortName (Role r)
{
    switch (r)
    {
        case Role::Kick:      return "KICK";
        case Role::Bass808:   return "808";
        case Role::SnareClap: return "SNARE";
        case Role::HiHats:    return "HATS";
        case Role::Perc:      return "PERC";
        case Role::Melody:    return "MELODY";
        case Role::PadKeys:   return "PADS";
        case Role::LeadVocal: return "VOX";
        case Role::AdLibs:    return "ADLIBS";
        case Role::FX:        return "FX";
        case Role::Bus:       return "BUS";
        case Role::Master:    return "MASTER";
        case Role::Auto:
        case Role::NumRoles:
        default:              return "???";
    }
}

inline juce::Colour roleColour (Role r)
{
    switch (r)
    {
        case Role::Kick:      return juce::Colour (0xffff3d6e);
        case Role::Bass808:   return juce::Colour (0xffb44dff);
        case Role::SnareClap: return juce::Colour (0xffff8a3d);
        case Role::HiHats:    return juce::Colour (0xffffe14d);
        case Role::Perc:      return juce::Colour (0xffffb347);
        case Role::Melody:    return juce::Colour (0xff3dffb4);
        case Role::PadKeys:   return juce::Colour (0xff3dd5ff);
        case Role::LeadVocal: return juce::Colour (0xff00f0ff);
        case Role::AdLibs:    return juce::Colour (0xff5d8bff);
        case Role::FX:        return juce::Colour (0xffd0d0ff);
        case Role::Bus:       return juce::Colour (0xff9aa0b8);
        case Role::Master:    return juce::Colour (0xffffffff);
        case Role::Auto:
        case Role::NumRoles:
        default:              return juce::Colour (0xff8088a0);
    }
}

// Guess a role from an Ableton track name like "808", "Kick 2", "Vox Lead"...
inline Role guessRoleFromName (const juce::String& rawName)
{
    const auto n = rawName.toLowerCase();
    auto has = [&n] (const char* s) { return n.contains (s); };

    if (n.isEmpty())                                            return Role::Auto;
    if (has ("master") || has ("main out") || has ("2bus") || has ("mix bus")) return Role::Master;
    if (has ("adlib") || has ("ad-lib") || has ("ad lib") || has ("bgv") || has ("dub")
        || has ("double") || has ("backing") || has ("harmony") || has ("hype"))   return Role::AdLibs;
    if (has ("vox") || has ("vocal") || has ("verse") || has ("hook")
        || has ("lead v"))                                           return Role::LeadVocal;
    if (has ("808") || has ("bass") || has ("sub"))                               return Role::Bass808;
    if (has ("kick") || has ("kik"))                                return Role::Kick;
    if (has ("snare") || has ("clap") || has ("snr") || has ("rimshot"))              return Role::SnareClap;
    if (has ("hat") || has ("hh") || has ("hihat") || has ("cymbal") || has ("ride")
        || has ("open h"))                                            return Role::HiHats;
    if (has ("perc") || has ("shaker") || has ("conga") || has ("bongo")
        || has ("tamb") || has ("drum"))                                            return Role::Perc;
    if (has ("pad") || has ("keys") || has ("piano") || has ("choir") || has ("string")
        || has ("organ") || has ("rhodes"))                                         return Role::PadKeys;
    if (has ("fx") || has ("riser") || has ("sweep") || has ("impact")
        || has ("crash") || has ("noise"))                                          return Role::FX;
    if (has ("bus") || has ("group") || has ("drums"))                            return Role::Bus;
    if (has ("melod") || has ("lead") || has ("synth") || has ("guitar") || has ("bell")
        || has ("flute") || has ("pluck") || has ("arp") || has ("sample") || has ("loop")
        || has ("chord"))                                                           return Role::Melody;
    return Role::Auto;
}

// ----------------------------------------------------------------------------
struct Slot
{
    std::atomic<bool>     inUse      { false };
    std::atomic<uint32_t> generation { 0 };

    // published by the processor
    std::atomic<int>   role          { (int) Role::Auto };   // resolved role
    std::atomic<float> peakDb        { -100.0f };   // decaying peak
    std::atomic<float> rmsDb         { -100.0f };   // ~300ms RMS
    std::atomic<float> momentaryLufs { -100.0f };   // 400ms K-weighted
    std::atomic<float> shortLufs     { -100.0f };   // 3s K-weighted
    std::atomic<float> maxPeakDb     { -100.0f };   // since last reset
    std::atomic<float> correlation   { 1.0f };      // -1..+1
    std::atomic<float> lowRatio      { 0.0f };      // energy below ~120Hz / total
    std::atomic<float> lowSideRatio  { 0.0f };      // stereo energy within the lows
    std::atomic<float> crestDb       { 0.0f };      // peak-to-RMS (punch)
    std::atomic<int>   clipCount     { 0 };         // samples over 0 dBFS since reset
    std::atomic<double> lastActiveMs { 0.0 };
    std::atomic<bool>  resetRequest  { false };
    std::atomic<uint32_t> colourARGB { 0 };
    std::atomic<int>   channels      { 2 };

    void setName (const juce::String& s)
    {
        const juce::SpinLock::ScopedLockType sl (nameLock);
        name = s;
    }

    juce::String getName() const
    {
        const juce::SpinLock::ScopedLockType sl (nameLock);
        return name;
    }

    void clearMeters()
    {
        peakDb = -100.0f; rmsDb = -100.0f; momentaryLufs = -100.0f; shortLufs = -100.0f;
        maxPeakDb = -100.0f; correlation = 1.0f; lowRatio = 0.0f; lowSideRatio = 0.0f;
        crestDb = 0.0f; clipCount = 0; lastActiveMs = 0.0; resetRequest = false; colourARGB = 0;
    }

private:
    mutable juce::SpinLock nameLock;
    juce::String name;
};

class SharedBus
{
public:
    static constexpr int maxSlots = 64;

    static SharedBus& get()
    {
        static SharedBus instance;
        return instance;
    }

    int acquire()
    {
        const juce::ScopedLock sl (lock);
        for (int i = 0; i < maxSlots; ++i)
        {
            if (! slots[(size_t) i].inUse.load())
            {
                auto& s = slots[(size_t) i];
                s.clearMeters();
                s.setName ({});
                s.role = (int) Role::Auto;
                s.generation = s.generation.load() + 1;
                s.inUse = true;
                return i;
            }
        }
        return -1; // more than 64 instances: this one just won't show on the board
    }

    void release (int index)
    {
        if (! juce::isPositiveAndBelow (index, maxSlots))
            return;
        const juce::ScopedLock sl (lock);
        slots[(size_t) index].inUse = false;
    }

    Slot* slot (int index)
    {
        return juce::isPositiveAndBelow (index, maxSlots) ? &slots[(size_t) index] : nullptr;
    }

    void resetAllClips()
    {
        for (auto& s : slots)
            if (s.inUse.load())
                s.resetRequest = true;
    }

private:
    SharedBus() = default;
    juce::CriticalSection lock;
    std::array<Slot, maxSlots> slots;
};

} // namespace drip
