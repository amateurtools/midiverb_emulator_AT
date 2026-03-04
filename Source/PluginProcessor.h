#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>

// Forward declarations for LFO types (defined in lfo.h)
struct Lfo;
struct LfoPatch;

// Simple vibrato delay for feedback
static constexpr int maxVibratoDelaySamples = 256; // ~5–6 ms at 48 kHz



// Biquad filter section (transposed direct form II)
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0;
    double a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    double process(double x) {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    // Set as Butterworth lowpass for one section of an Nth order filter
    // Q values for Butterworth: Q = 1 / (2 * cos(pi * (2*k + 1) / (2*N)))
    void setLowpass(double sampleRate, double cutoff, double Q) {
        double w0 = 2.0 * 3.1415926 * cutoff / sampleRate;
        double cosw0 = std::cos(w0);
        double sinw0 = std::sin(w0);
        double alpha = sinw0 / (2.0 * Q);

        double a0 = 1.0 + alpha;
        b0 = ((1.0 - cosw0) / 2.0) / a0;
        b1 = (1.0 - cosw0) / a0;
        b2 = ((1.0 - cosw0) / 2.0) / a0;
        a1 = (-2.0 * cosw0) / a0;
        a2 = (1.0 - alpha) / a0;
    }
};

// 8th order Butterworth lowpass (4 cascaded biquads)
struct LowpassFilter8 {
    std::array<Biquad, 4> sections;

    void reset() {
        for (auto& s : sections) s.reset();
    }

    void setCutoff(double sampleRate, double cutoff) {
        // Q values for 8th order Butterworth
        // Q_k = 1 / (2 * cos(pi * (2*k + 1) / 16)) for k = 0,1,2,3
        static const double Qs[4] = {
            0.5097956518,  // k=0
            0.6013448869,  // k=1
            0.8999197654,  // k=2
            2.5629154478   // k=3
        };
        for (int i = 0; i < 4; i++) {
            sections[i].setLowpass(sampleRate, cutoff, Qs[i]);
        }
    }

    double process(double x) {
        for (auto& s : sections) {
            x = s.process(x);
        }
        return x;
    }
};

// Output interpolation buffer (linear interpolation)
// Used for upsampling: effect rate -> host rate
struct OutputInterpBuffer {
    static constexpr int SIZE = 2;
    std::array<double, SIZE> data = {};
    int writePos = 0;

    void reset() {
        data.fill(0);
        writePos = 0;
    }

    void push(double x) {
        data[writePos] = x;
        writePos = (writePos + 1) % SIZE;
    }

    // Linear interpolation between oldest and newest
    // frac=0 gives oldest, frac=1 gives newest
    double interpolate(double frac) const {
        int i0 = writePos;              // oldest
        int i1 = (writePos + 1) % SIZE; // newest
        return data[i0] + frac * (data[i1] - data[i0]);
    }
};

// Input interpolation buffer (linear interpolation)
// Used for downsampling: host rate -> effect rate
struct InputInterpBuffer {
    static constexpr int SIZE = 2;
    std::array<double, SIZE> data = {};
    int writePos = 0;

    void reset() {
        data.fill(0);
        writePos = 0;
    }

    void push(double x) {
        data[writePos] = x;
        writePos = (writePos + 1) % SIZE;
    }

    // Linear interpolation between 2nd newest and newest
    // frac=0 gives 2nd newest, frac=1 gives newest
    double interpolate(double frac) const {
        int i0 = writePos;              // 2nd newest (older)
        int i1 = (writePos + 1) % SIZE; // newest
        return data[i0] + frac * (data[i1] - data[i0]);
    }
};

class MidiverbAudioProcessor : public juce::AudioProcessor
{
public:
    MidiverbAudioProcessor();
    ~MidiverbAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Expose levels/clip for the GUI ----------------------------------------------------
    float getInputLevel()  const { return inputLevel.load  (std::memory_order_relaxed); }
    float getOutputLevel() const { return outputLevel.load (std::memory_order_relaxed); }

    bool getAndClearInputOverload()
    {
        return inputOverload.exchange (false, std::memory_order_relaxed);
    }

    bool getAndClearOutputOverload()
    {
        return outputOverload.exchange (false, std::memory_order_relaxed);
    }

    bool getClipLatched() const noexcept
    {
        return clipLatched.load (std::memory_order_relaxed);
    }
    //-------------------------------------------------------------------------------------

    static constexpr int NUM_DEVICES = 3;
    static constexpr int MAX_PROGRAMS = 100;  // MIDIVerb 2 has 100 programs (0-99)
    static constexpr double EFFECT_SAMPLE_RATE = 24000.0;
    static constexpr double FILTER_CUTOFF = 10000.0;  // Below 12kHz Nyquist

    // Device info accessors
    static const char* getDeviceName(int deviceIndex);
    static int getDeviceNumEffects(int deviceIndex);
    static int getDeviceDisplayOffset(int deviceIndex);
    static const char* getEffectName(int deviceIndex, int effectIndex);

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // filters before reverbs
    juce::dsp::IIR::Filter<float> hpfAL, hpfAR, lpfAL, lpfAR;
    juce::dsp::IIR::Filter<float> hpfBL, hpfBR, lpfBL, lpfBR;

    juce::AudioParameterFloat* preDelayAParam = nullptr;
    juce::AudioParameterFloat* preDelayBParam = nullptr;

    // Pre-delay buffers (stereo) and indices
    static constexpr int MAX_PREDELAY_MS      = 120;
    static constexpr int MAX_SAMPLE_RATE      = 96000;
    static constexpr int MAX_PREDELAY_SAMPLES = MAX_SAMPLE_RATE * MAX_PREDELAY_MS / 1000;

    juce::AudioBuffer<float> preDelayBufferA;
    juce::AudioBuffer<float> preDelayBufferB;
    int preDelayWritePosA = 0;
    int preDelayWritePosB = 0;

    // Effect state
    std::array<int16_t, 0x4000> DRAM_A{};
    std::array<int16_t, 0x4000> DRAM_B{};
    int memoryPointerA = 0;
    int memoryPointerB = 0;
    double lastWetAL = 0.0, lastWetAR = 0.0;
    double lastWetBL = 0.0, lastWetBR = 0.0;

    // Resampling state
    double hostSampleRate = 48000.0;
    double phaseIncrement = 1.0;  // 24000 / hostRate

    // Output interpolation buffers (effect outputs stereo)
    OutputInterpBuffer outputBufferAL, outputBufferAR;
    OutputInterpBuffer outputBufferBL, outputBufferBR;

    // A: downsample + upsample
    LowpassFilter8   antiAliasFilterA;
    InputInterpBuffer inputBufferA;
    double            phaseA = 0.0;
    LowpassFilter8    reconstructFilterAL;
    LowpassFilter8    reconstructFilterAR;

    // B: downsample + upsample
    LowpassFilter8   antiAliasFilterB;
    InputInterpBuffer inputBufferB;
    double            phaseB = 0.0;
    LowpassFilter8    reconstructFilterBL;
    LowpassFilter8    reconstructFilterBR;

    // LFO state...
    Lfo* lfo1A = nullptr;
    Lfo* lfo2A = nullptr;
    LfoPatch* currentLfoPatchA = nullptr;
    uint32_t lfo1ValueA = 0, lfo2ValueA = 0;
    int lfoSampleCounterA = 0;
    int lastDeviceIndexA = -1, lastProgramIndexA = -1;

    Lfo* lfo1B = nullptr;
    Lfo* lfo2B = nullptr;
    LfoPatch* currentLfoPatchB = nullptr;
    uint32_t lfo1ValueB = 0, lfo2ValueB = 0;
    int lfoSampleCounterB = 0;
    int lastDeviceIndexB = -1, lastProgramIndexB = -1;

    // Simple levels + clip flags for 3‑state LED
    std::atomic<float> inputLevel  { 0.0f };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<bool>  inputOverload  { false };
    std::atomic<bool>  outputOverload { false };

    // flame stuff
    std::atomic<bool> clipLatched    { false };   // 10s+ continuous clip
    double clipAccumSeconds = 0.0;               // audio-thread only

    static constexpr int LFO_UPDATE_INTERVAL = 8;
    
    struct VibratoDelay
    {
        float buffer[maxVibratoDelaySamples] {};
        int   writePos = 0;

        float lfoPhase   = 0.0f;   // 0..1
        float lfoRateHz  = 0.3f;   // slow movement
        float depthSamples = 4.0f; // peak deviation in samples (e.g. ±4)

        float sampleRate = 48000.0f;

        void prepare (double sr)
        {
            sampleRate = (float) sr;
            writePos   = 0;
            lfoPhase   = 0.0f;
            std::fill (std::begin (buffer), std::end (buffer), 0.0f);
        }

        void setParams (float rateHz, float depthSmps)
        {
            lfoRateHz    = rateHz;
            depthSamples = juce::jlimit (0.0f, (float) (maxVibratoDelaySamples - 2), depthSmps);
        }

        float process (float x)
        {
            // write input
            buffer[writePos] = x;

            // LFO
            float phaseInc = lfoRateHz / sampleRate;
            lfoPhase += phaseInc;
            if (lfoPhase >= 1.0f)
                lfoPhase -= 1.0f;

            // Sine LFO in [-1, 1]
            float lfo = std::sin (2.0f * juce::MathConstants<float>::pi * lfoPhase);

            // Base delay in the middle of the buffer, modulated +/- depth
            float baseDelay = 8.0f; // samples of static delay
            float delaySmps = baseDelay + lfo * depthSamples;

            delaySmps = juce::jlimit (1.0f, (float) (maxVibratoDelaySamples - 2), delaySmps);

            float readPosFloat = (float) writePos - delaySmps;
            while (readPosFloat < 0.0f)
                readPosFloat += maxVibratoDelaySamples;

            int   idx0 = (int) readPosFloat;
            int   idx1 = (idx0 + 1) % maxVibratoDelaySamples;
            float frac = readPosFloat - (float) idx0;

            float y = buffer[idx0] + frac * (buffer[idx1] - buffer[idx0]);

            // advance write pointer
            writePos = (writePos + 1) % maxVibratoDelaySamples;

            return y;
        }
    };

    // In your processor:
    VibratoDelay vibratoA, vibratoB;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiverbAudioProcessor)
};
