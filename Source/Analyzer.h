#pragma once

#include "SharedBus.h"
#include <cmath>

// ============================================================================
//  Analyzer: runs on the audio thread. No allocation, no locks.
//  Measures peak, RMS, LUFS (ITU-R BS.1770 K-weighting), clipping,
//  stereo correlation, low-end content and crest factor.
// ============================================================================
namespace drip
{

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0; }

    void setLowPass (double fs, double f0, double q) noexcept
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = ((1.0 - cw) * 0.5) / a0;
        b1 = (1.0 - cw) / a0;
        b2 = b0;
        a1 = (-2.0 * cw) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    // BS.1770 stage 1 (high shelf), sample-rate independent form
    void setKShelf (double fs) noexcept
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K  = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        b0 = (Vh + Vb * K / Q + K * K) / a0;
        b1 = 2.0 * (K * K - Vh) / a0;
        b2 = (Vh - Vb * K / Q + K * K) / a0;
        a1 = 2.0 * (K * K - 1.0) / a0;
        a2 = (1.0 - K / Q + K * K) / a0;
    }

    // BS.1770 stage 2 (RLB high-pass)
    void setKHighPass (double fs) noexcept
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K  = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        b0 = 1.0; b1 = -2.0; b2 = 1.0;
        a1 = 2.0 * (K * K - 1.0) / a0;
        a2 = (1.0 - K / Q + K * K) / a0;
    }
};

inline float toDb (double lin, float floorDb = -100.0f) noexcept
{
    return lin > 1.0e-10 ? juce::jmax (floorDb, (float) (20.0 * std::log10 (lin))) : floorDb;
}

class Analyzer
{
public:
    static constexpr int numShortBlocks = 30; // 30 x 100ms = 3s
    static constexpr int numMomentaryBlocks = 4; // 400ms

    void prepare (double sampleRate)
    {
        fs = sampleRate > 0 ? sampleRate : 44100.0;
        blockLen = juce::jmax (1, (int) std::round (fs * 0.1));

        for (int c = 0; c < 2; ++c)
        {
            kShelf[c].setKShelf (fs);   kShelf[c].reset();
            kHp[c].setKHighPass (fs);   kHp[c].reset();
        }
        lowMid.setLowPass (fs, 120.0, 0.7071);  lowMid.reset();
        lowSide.setLowPass (fs, 120.0, 0.7071); lowSide.reset();

        rmsCoeff     = std::exp (-1.0 / (0.3 * fs));          // 300ms
        peakDecayPerSample = 20.0 / fs;                       // 20 dB/s release on the decaying peak
        resetAccumulators();
        std::fill (std::begin (kBlocks), std::end (kBlocks), 0.0);
        kBlockWrite = 0;
        kBlocksFilled = 0;
        meanSq = 0.0;
        decayPeakDb = -100.0;
        peakWindowLin = 0.0;
        peakWindowCount = 0;
        crestSmoothed = 0.0;
        corrSmoothed = 1.0;
        lowRatioSmoothed = 0.0;
        lowSideSmoothed = 0.0;
        maxPeakLin = 0.0;
        clipCount = 0;
    }

    // Measure the buffer (does not modify it) and publish to the slot.
    void process (const juce::AudioBuffer<float>& buffer, int numChannels, Slot& slot) noexcept
    {
        const int n = buffer.getNumSamples();
        if (n <= 0 || numChannels <= 0)
            return;

        if (slot.resetRequest.exchange (false))
        {
            maxPeakLin = 0.0;
            clipCount = 0;
        }

        const bool stereo = numChannels >= 2;
        const float* L = buffer.getReadPointer (0);
        const float* R = stereo ? buffer.getReadPointer (1) : L;

        double blockPeak = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double l = L[i];
            const double r = R[i];
            const double al = std::abs (l), ar = std::abs (r);
            const double pk = juce::jmax (al, ar);

            if (pk > blockPeak) blockPeak = pk;
            if (al > 1.0) ++clipCount;
            if (stereo && ar > 1.0) ++clipCount;

            // RMS (power averaged across channels)
            const double sq = stereo ? 0.5 * (l * l + r * r) : l * l;
            meanSq = rmsCoeff * meanSq + (1.0 - rmsCoeff) * sq;

            // K-weighted energy (BS.1770 sums channels)
            const double kl = kHp[0].process (kShelf[0].process (l));
            double kEnergy = kl * kl;
            if (stereo)
            {
                const double kr = kHp[1].process (kShelf[1].process (r));
                kEnergy += kr * kr;
            }
            accK += kEnergy;

            // Low end + stereo
            const double mid  = stereo ? 0.5 * (l + r) : l;
            const double side = stereo ? 0.5 * (l - r) : 0.0;
            const double lm = lowMid.process (mid);
            const double ls = lowSide.process (side);
            accTotal += mid * mid + side * side;
            accLowMid += lm * lm;
            accLowSide += ls * ls;
            accLR += l * r;
            accLL += l * l;
            accRR += r * r;

            if (++accCount >= blockLen)
                finishBlock (stereo);
        }

        // Decaying peak (instant attack)
        const double blockPeakDb = toDb (blockPeak);
        decayPeakDb = juce::jmax (blockPeakDb, decayPeakDb - peakDecayPerSample * n);
        maxPeakLin = juce::jmax (maxPeakLin, blockPeak);

        // 1s window peak for crest factor
        peakWindowLin = juce::jmax (peakWindowLin, blockPeak);
        peakWindowCount += n;
        if (peakWindowCount >= (int) fs)
        {
            lastWindowPeak = peakWindowLin;
            peakWindowLin = 0.0;
            peakWindowCount = 0;
        }

        const float rms = toDb (std::sqrt (meanSq));
        const double nowMs = juce::Time::getMillisecondCounterHiRes();

        slot.peakDb    = (float) decayPeakDb;
        slot.rmsDb     = rms;
        slot.maxPeakDb = toDb (maxPeakLin);
        slot.clipCount = clipCount;
        slot.channels  = numChannels;

        if (blockPeak > 0.0003) // about -70 dBFS: counts as "playing"
            slot.lastActiveMs = nowMs;

        slot.momentaryLufs = momentaryLufs;
        slot.shortLufs     = shortLufs;
        slot.correlation   = (float) corrSmoothed;
        slot.lowRatio      = (float) lowRatioSmoothed;
        slot.lowSideRatio  = (float) lowSideSmoothed;
        slot.crestDb       = (float) crestSmoothed;
    }

private:
    void finishBlock (bool stereo) noexcept
    {
        const double cnt = (double) accCount;
        kBlocks[kBlockWrite] = accK / cnt;
        kBlockWrite = (kBlockWrite + 1) % numShortBlocks;
        kBlocksFilled = juce::jmin (kBlocksFilled + 1, numShortBlocks);

        auto meanOfLast = [this] (int count) -> double
        {
            count = juce::jmin (count, kBlocksFilled);
            if (count <= 0) return 0.0;
            double sum = 0.0;
            for (int i = 1; i <= count; ++i)
                sum += kBlocks[(kBlockWrite - i + numShortBlocks) % numShortBlocks];
            return sum / count;
        };

        auto lufs = [] (double e) -> float
        {
            return e > 1.0e-12 ? juce::jmax (-100.0f, (float) (-0.691 + 10.0 * std::log10 (e))) : -100.0f;
        };

        momentaryLufs = lufs (meanOfLast (numMomentaryBlocks));
        shortLufs     = lufs (meanOfLast (numShortBlocks));

        // Only update spectral/stereo/crest stats while the track is actually playing
        const double meanTotal = accTotal / cnt;
        if (meanTotal > 1.0e-7) // ~ -70 dB
        {
            const double a = 0.25;
            const double lowE = accLowMid + accLowSide;
            lowRatioSmoothed += a * (juce::jlimit (0.0, 1.0, lowE / accTotal) - lowRatioSmoothed);

            if (lowE > accTotal * 0.02)
                lowSideSmoothed += a * (juce::jlimit (0.0, 1.0, accLowSide / lowE) - lowSideSmoothed);

            if (stereo && accLL > 0.0 && accRR > 0.0)
            {
                const double c = accLR / std::sqrt (accLL * accRR);
                corrSmoothed += a * (juce::jlimit (-1.0, 1.0, c) - corrSmoothed);
            }

            const double rmsBlock = std::sqrt (stereo ? 0.5 * (accLL + accRR) / cnt : accLL / cnt);
            const double pk = juce::jmax (lastWindowPeak, peakWindowLin);
            if (rmsBlock > 0.0 && pk > 0.0)
            {
                const double crest = 20.0 * std::log10 (pk / rmsBlock);
                crestSmoothed += 0.1 * (juce::jlimit (0.0, 40.0, crest) - crestSmoothed);
            }
        }

        resetAccumulators();
    }

    void resetAccumulators() noexcept
    {
        accK = accTotal = accLowMid = accLowSide = accLR = accLL = accRR = 0.0;
        accCount = 0;
    }

    double fs = 44100.0;
    int blockLen = 4410;
    Biquad kShelf[2], kHp[2], lowMid, lowSide;
    double rmsCoeff = 0.999, peakDecayPerSample = 0.0005;

    double accK = 0, accTotal = 0, accLowMid = 0, accLowSide = 0, accLR = 0, accLL = 0, accRR = 0;
    int accCount = 0;

    double kBlocks[numShortBlocks] {};
    int kBlockWrite = 0, kBlocksFilled = 0;

    double meanSq = 0.0, decayPeakDb = -100.0, maxPeakLin = 0.0;
    double peakWindowLin = 0.0, lastWindowPeak = 0.0;
    int peakWindowCount = 0;
    double crestSmoothed = 0.0, corrSmoothed = 1.0, lowRatioSmoothed = 0.0, lowSideSmoothed = 0.0;
    float momentaryLufs = -100.0f, shortLufs = -100.0f;
    int clipCount = 0;
};

} // namespace drip
