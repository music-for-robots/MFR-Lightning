// MFR-Lightning - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Early-'80s thrash rhythm rig, run 4x oversampled:
//
//   guitar -> gate -> [green overdrive] -> master-volume British head -> 4x12 -> plate reverb -> L/R
//
// The overdrive is a mid-hump op-amp design: it only clips what's above ~720 Hz,
// so with drive low and level high it tightens and pushes an already-distorted
// amp rather than adding fizz. The head is a cascaded-preamp master-volume
// design with the classic passive tone stack (circuit model, double precision)
// and an EL34-style power stage. The reverb is a small plate-style FDN.
//
// Pure C++ (no JUCE).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace light
{

constexpr double kPi = 3.14159265358979323846;
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
    void lowpass (double fs, double f, double q)
    { const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q); set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
    void highpass (double fs, double f, double q)
    { const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q); set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
    void peaking (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowshelf (double fs, double f, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos (w), s = std::sqrt (A) * std::sin (w) * std::sqrt (2.0);
        set (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
             (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
    }
    void highshelf (double fs, double f, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos (w), s = std::sqrt (A) * std::sin (w) * std::sqrt (2.0);
        set (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
             (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
    }
private:
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    { b0 = float (B0 / A0); b1 = float (B1 / A0); b2 = float (B2 / A0); a1 = float (A1 / A0); a2 = float (A2 / A0); }
};

struct OnePole
{
    float a = 0, z = 0;
    void setCutoff (double fs, double f) { a = float (1 - std::exp (-2 * kPi * f / fs)); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { z += a * (x - z); return x - z; }
    void reset() { z = 0; }
};

//==============================================================================
// Passive treble/middle/bass network (Yeh & Smith 2006 analog model, bilinear),
// with the British head's component values. Double precision: at 4x oversampling
// the poles sit very close to z = 1, and single precision goes unstable.
class ToneStack
{
public:
    void set (double fs, double treble, double middle, double bass)
    {
        const double R1 = 220e3, R2 = 1e6, R3 = 22e3, R4 = 33e3;      // treble, bass, middle pots; slope resistor
        const double C1 = 470e-12, C2 = 22e-9, C3 = 22e-9;
        const double t = std::clamp (treble, 0.0, 1.0), m = std::clamp (middle, 0.0, 1.0);
        const double l = std::exp ((std::clamp (bass, 0.0, 1.0) - 1.0) * 3.4);

        const double b1 = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
        const double b2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
                        + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                        + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
        const double b3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                        - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4
                        + t * l * C1 * C2 * C3 * R1 * R2 * R4;
        const double a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
        const double a2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                        - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
                        + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
        const double a3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                        - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
                        + l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;

        const double c = 2 * fs, c2 = c * c, c3 = c2 * c;
        const double B0 = -b1 * c - b2 * c2 - b3 * c3, B1 = -b1 * c + b2 * c2 + 3 * b3 * c3;
        const double B2 = b1 * c + b2 * c2 - 3 * b3 * c3, B3 = b1 * c - b2 * c2 + b3 * c3;
        const double A0 = -1 - a1 * c - a2 * c2 - a3 * c3, A1 = -3 - a1 * c + a2 * c2 + 3 * a3 * c3;
        const double A2 = -3 + a1 * c + a2 * c2 - 3 * a3 * c3, A3 = -1 + a1 * c - a2 * c2 + a3 * c3;
        nb0 = B0 / A0; nb1 = B1 / A0; nb2 = B2 / A0; nb3 = B3 / A0;
        na1 = A1 / A0; na2 = A2 / A0; na3 = A3 / A0;
    }
    float process (float in)
    {
        const double x = in;
        const double y = nb0 * x + s1;
        s1 = nb1 * x - na1 * y + s2;
        s2 = nb2 * x - na2 * y + s3;
        s3 = nb3 * x - na3 * y;
        if (! std::isfinite (y)) { reset(); return 0.0f; }
        return (float) y;
    }
    void reset() { s1 = s2 = s3 = 0; }
private:
    double nb0 = 0, nb1 = 0, nb2 = 0, nb3 = 0, na1 = 0, na2 = 0, na3 = 0, s1 = 0, s2 = 0, s3 = 0;
};

//==============================================================================
// Small plate-style reverb: 8-line feedback delay network, Householder mixing,
// damped feedback, stereo taps.
class Plate
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        static const double ms[kLines] = { 29.7, 37.1, 41.1, 43.7, 53.9, 61.3, 67.9, 79.3 };
        for (int i = 0; i < kLines; ++i)
        {
            lines[(size_t) i].assign ((size_t) (ms[i] * 0.001 * fs) + 1, 0.0f);
            pos[(size_t) i] = 0;
            damp[(size_t) i].setCutoff (fs, 4500);
        }
        pre.assign ((size_t) (0.015 * fs) + 1, 0.0f);
        prePos = 0;
        inHP.setCutoff (fs, 200);
        setDecay (1.8);
        reset();
    }
    void reset()
    {
        for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
        std::fill (pre.begin(), pre.end(), 0.0f);
        for (auto& d : damp) d.reset();
        inHP.reset();
    }
    void setDecay (double rt60)
    {
        for (int i = 0; i < kLines; ++i)
            gain[(size_t) i] = (float) std::pow (10.0, -3.0 * (double) lines[(size_t) i].size() / (rt60 * fs));
    }
    void process (float x, float& outL, float& outR)
    {
        // Pre-delay, and keep the low end out of the tail.
        const float in = pre[(size_t) prePos];
        pre[(size_t) prePos] = inHP.hp (x);
        if (++prePos == (int) pre.size()) prePos = 0;

        std::array<float, kLines> o {};
        float sum = 0;
        for (int i = 0; i < kLines; ++i) { o[(size_t) i] = lines[(size_t) i][(size_t) pos[(size_t) i]]; sum += o[(size_t) i]; }
        const float h = sum * (2.0f / kLines);   // Householder: v - 2/N * sum
        for (int i = 0; i < kLines; ++i)
        {
            const float fb = damp[(size_t) i].lp ((o[(size_t) i] - h) * gain[(size_t) i]) + in * 0.35f;
            lines[(size_t) i][(size_t) pos[(size_t) i]] = fb;
            if (++pos[(size_t) i] == (int) lines[(size_t) i].size()) pos[(size_t) i] = 0;
        }
        outL = o[0] - o[2] + o[4] - o[6];
        outR = o[1] - o[3] + o[5] - o[7];
    }
private:
    static constexpr int kLines = 8;
    double fs = 192000;
    std::array<std::vector<float>, kLines> lines;
    std::array<int, kLines> pos {};
    std::array<float, kLines> gain {};
    std::array<OnePole, kLines> damp;
    std::vector<float> pre;
    int prePos = 0;
    OnePole inHP;
};

//==============================================================================
struct Params
{
    float gateDb = -65;                          // -90 = off
    bool  odOn = true;
    float odDrive = 0.2f, odTone = 0.5f, odLevel = 0.8f;   // drive low, level high
    bool  ampOn = true;
    float preamp = 7.0f, master = 6.0f;          // 0..10
    float bass = 9.0f, middle = 1.0f, treble = 3.0f, presence = 7.0f;   // 0..10, fitted to the reference: scooped
    float reverb = 0.18f;                        // 0..1 wet
    float outputDb = -6;
};

class Chain
{
public:
    void prepare (double oversampledRate)
    {
        fs = oversampledRate;
        gateDecay = float (std::exp (-1.0 / (0.03 * fs)));
        gateOpen = float (1 - std::exp (-1.0 / (0.0005 * fs)));
        gateClose = float (1 - std::exp (-1.0 / (0.06 * fs)));
        odIn.setCutoff (fs, 30); odHump.setCutoff (fs, 720); odLP.lowpass (fs, 6000, 0.7); odTreble.lowpass (fs, 3200, 0.6);
        couple1.setCutoff (fs, 30); couple2.setCutoff (fs, 25); couple3.setCutoff (fs, 20);
        tight.highpass (fs, 70, 0.7);
        cabHP.highpass (fs, 70, 0.8); cabThump.peaking (fs, 115, 1.3, 3.0); cabBite.peaking (fs, 2800, 1.2, 4.0);
        cabLP1.lowpass (fs, 6800, 0.8); cabLP2.lowpass (fs, 9000, 0.6);
        resonance.lowshelf (fs, 100, 2.0);
        plate.prepare (fs);
        setParams (params);
        reset();
    }
    void reset()
    {
        env = 0; gate = 0;
        for (auto* o : { &odIn, &odHump, &couple1, &couple2, &couple3 }) o->reset();
        for (auto* b : { &odLP, &odTreble, &odTone, &bright, &tight, &presence, &resonance, &cabHP, &cabThump, &cabBite, &cabLP1, &cabLP2 }) b->reset();
        stack.reset(); plate.reset();
    }
    void setParams (const Params& p)
    {
        params = p;
        gateOn = p.gateDb > -89.5f;
        gateThresh = dbToGain (p.gateDb);
        odGain = 1.0f + (51e3f + p.odDrive * 500e3f) / 4.7e3f - 1.0f;   // gain above the hump corner, minus the unity path
        odTone.highshelf (fs, 1200, (p.odTone - 0.5f) * 14.0f);
        odOut = dbToGain (-20.0f + 30.0f * p.odLevel);
        const float pv = std::clamp (p.preamp / 10.0f, 0.0f, 1.0f);
        preGain = 1.0f + 60.0f * pv * pv;
        bright.highshelf (fs, 2500, 6.0 * (1.0 - pv));                  // bright cap: fades out as preamp goes up
        stack.set (fs, p.treble / 10.0, p.middle / 10.0, p.bass / 10.0);
        const float mv = std::clamp (p.master / 10.0f, 0.0f, 1.0f);
        masterGain = 0.3f + 4.0f * mv * mv;
        presence.highshelf (fs, 3500, (p.presence - 5.0f) * 1.6f);
        outGain = dbToGain (p.outputDb);
    }

    void process (float x, float& outL, float& outR)
    {
        if (gateOn)
        {
            env = std::max (std::abs (x), env * gateDecay);
            const float target = env > gateThresh ? 1.0f : 0.0f;
            gate += (target > gate ? gateOpen : gateClose) * (target - gate);
            x *= gate;
        }

        x = odIn.hp (x);
        if (params.odOn)
        {
            // Only what's above the ~720 Hz corner gets the gain and the diode clip.
            float v = odHump.hp (x) * odGain;
            v = 0.6f * v / std::sqrt (1.0f + (v / 0.6f) * (v / 0.6f));
            x = odTone.process (odTreble.process (odLP.process (x + v))) * odOut;
        }

        float y;
        if (params.ampOn)
        {
            // Cascaded preamp: a bright first stage, then a cold-biased second one.
            float a = bright.process (tight.process (x)) * preGain;
            a = a > 0 ? std::tanh (1.3f * a) / 1.3f : std::tanh (0.9f * a) / 0.9f;
            a = couple1.hp (a) * 6.0f;
            a = a > 0 ? std::tanh (0.7f * a) / 0.7f : std::tanh (2.2f * a) / 2.2f;   // cold stage clips the bottom hard
            a = couple2.hp (a);
            a = stack.process (a) * 3.0f;
            a = couple3.hp (a) * masterGain;
            a = std::tanh (a);                                                        // EL34 push-pull
            a = resonance.process (presence.process (a));
            y = cabLP2.process (cabLP1.process (cabBite.process (cabThump.process (cabHP.process (a)))));
        }
        else
            y = x * 0.5f;

        float rl, rr;
        plate.process (y, rl, rr);
        const float wet = params.reverb * 0.6f;
        outL = (y + wet * rl) * outGain * 0.42f;
        outR = (y + wet * rr) * outGain * 0.42f;
        if (! std::isfinite (outL) || ! std::isfinite (outR)) { reset(); outL = outR = 0; }
    }

    float getGate() const { return gateOn ? gate : 1.0f; }

private:
    double fs = 192000;
    Params params;
    bool gateOn = true;
    float env = 0, gate = 0, gateThresh = 0, gateDecay = 0, gateOpen = 0, gateClose = 0;
    OnePole odIn, odHump, couple1, couple2, couple3;
    Biquad odLP, odTreble, odTone, bright, tight, presence, resonance, cabHP, cabThump, cabBite, cabLP1, cabLP2;
    ToneStack stack;
    Plate plate;
    float odGain = 20, odOut = 1, preGain = 30, masterGain = 1, outGain = 0.5f;
};

} // namespace light
