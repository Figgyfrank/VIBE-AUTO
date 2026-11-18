#include "Advisor.h"
#include <algorithm>

namespace drip
{

static juce::String displayName (const TrackSnap& t)
{
    if (t.name.isNotEmpty())
        return t.name;
    if (t.role != Role::Auto)
        return roleShortName (t.role);
    return "Track " + juce::String (t.slotIndex + 1);
}

static juce::String dbAbs (float v) { return juce::String (std::abs (v), 1) + " dB"; }

static juce::String db (float v, int decimals = 1)
{
    return (v >= 0 ? "+" : "") + juce::String (v, decimals) + " dB";
}

std::vector<TrackSnap> snapshotBus (int selfSlot)
{
    std::vector<TrackSnap> out;
    auto& bus = SharedBus::get();
    const double now = juce::Time::getMillisecondCounterHiRes();

    for (int i = 0; i < SharedBus::maxSlots; ++i)
    {
        auto* s = bus.slot (i);
        if (s == nullptr || ! s->inUse.load())
            continue;

        TrackSnap t;
        t.slotIndex     = i;
        t.name          = s->getName();
        t.role          = (Role) juce::jlimit (0, (int) Role::NumRoles - 1, s->role.load());
        const auto argb = s->colourARGB.load();
        t.colour        = argb != 0 ? juce::Colour (argb) : roleColour (t.role);
        t.peakDb        = s->peakDb.load();
        t.rmsDb         = s->rmsDb.load();
        t.momentaryLufs = s->momentaryLufs.load();
        t.shortLufs     = s->shortLufs.load();
        t.maxPeakDb     = s->maxPeakDb.load();
        t.correlation   = s->correlation.load();
        t.lowRatio      = s->lowRatio.load();
        t.lowSideRatio  = s->lowSideRatio.load();
        t.crestDb       = s->crestDb.load();
        t.clipCount     = s->clipCount.load();
        t.channels      = s->channels.load();
        t.active        = (now - s->lastActiveMs.load()) < 3000.0;
        t.isSelf        = (i == selfSlot);
        out.push_back (t);
    }

    // Master on the right, everything else ordered by role then slot
    std::stable_sort (out.begin(), out.end(), [] (const TrackSnap& a, const TrackSnap& b)
    {
        auto key = [] (const TrackSnap& t) { return t.role == Role::Master ? 1000 : (t.role == Role::Auto ? 999 : (int) t.role); };
        if (key (a) != key (b)) return key (a) < key (b);
        return a.slotIndex < b.slotIndex;
    });
    return out;
}

// ---------------------------------------------------------------------------
namespace
{
    struct Ctx
    {
        const std::vector<TrackSnap>& tracks;
        std::vector<Tip> tips;

        // the loudest *playing* track with this role
        const TrackSnap* loudest (Role r) const
        {
            const TrackSnap* best = nullptr;
            for (auto& t : tracks)
                if (t.role == r && t.active && t.shortLufs > -60.0f)
                    if (best == nullptr || t.shortLufs > best->shortLufs)
                        best = &t;
            return best;
        }

        void add (Severity s, juce::String title, juce::String detail, const TrackSnap* t = nullptr)
        {
            Tip tip;
            tip.severity  = s;
            tip.title     = std::move (title);
            tip.detail    = std::move (detail);
            tip.slotIndex = t != nullptr ? t->slotIndex : -1;
            tip.accent    = t != nullptr ? roleColour (t->role) : juce::Colour (0xffb44dff);
            tips.push_back (std::move (tip));
        }
    };
}

std::vector<Tip> buildAdvice (const std::vector<TrackSnap>& tracks)
{
    Ctx c { tracks, {} };

    int activeCount = 0;
    const TrackSnap* master = nullptr;
    for (auto& t : tracks)
    {
        if (t.active) ++activeCount;
        if (t.role == Role::Master && master == nullptr) master = &t;
    }

    if (activeCount == 0)
    {
        c.add (Severity::Info, "Hit play",
               "Drop DRIPMIX on every track you care about (kick, 808, vox, hats, melody) plus your Master, "
               "then play the loudest part of the song. The coach listens to all of them at once.");
        return c.tips;
    }

    // ---- 1. Clipping ----------------------------------------------------------
    for (auto& t : tracks)
    {
        if (t.maxPeakDb <= 0.0f)
            continue;

        const float over = t.maxPeakDb;
        const float drop = std::ceil (over + 1.0f);

        if (t.role == Role::Master)
            c.add (Severity::Critical, "MASTER IS CLIPPING (" + db (over) + ")",
                   "Your mix is going over 0 dBFS. Select all your track faders and pull them down about "
                   + juce::String ((int) drop + 5) + " dB together (or drop a Utility on your groups). "
                   "Turning the master fader down won't fix it - DRIPMIX on the master reads before the fader. "
                   "Aim for peaks around -6 dBFS so mastering has room.", &t);
        else
            c.add (Severity::Critical, displayName (t) + " is clipping (" + db (over) + ")",
                   "Bring it down at least " + juce::String ((int) drop) + " dB with the clip Gain knob or a Utility placed BEFORE DRIPMIX "
                   "(Ableton's track fader comes after plugins, so I read pre-fader). If you WANT that clipped sound, "
                   "use a soft clipper (Saturator > Soft Sine, or a clipper plugin) instead of slamming 0 dB. Click the clip light to reset.", &t);
    }

    // ---- 2. Master headroom + loudness ---------------------------------------
    if (master == nullptr)
    {
        c.add (Severity::Info, "Put DRIPMIX on your Master too",
               "With one on the Master I can check total headroom, mix loudness and mono compatibility.");
    }
    else if (master->active && master->maxPeakDb <= 0.0f)
    {
        if (master->maxPeakDb > -3.0f)
            c.add (Severity::Warn, "Mix is running hot (" + db (master->maxPeakDb) + " peak)",
                   "Leave headroom for mastering: bring everything down ~" + juce::String ((int) std::ceil (master->maxPeakDb + 6.0f))
                   + " dB so the master peaks around -6 dBFS. Mastering pushes it loud later, not the mix.", master);
        else if (master->maxPeakDb < -18.0f)
            c.add (Severity::Info, "Mix is pretty quiet (" + db (master->maxPeakDb) + " peak)",
                   "No damage, but you can bring the whole mix up a bit - peaks around -6 dBFS is the sweet spot before mastering.", master);
        else if (master->maxPeakDb >= -8.0f && master->maxPeakDb <= -3.0f)
            c.add (Severity::Good, "Master headroom is clean (" + db (master->maxPeakDb) + " peak)",
                   "Perfect zone for sending to mastering. Final trap masters usually land around -8 to -10 LUFS with peaks under -1 dB true peak.", master);

        if (master->shortLufs > -9.0f && master->crestDb < 7.0f && master->crestDb > 0.1f)
            c.add (Severity::Warn, "Mix bus is getting crushed",
                   "Short-term loudness is " + juce::String (master->shortLufs, 1) + " LUFS with only " + juce::String (master->crestDb, 1)
                   + " dB of punch left. Take the limiter/clipper off the master while mixing so your kick and 808 still hit.", master);

        if (master->channels >= 2 && master->correlation < 0.2f)
            c.add (Severity::Warn, "Mix has phase issues",
                   "Stereo correlation is " + juce::String (master->correlation, 2) + ". Check mono (Utility > Mono on the master): "
                   "wide stereo FX or a stereo 808 can disappear on phones and club systems.", master);
    }

    // ---- 3. Balance -----------------------------------------------------------
    const auto* kick  = c.loudest (Role::Kick);
    const auto* b808  = c.loudest (Role::Bass808);
    const auto* vox   = c.loudest (Role::LeadVocal);
    const auto* adlib = c.loudest (Role::AdLibs);
    const auto* hats  = c.loudest (Role::HiHats);
    const auto* snare = c.loudest (Role::SnareClap);
    const auto* mel   = c.loudest (Role::Melody);
    const auto* pads  = c.loudest (Role::PadKeys);

    if (kick != nullptr && b808 != nullptr)
    {
        const float d = b808->shortLufs - kick->shortLufs; // 808 sustains so it normally measures louder
        if (d > 9.0f)
            c.add (Severity::Warn, "808 is burying the kick",
                   "The 808 is " + dbAbs (d) + " louder than the kick. Pull the 808 down 2-3 dB, or sidechain it to the kick "
                   "(Compressor > Sidechain from the kick track) so they hit as one.", b808);
        else if (d < -3.0f)
            c.add (Severity::Warn, "808 is weak next to the kick",
                   "The kick is " + dbAbs (d) + " louder. Bring the 808 up or add Saturator/distortion so it reads on phone speakers "
                   "- saturation makes it feel louder without eating headroom.", b808);
        else
            c.add (Severity::Good, "Kick + 808 balance is solid",
                   "They're sitting in a good relationship (" + db (d) + "). Make sure they don't fight: tune the kick to the 808's key "
                   "or keep the kick short.", b808);
    }

    if (vox != nullptr)
    {
        const TrackSnap* beatTop = nullptr;
        for (auto* t : { b808, mel, pads, kick })
            if (t != nullptr && (beatTop == nullptr || t->shortLufs > beatTop->shortLufs))
                beatTop = t;

        if (beatTop != nullptr)
        {
            const float d = vox->shortLufs - beatTop->shortLufs;
            if (d < -4.0f)
                c.add (Severity::Warn, "Vocal is buried",
                       "The " + displayName (*beatTop) + " is " + dbAbs (d) + " louder than the lead vocal. The vocal is the star in trap: "
                       "bring it up or turn the beat down, and EQ a little dip around 1-3 kHz on the melody to make room.", vox);
            else if (d > 8.0f)
                c.add (Severity::Info, "Vocal is sitting way on top",
                       "The lead is " + dbAbs (d) + " above the loudest beat element. If the beat feels small, bring the 808 and drums up a bit.", vox);
        }

        if (mel != nullptr && mel->shortLufs > vox->shortLufs - 2.0f)
            c.add (Severity::Warn, "Melody is fighting the vocal",
                   displayName (*mel) + " is almost as loud as the vocal. Turn it down 2-4 dB or sidechain it to the vocal so it ducks when you rap.", mel);

        if (adlib != nullptr && adlib->shortLufs > vox->shortLufs - 2.0f)
            c.add (Severity::Warn, "Ad-libs are too loud",
                   "Ad-libs should sit roughly 4-8 dB under the lead. Pull " + displayName (*adlib) + " down and pan or widen it so the lead stays clear.", adlib);

        if (hats != nullptr && hats->shortLufs > vox->shortLufs - 5.0f)
            c.add (Severity::Warn, "Hi-hats are too loud",
                   "Hats are close to the vocal level. Drop them 3-5 dB - in trap the rolls should be felt more than heard. "
                   "A gentle high-shelf cut tames harshness.", hats);
    }
    else if (hats != nullptr && kick != nullptr && hats->shortLufs > kick->shortLufs + 2.0f)
    {
        c.add (Severity::Warn, "Hi-hats are louder than the kick",
               "Pull the hats down so the kick and 808 drive the beat.", hats);
    }

    if (snare != nullptr && kick != nullptr)
    {
        const float d = snare->shortLufs - kick->shortLufs;
        if (d > 5.0f)
            c.add (Severity::Info, "Snare/clap is loud",
                   "The snare is " + dbAbs (d) + " over the kick. If it's poking out, pull it down 2-3 dB.", snare);
        else if (d < -8.0f)
            c.add (Severity::Info, "Snare/clap is getting lost",
                   "The snare is " + dbAbs (d) + " under the kick. Bring it up so the backbeat cracks.", snare);
    }

    // ---- 4. Low end + stereo --------------------------------------------------
    int lowCrowd = 0;
    for (auto& t : tracks)
    {
        if (! t.active || t.shortLufs < -60.0f)
            continue;

        const bool isLowRole = (t.role == Role::Kick || t.role == Role::Bass808 || t.role == Role::Master || t.role == Role::Bus);

        if (t.role == Role::Bass808 && t.channels >= 2 && (t.lowSideRatio > 0.12f || t.correlation < 0.6f))
            c.add (Severity::Warn, displayName (t) + " low end is stereo",
                   "Keep the 808 mono so it hits the same everywhere: Ableton's Utility has Bass Mono - set it around 120 Hz.", &t);

        if (! isLowRole && t.lowRatio > 0.25f)
            ++lowCrowd;

        if (t.role == Role::LeadVocal || t.role == Role::AdLibs)
        {
            if (t.lowRatio > 0.08f)
                c.add (Severity::Warn, displayName (t) + " has low-end mud",
                       "High-pass the vocal around 80-100 Hz (EQ Eight, low cut) so it doesn't fight the kick and 808.", &t);

            if (t.crestDb > 22.0f)
                c.add (Severity::Info, displayName (t) + " is jumping around",
                       "Big level swings (" + juce::String (t.crestDb, 0) + " dB peak-to-average). Compress it: around 3-4:1, "
                       "3-6 dB of gain reduction, then a second light compressor if needed.", &t);
            else if (t.crestDb > 0.1f && t.crestDb < 8.0f)
                c.add (Severity::Info, displayName (t) + " sounds squashed",
                       "Only " + juce::String (t.crestDb, 0) + " dB of dynamics left. Back off the compression/limiting a little so it breathes.", &t);
        }

        if (t.role == Role::HiHats && t.lowRatio > 0.04f)
            c.add (Severity::Info, displayName (t) + " has extra low end",
                   "High-pass the hats around 250-400 Hz - there's nothing useful down there and it clouds the 808.", &t);

        if ((t.role == Role::Melody || t.role == Role::PadKeys) && t.lowRatio > 0.30f)
            c.add (Severity::Warn, displayName (t) + " is crowding the 808",
                   "Lots of energy under 120 Hz. High-pass it around 100-150 Hz so the 808 owns the sub.", &t);

        if (t.role == Role::Kick && t.crestDb > 0.1f && t.crestDb < 8.0f)
            c.add (Severity::Info, "Kick is losing its punch",
                   "Only " + juce::String (t.crestDb, 0) + " dB of transient. Ease off compression on the kick or add a transient shaper.", &t);

        // Gain staging
        if (t.role != Role::Master && t.peakDb < -36.0f && t.maxPeakDb < -30.0f)
            c.add (Severity::Info, displayName (t) + " is coming in really low",
                   "Peaks under -30 dBFS. Raise its clip gain / Utility gain so your plugins see a healthy level (around -18 dBFS average).", &t);
        else if (t.role != Role::Master && t.role != Role::Bus && t.rmsDb > -6.0f)
            c.add (Severity::Warn, displayName (t) + " is too hot on its own",
                   "Average level is " + db (t.rmsDb) + " on a single track. Gain-stage it down to about -18 to -12 dBFS with clip Gain or a Utility before DRIPMIX - "
                   "otherwise the master runs out of room once everything plays.", &t);
    }

    if (lowCrowd >= 3)
        c.add (Severity::Warn, "Too many sounds in the sub",
               juce::String (lowCrowd) + " non-bass tracks have heavy low end. High-pass everything that isn't the kick or 808.");

    // ---- 5. Unassigned --------------------------------------------------------
    for (auto& t : tracks)
        if (t.role == Role::Auto && t.active)
            c.add (Severity::Info, "Tell me what " + displayName (t) + " is",
                   "Pick a role (Kick, 808, Vocal...) in that DRIPMIX's dropdown so I can coach it.", &t);

    // ---- 6. All good? ---------------------------------------------------------
    bool anyProblem = false;
    for (auto& tip : c.tips)
        if (tip.severity >= Severity::Warn)
            anyProblem = true;

    if (! anyProblem)
        c.add (Severity::Good, "No major problems",
               "Levels look healthy. Now check it on earbuds, a phone and the car - the 808 is the thing most likely to translate weird.");

    std::stable_sort (c.tips.begin(), c.tips.end(), [] (const Tip& a, const Tip& b)
    {
        return (int) a.severity > (int) b.severity;
    });

    return c.tips;
}

} // namespace drip
