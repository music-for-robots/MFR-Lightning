// MFR-Lightning - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Offline checks (exit 1 on regressions, for CI):
//   1. tone stack response at noon
//   2. overdrive: the mid hump (much more gain at 1.5 kHz than at 150 Hz)
//   3. voicing: the dry rig against the measured balance of the reference rhythm guitars (--fit searches the EQ)
//   4. held chords: 8 s power chords must decay smoothly, never jump or cut out
//   5. gate: silence stays silent
//   6. full chain at defaults: finite, not clipping
// Writes lightning_demo.wav (stereo): a palm-muted gallop riff.
#include "../Source/LightningDSP.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

using namespace light;
constexpr double kFs = 48000.0, kOs = kFs * 4;
static bool gFailed = false;

// Target: 1/3-octave balance of the reference rhythm guitars (side channel, average of two long
// sections), dB re 1 kHz, 125 Hz .. 6.3 kHz.
static const double kBands[] = { 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300 };
static const double kTarget[] = { 0.2, 7.6, 1.6, 3.9, 1.4, 0.8, 0.3, -0.9, -2.2, 0.0, 1.1, 1.0, 0.8, 4.3, 4.6, 1.0, -1.4, -3.3 };
constexpr int kNumBands = 18;

struct Pluck
{
    std::vector<float> buf; size_t pos = 0; float damp;
    Pluck (double hz, float amp, uint32_t& rng, float damping) : buf ((size_t) std::lround (kOs / hz)), damp (damping)
    {
        for (auto& v : buf) { rng = rng * 1664525u + 1013904223u; v = amp * ((float) (rng >> 9) / 4194304.0f - 1.0f); }
        float lp = 0; for (auto& v : buf) { lp += 0.5f * (v - lp); v = lp; }
    }
    float tick() { const size_t nx = (pos + 1) % buf.size(); const float o = buf[pos]; buf[pos] = damp * 0.5f * (buf[pos] + buf[nx]); pos = nx; return o; }
};

// Palm-muted E5 gallops (eighth + two sixteenths) with chromatic power-chord moves, ~ 200 BPM.
static std::vector<float> gallopRiff (double seconds, float amp)
{
    std::vector<float> y ((size_t) (seconds * kOs), 0.0f);
    const double roots[] = { 82.41, 82.41, 82.41, 92.5, 82.41, 82.41, 98.0, 87.31 };   // E E E F# E E G F
    const double beat = 60.0 / 200.0;
    const double pattern[] = { 0.0, 0.5, 0.75 };    // gallop within a beat
    uint32_t rng = 3;
    int b = 0;
    for (double t = 0; t + beat <= seconds; t += beat, ++b)
        for (double off : pattern)
        {
            const double root = roots[(b / 2) % 8];
            const size_t start = (size_t) ((t + off * beat) * kOs);
            const size_t len = (size_t) ((off == 0.0 ? 0.5 : 0.25) * beat * kOs);
            for (double ratio : { 1.0, 1.5, 2.0 })
            {
                Pluck p (root * ratio, amp, rng, 0.985f);   // palm mute: fast damping
                for (size_t i = start; i < start + len && i < y.size(); ++i) y[i] += p.tick();
            }
        }
    return y;
}

static void run (const Params& p, const std::vector<float>& in, std::vector<float>& L, std::vector<float>& R)
{
    Chain c; c.prepare (kOs); c.setParams (p);
    L.resize (in.size()); R.resize (in.size());
    for (size_t i = 0; i < in.size(); ++i) c.process (in[i], L[i], R[i]);
}

static void bandLevels (const std::vector<float>& x, double* out)
{
    const int N = 16384;
    std::vector<double> acc (kNumBands, 0.0);
    for (size_t s = 0; s + N <= x.size(); s += N / 2)
        for (int b = 0; b < kNumBands; ++b)
        {
            const double lo = kBands[b] / std::pow (2.0, 1.0 / 6), hi = kBands[b] * std::pow (2.0, 1.0 / 6);
            for (double f = lo; f < hi; f += (hi - lo) / 12)
            {
                const double w = 2 * kPi * f / kOs, c = 2 * std::cos (w);
                double s1 = 0, s2 = 0;
                for (int i = 0; i < N; ++i)
                {
                    const double win = 0.5 - 0.5 * std::cos (2 * kPi * i / N);
                    const double s0 = x[s + (size_t) i] * win + c * s1 - s2; s2 = s1; s1 = s0;
                }
                acc[(size_t) b] += s1 * s1 + s2 * s2 - c * s1 * s2;
            }
        }
    const double ref = 10 * std::log10 (acc[9] + 1e-30);
    for (int b = 0; b < kNumBands; ++b) out[b] = 10 * std::log10 (acc[(size_t) b] + 1e-30) - ref;
}

static double fitError (const double* lv)
{
    double e = 0; for (int b = 0; b < kNumBands; ++b) { const double d = lv[b] - kTarget[b]; e += d * d; }
    return std::sqrt (e / kNumBands);
}

static void writeWavStereo (const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    const uint32_t bytes = (uint32_t) L.size() * 4;
    f.write ("RIFF", 4); u32 (36 + bytes); f.write ("WAVEfmt ", 8);
    u32 (16); u16 (1); u16 (2); u32 (48000); u32 (48000 * 4); u16 (4); u16 (16);
    f.write ("data", 4); u32 (bytes);
    for (size_t i = 0; i < L.size(); ++i)
        for (float s : { L[i], R[i] })
        { const auto v = (int16_t) std::lround (std::clamp (s, -1.0f, 1.0f) * 32767.0f); f.write ((const char*) &v, 2); }
}

int main (int argc, char** argv)
{
    const bool fit = argc > 1 && std::strcmp (argv[1], "--fit") == 0;

    // 1. Tone stack at noon.
    {
        ToneStack ts; ts.set (kOs, 0.5, 0.5, 0.5);
        std::printf ("tone stack at noon (dB):");
        for (double f : { 100.0, 250.0, 500.0, 1000.0, 2500.0, 5000.0 })
        {
            ts.reset(); double peak = 0;
            for (int i = 0; i < (int) kOs / 4; ++i)
            {
                const float y = ts.process ((float) std::sin (2 * kPi * f * i / kOs));
                if (i > (int) kOs / 8) peak = std::max (peak, (double) std::abs (y));
            }
            const double db = 20 * std::log10 (peak + 1e-12);
            std::printf ("  %.0f:%+.1f", f, db);
            if (! std::isfinite (db)) gFailed = true;
        }
        std::printf ("\n");
    }

    // 2. Overdrive mid hump (small signal, amp off, no reverb).
    {
        Params p; p.ampOn = false; p.reverb = 0; p.gateDb = -90; p.odDrive = 0.5f;
        auto gainAt = [&] (double f)
        {
            Chain c; c.prepare (kOs); c.setParams (p);
            double peak = 0;
            for (int i = 0; i < (int) kOs / 4; ++i)
            {
                float l, r; c.process (0.002f * (float) std::sin (2 * kPi * f * i / kOs), l, r);
                if (i > (int) kOs / 8) peak = std::max (peak, (double) std::abs (l));
            }
            return 20 * std::log10 (peak / 0.002 + 1e-12);
        };
        const double lo = gainAt (150), hi = gainAt (1500);
        std::printf ("overdrive hump: 150 Hz %+.1f dB, 1.5 kHz %+.1f dB (difference %.1f dB)\n", lo, hi, hi - lo);
        if (! (hi - lo > 10.0)) gFailed = true;
    }

    // 3. Voicing (dry: reverb off).
    const auto riff = gallopRiff (3.0, 0.1f);
    {
        Params p; p.reverb = 0;
        std::vector<float> L, R; run (p, riff, L, R);
        double lv[kNumBands]; bandLevels (L, lv);
        std::printf ("voicing at defaults vs reference (dB re 1 kHz):\n  band  ");
        for (int b = 0; b < kNumBands; ++b) std::printf ("%6.0f", kBands[b]);
        std::printf ("\n  ref   "); for (int b = 0; b < kNumBands; ++b) std::printf ("%+6.1f", kTarget[b]);
        std::printf ("\n  ours  "); for (int b = 0; b < kNumBands; ++b) std::printf ("%+6.1f", lv[b]);
        std::printf ("\n  RMS error: %.1f dB\n", fitError (lv));
        if (! fit && fitError (lv) > 3.0) gFailed = true;   // defaults were fitted to 1.9 dB
    }
    if (fit)
    {
        double best = 1e9; Params bp;
        const auto shortRiff = gallopRiff (1.2, 0.1f);
        for (float bs = 3; bs <= 9; bs += 2)
            for (float m = 1; m <= 7; m += 2)
                for (float t = 3; t <= 9; t += 2)
                    for (float pr = 3; pr <= 9; pr += 2)
                    {
                        Params p; p.reverb = 0; p.bass = bs; p.middle = m; p.treble = t; p.presence = pr;
                        std::vector<float> L, R; run (p, shortRiff, L, R);
                        double lv[kNumBands]; bandLevels (L, lv);
                        const double e = fitError (lv);
                        if (e < best) { best = e; bp = p; }
                    }
        std::printf ("best fit: bass %.0f middle %.0f treble %.0f presence %.0f -> RMS error %.1f dB\n",
                     bp.bass, bp.middle, bp.treble, bp.presence, best);
    }

    // 4. Held chords: one E5 strum left to ring for 8 s.
    for (bool od : { false, true })
    {
        std::vector<float> in ((size_t) (8.0 * kOs), 0.0f);
        uint32_t rng = 9;
        for (double ratio : { 1.0, 1.5, 2.0 })
        {
            Pluck p (82.41 * ratio, 0.1f, rng, 0.99995f);
            for (auto& v : in) v += p.tick();
        }
        Params p; p.odOn = od; p.reverb = 0; p.gateDb = -90;
        Chain c; c.prepare (kOs); c.setParams (p);
        const size_t win = (size_t) (0.25 * kOs);
        double prev = 0, worst = 0; bool ok = true;
        for (size_t s0 = 0; s0 + win <= in.size(); s0 += win)
        {
            double e = 0;
            for (size_t i = s0; i < s0 + win; ++i) { float l, r; c.process (in[i], l, r); e += (double) l * l; }
            const double db = 10 * std::log10 (e / win + 1e-30);
            if (! std::isfinite (db)) ok = false;
            if (s0 >= 2 * win) worst = std::max (worst, std::abs (db - prev));
            prev = db;
        }
        std::printf ("held E5, overdrive %s: biggest 0.25 s level step %.1f dB\n", od ? "on " : "off", worst);
        if (! ok || ! (worst < 3.0)) gFailed = true;
    }

    // 5. Gate.
    {
        Params p;
        std::vector<float> L, R; run (p, std::vector<float> ((size_t) kOs, 0.0f), L, R);
        double e = 0; for (size_t i = 0; i < L.size(); ++i) e += L[i] * L[i] + R[i] * R[i];
        const double db = 10 * std::log10 (e / L.size() + 1e-30);
        std::printf ("gate, silent input: %.1f dBFS\n", db);
        if (db > -90) gFailed = true;
    }

    // 6. Level and the demo (with reverb).
    {
        Params p;
        std::vector<float> L, R; run (p, gallopRiff (6.0, 0.1f), L, R);
        std::vector<float> dl, dr;
        double peak = 0; bool finite = true;
        float a1 = 0, a2 = 0, b1 = 0, b2 = 0;
        for (size_t i = 0; i < L.size(); ++i)
        {
            a1 += 0.25f * (L[i] - a1); a2 += 0.25f * (a1 - a2);
            b1 += 0.25f * (R[i] - b1); b2 += 0.25f * (b1 - b2);
            if (i % 4 == 3)
            {
                dl.push_back (a2); dr.push_back (b2);
                finite &= std::isfinite (a2) && std::isfinite (b2);
                peak = std::max ({ peak, (double) std::abs (a2), (double) std::abs (b2) });
            }
        }
        std::printf ("defaults: peak %.1f dBFS, %s\n", 20 * std::log10 (peak + 1e-12), finite ? "finite" : "NON-FINITE");
        if (! finite || peak > 1.0) gFailed = true;
        writeWavStereo ("lightning_demo.wav", dl, dr);
    }

    std::printf ("\n%s\n", gFailed ? "CHECKS FAILED" : "all checks passed");
    return gFailed ? 1 : 0;
}
