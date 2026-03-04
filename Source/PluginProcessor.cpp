#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

#include "Lfo.h"
#include "Effects.h"

const char* MidiverbAudioProcessor::getDeviceName(int deviceIndex)
{
    if (deviceIndex >= 0 && deviceIndex < NUM_DEVICES)
        return devices[deviceIndex].name;
    return "Unknown";
}

int MidiverbAudioProcessor::getDeviceNumEffects(int deviceIndex)
{
    if (deviceIndex >= 0 && deviceIndex < NUM_DEVICES)
        return devices[deviceIndex].numEffects;
    return 0;
}

int MidiverbAudioProcessor::getDeviceDisplayOffset(int deviceIndex)
{
    if (deviceIndex >= 0 && deviceIndex < NUM_DEVICES)
        return devices[deviceIndex].displayOffset;
    return 0;
}

const char* MidiverbAudioProcessor::getEffectName(int deviceIndex, int effectIndex)
{
    if (deviceIndex >= 0 && deviceIndex < NUM_DEVICES) {
        const auto& dev = devices[deviceIndex];
        if (effectIndex >= 0 && effectIndex < dev.numEffects)
            return dev.effectNames[effectIndex];
    }
    return "Unknown";
}

static inline float saturate(float x)
{
    if (x <= -1.0f) return -1.0f;
    if (x >= 1.0f) return 1.0f;
    return x;
}

MidiverbAudioProcessor::MidiverbAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    DRAM_A.fill (0);
    DRAM_B.fill (0);

    lfo1A = new Lfo();
    lfo2A = new Lfo();
    lfo1B = new Lfo();
    lfo2B = new Lfo();

    // Pre-delay parameter pointers
    preDelayAParam = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("preDelayA"));
    preDelayBParam = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("preDelayB"));

    // Pre-delay buffers: stereo, max length
    preDelayBufferA.setSize (2, MAX_PREDELAY_SAMPLES);
    preDelayBufferA.clear();
    preDelayBufferB.setSize (2, MAX_PREDELAY_SAMPLES);
    preDelayBufferB.clear();

    preDelayWritePosA = 0;
    preDelayWritePosB = 0;
}


MidiverbAudioProcessor::~MidiverbAudioProcessor()
{
    delete lfo1A;
    delete lfo2A;
    delete lfo1B;
    delete lfo2B;
}

juce::AudioProcessorValueTreeState::ParameterLayout MidiverbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Device selection for A (0=MIDIVerb, 1=MidiFex, 2=MIDIVerb 2)
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"deviceA", 1}, "Device A",
        0, NUM_DEVICES - 1, 2)); // default: Midiverb 2

    // Program selection for A (0-99)
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"programA", 1}, "Program A",
        0, MAX_PROGRAMS - 1, 1)); // default: 1

    // Device selection for B
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"deviceB", 1}, "Device B",
        0, NUM_DEVICES - 1, 2)); // default: Midiverb 2

    // Program selection for B
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"programB", 1}, "Program B",
        0, MAX_PROGRAMS - 1, 1)); // default: 1

    // Global wet/dry
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dryWet", 1}, "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    // Per-engine level (0–1)
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"levelA", 1}, "Level A",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"levelB", 1}, "Level B",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    // Enable toggles
    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"enableA", 1}, "Enable A", true));

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"enableB", 1}, "Enable B", true));


    // Input / Output gain in dB (-12..+12)
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"inputGain", 1}, "Input Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"feedbackA", 1}, "Feedback A",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"feedbackB", 1}, "Feedback B",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    // Routing
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"routing", 1}, "Routing",
        juce::StringArray{"Parallel", "Serial A→B", "Serial B→A"}, 0));

    // Per‑engine pan
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"panA", 1}, "Pan A",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"panB", 1}, "Pan B",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

    // Pre-EQ for A
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"hpfFreqA", 1}, "HPF A",
        juce::NormalisableRange<float>(10.0f, 10000.0f, 1.0f, 0.5f), 10.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lpfFreqA", 1}, "LPF A",
        juce::NormalisableRange<float>(50.0f, 20000.0f, 1.0f, 0.5f), 20000.0f));

    // Pre-EQ for B
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"hpfFreqB", 1}, "HPF B",
        juce::NormalisableRange<float>(10.0f, 10000.0f, 1.0f, 0.5f), 10.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lpfFreqB", 1}, "LPF B",
        juce::NormalisableRange<float>(50.0f, 20000.0f, 1.0f, 0.5f), 20000.0f));

    // LFO controls (per engine, for MV2 flanger/chorus)
    /* Rate: 0.25x .. 4x, default 1.0 */
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfoRateA", 1}, "LFO Rate A",
        juce::NormalisableRange<float>(0.25f, 4.0f, 0.01f, 0.5f), 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfoDepthA", 1}, "LFO Depth A",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f, 0.5f), 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfoRateB", 1}, "LFO Rate B",
        juce::NormalisableRange<float>(0.25f, 4.0f, 0.01f, 0.5f), 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfoDepthB", 1}, "LFO Depth B",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f, 0.5f), 1.0f));

    // Per‑engine pre-delay in ms (0–120 ms)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "preDelayA", 1 }, "PreDelay A",
        juce::NormalisableRange<float> (0.1f, 120.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "preDelayB", 1 }, "PreDelay B",
        juce::NormalisableRange<float> (0.1f, 120.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"fbSoftClip", 1}, "FB Soft Clip", false));


    return { params.begin(), params.end() };
}

const juce::String MidiverbAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MidiverbAudioProcessor::acceptsMidi() const { return false; }
bool MidiverbAudioProcessor::producesMidi() const { return false; }
bool MidiverbAudioProcessor::isMidiEffect() const { return false; }
double MidiverbAudioProcessor::getTailLengthSeconds() const { return 2.0; }

// important: do not confuse the internal midiverb programs with the plugin wrapper program!
int MidiverbAudioProcessor::getNumPrograms()        { return 1; }
int MidiverbAudioProcessor::getCurrentProgram()     { return 0; }
void MidiverbAudioProcessor::setCurrentProgram(int) { }
const juce::String MidiverbAudioProcessor::getProgramName(int) { return "Init"; }
void MidiverbAudioProcessor::changeProgramName(int, const juce::String&) { }



void MidiverbAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    hostSampleRate = sampleRate;
    phaseIncrement = EFFECT_SAMPLE_RATE / sampleRate;

    // Setup filters at host sample rate
    hostSampleRate  = sampleRate;
    phaseIncrement  = EFFECT_SAMPLE_RATE / sampleRate;

    // A filters
    antiAliasFilterA.setCutoff (sampleRate, FILTER_CUTOFF);
    antiAliasFilterA.reset();
    reconstructFilterAL.setCutoff (sampleRate, FILTER_CUTOFF);
    reconstructFilterAL.reset();
    reconstructFilterAR.setCutoff (sampleRate, FILTER_CUTOFF);
    reconstructFilterAR.reset();

    // B filters
    antiAliasFilterB.setCutoff (sampleRate, FILTER_CUTOFF);
    antiAliasFilterB.reset();
    reconstructFilterBL.setCutoff (sampleRate, FILTER_CUTOFF);
    reconstructFilterBL.reset();
    reconstructFilterBR.setCutoff (sampleRate, FILTER_CUTOFF);
    reconstructFilterBR.reset();

    // Interp buffers
    inputBufferA.reset();
    inputBufferB.reset();
    outputBufferAL.reset();
    outputBufferAR.reset();
    outputBufferBL.reset();
    outputBufferBR.reset();

    // Reset effect state

    // Reset effect state
    DRAM_A.fill (0);
    DRAM_B.fill (0);
    memoryPointerA = memoryPointerB = 0;
    lastWetAL = lastWetAR = lastWetBL = lastWetBR = 0.0;
    phaseA = phaseB = 0.0;

    // Reset LFO state for A
    currentLfoPatchA   = nullptr;
    lfo1ValueA         = 0;
    lfo2ValueA         = 0;
    lfoSampleCounterA  = 0;
    lastDeviceIndexA   = -1;
    lastProgramIndexA  = -1;

    // Reset LFO state for B
    currentLfoPatchB   = nullptr;
    lfo1ValueB         = 0;
    lfo2ValueB         = 0;
    lfoSampleCounterB  = 0;
    lastDeviceIndexB   = -1;
    lastProgramIndexB  = -1;


    using Coeff = juce::dsp::IIR::Coefficients<float>;

    hpfAL.reset(); hpfAR.reset();
    lpfAL.reset(); lpfAR.reset();
    hpfBL.reset(); hpfBR.reset();
    lpfBL.reset(); lpfBR.reset();

    hpfAL.coefficients = Coeff::makeHighPass (sampleRate, 10.0f);
    hpfAR.coefficients = Coeff::makeHighPass (sampleRate, 10.0f);
    lpfAL.coefficients = Coeff::makeLowPass  (sampleRate, 20000.0f);
    lpfAR.coefficients = Coeff::makeLowPass  (sampleRate, 20000.0f);

    hpfBL.coefficients = Coeff::makeHighPass (sampleRate, 10.0f);
    hpfBR.coefficients = Coeff::makeHighPass (sampleRate, 10.0f);
    lpfBL.coefficients = Coeff::makeLowPass  (sampleRate, 20000.0f);
    lpfBR.coefficients = Coeff::makeLowPass  (sampleRate, 20000.0f);

    // Reset pre-delay buffers
    preDelayBufferA.clear();
    preDelayBufferB.clear();
    preDelayWritePosA = 0;
    preDelayWritePosB = 0;

    vibratoA.prepare (sampleRate);
    vibratoB.prepare (sampleRate);

}

void MidiverbAudioProcessor::releaseResources()
{
}

bool MidiverbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void MidiverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    int deviceIndexA  = (int)*apvts.getRawParameterValue ("deviceA");
    int programIndexA = (int)*apvts.getRawParameterValue ("programA");
    int deviceIndexB  = (int)*apvts.getRawParameterValue ("deviceB");
    int programIndexB = (int)*apvts.getRawParameterValue ("programB");

    float dryWet     = *apvts.getRawParameterValue ("dryWet");
    float feedbackA  = *apvts.getRawParameterValue ("feedbackA");
    float feedbackB  = *apvts.getRawParameterValue ("feedbackB");
    int   routing    = (int)*apvts.getRawParameterValue ("routing");
    float panA       = *apvts.getRawParameterValue ("panA");
    float panB       = *apvts.getRawParameterValue ("panB");
    float inputGain  = *apvts.getRawParameterValue ("inputGain");
    float outputGain = *apvts.getRawParameterValue ("outputGain");
    float levelA     = *apvts.getRawParameterValue ("levelA");
    float levelB     = *apvts.getRawParameterValue ("levelB");
    bool  enableA    = *apvts.getRawParameterValue ("enableA") > 0.5f;
    bool  enableB    = *apvts.getRawParameterValue ("enableB") > 0.5f;
    bool fbSoftClip = *apvts.getRawParameterValue ("fbSoftClip") > 0.5f;

    auto dbToLin = [] (float db) { return std::pow (10.0f, db / 20.0f); };

    float inputLin  = dbToLin (inputGain);
    float outputLin = dbToLin (outputGain);

    float dryGain, wetGain;
    {
        float ang = dryWet * 1.5707963f;
        dryGain = std::cos (ang);
        wetGain = std::sin (ang);
    }

    //====================================================================

    const auto& devA = devices[deviceIndexA];
    if (programIndexA < 0)                     programIndexA = 0;
    if (programIndexA >= devA.numEffects)      programIndexA = devA.numEffects - 1;

    const auto& devB = devices[deviceIndexB];
    if (programIndexB < 0)                     programIndexB = 0;
    if (programIndexB >= devB.numEffects)      programIndexB = devB.numEffects - 1;

    auto effectFnA = devA.effects[programIndexA];
    auto effectFnB = devB.effects[programIndexB];

    //====================================================================
    // On device/program change for A
    if (deviceIndexA != lastDeviceIndexA || programIndexA != lastProgramIndexA)
    {
        lastDeviceIndexA  = deviceIndexA;
        lastProgramIndexA = programIndexA;

        if (deviceIndexA == 2 && init_lfo_for_program (programIndexA, lfo1A, lfo2A, &currentLfoPatchA))
        {
            lfoSampleCounterA = 0;

            float rateScaleA  = *apvts.getRawParameterValue ("lfoRateA");
            float depthScaleA = *apvts.getRawParameterValue ("lfoDepthA");

            // scale base patch once, then reset and get first values
            lfo1A->rate = (int) std::round (currentLfoPatchA->rate1 * rateScaleA);
            lfo2A->rate = (int) std::round (currentLfoPatchA->rate2 * rateScaleA);
            lfo1A->max  = (int) std::round (currentLfoPatchA->max   * depthScaleA);
            lfo2A->max  = (int) std::round (currentLfoPatchA->max   * depthScaleA);

            lfo1A->reset (lfo1A);
            lfo2A->reset (lfo2A);

            lfo1ValueA = lfo1A->update (lfo1A) | (uint32_t (currentLfoPatchA->top1) << 16);
            lfo2ValueA = lfo2A->update (lfo2A) | (uint32_t (currentLfoPatchA->top2) << 16);
        }
        else
        {
            currentLfoPatchA = nullptr;
            lfo1ValueA = lfo2ValueA = 0;
        }
    }

    // On device/program change for B
    if (deviceIndexB != lastDeviceIndexB || programIndexB != lastProgramIndexB)
    {
        lastDeviceIndexB  = deviceIndexB;
        lastProgramIndexB = programIndexB;

        if (deviceIndexB == 2 && init_lfo_for_program (programIndexB, lfo1B, lfo2B, &currentLfoPatchB))
        {
            lfoSampleCounterB = 0;

            float rateScaleB  = *apvts.getRawParameterValue ("lfoRateB");
            float depthScaleB = *apvts.getRawParameterValue ("lfoDepthB");

            lfo1B->rate = (int) std::round (currentLfoPatchB->rate1 * rateScaleB);
            lfo2B->rate = (int) std::round (currentLfoPatchB->rate2 * rateScaleB);
            lfo1B->max  = (int) std::round (currentLfoPatchB->max   * depthScaleB);
            lfo2B->max  = (int) std::round (currentLfoPatchB->max   * depthScaleB);

            lfo1B->reset (lfo1B);
            lfo2B->reset (lfo2B);

            lfo1ValueB = lfo1B->update (lfo1B) | (uint32_t (currentLfoPatchB->top1) << 16);
            lfo2ValueB = lfo2B->update (lfo2B) | (uint32_t (currentLfoPatchB->top2) << 16);
        }
        else
        {
            currentLfoPatchB = nullptr;
            lfo1ValueB = lfo2ValueB = 0;
        }
    }

    // After currentLfoPatchA/B are valid, decide which engines have MV2 LFO
    const bool isMV2A       = (deviceIndexA == 2);
    const bool isMV2B       = (deviceIndexB == 2);
    const bool patchHasLfoA = isMV2A && (currentLfoPatchA != nullptr);
    const bool patchHasLfoB = isMV2B && (currentLfoPatchB != nullptr);

    // Per-block MV2 LFO re-scaling (if desired)
    if (patchHasLfoA)
    {
        float rateScaleA  = *apvts.getRawParameterValue ("lfoRateA");
        float depthScaleA = *apvts.getRawParameterValue ("lfoDepthA");

        lfo1A->rate = (int) std::round (currentLfoPatchA->rate1 * rateScaleA);
        lfo2A->rate = (int) std::round (currentLfoPatchA->rate2 * rateScaleA);
        lfo1A->max  = (int) std::round (currentLfoPatchA->max   * depthScaleA);
        lfo2A->max  = (int) std::round (currentLfoPatchA->max   * depthScaleA);
    }

    if (patchHasLfoB)
    {
        float rateScaleB  = *apvts.getRawParameterValue ("lfoRateB");
        float depthScaleB = *apvts.getRawParameterValue ("lfoDepthB");

        lfo1B->rate = (int) std::round (currentLfoPatchB->rate1 * rateScaleB);
        lfo2B->rate = (int) std::round (currentLfoPatchB->rate2 * rateScaleB);
        lfo1B->max  = (int) std::round (currentLfoPatchB->max   * depthScaleB);
        lfo2B->max  = (int) std::round (currentLfoPatchB->max   * depthScaleB);
    }

    auto* leftChannel  = buffer.getWritePointer (0);
    auto* rightChannel = buffer.getWritePointer (1);
    int numSamples     = buffer.getNumSamples ();

    //=====================================================
    // VIBRATO PARAMS (reuse lfoRate/Depth when no MV2 LFO)
    float uiRateA  = *apvts.getRawParameterValue ("lfoRateA");
    float uiDepthA = *apvts.getRawParameterValue ("lfoDepthA");
    float uiRateB  = *apvts.getRawParameterValue ("lfoRateB");
    float uiDepthB = *apvts.getRawParameterValue ("lfoDepthB");

    auto mapRate  = [] (float r) { return juce::jlimit (0.05f, 2.0f, r); };
    auto mapDepth = [] (float d) { return juce::jlimit (0.0f, 6.0f, d * 3.0f); };

    if (! patchHasLfoA)
        vibratoA.setParams (mapRate (uiRateA), mapDepth (uiDepthA));
    else
        vibratoA.setParams (0.0f, 0.0f);

    if (! patchHasLfoB)
        vibratoB.setParams (mapRate (uiRateB), mapDepth (uiDepthB));
    else
        vibratoB.setParams (0.0f, 0.0f);
    //=====================================================

    // =================================================================

    // // Pan stuff
    auto panToGains = [](float pan, float& gL, float& gR)
    {
        // pan in [-1, 1] → equal‑power L/R
        float angle = (pan + 1.0f) * 0.25f * 3.1415926f; // 0..pi/2
        gL = std::cos(angle);
        gR = std::sin(angle);
    };

    float panAL, panAR, panBL, panBR;
    panToGains (panA, panAL, panAR);
    panToGains (panB, panBL, panBR);

    // grab pre filter stuff, make coeffs
    auto* hpfAParam = apvts.getRawParameterValue ("hpfFreqA");
    auto* lpfAParam = apvts.getRawParameterValue ("lpfFreqA");
    auto* hpfBParam = apvts.getRawParameterValue ("hpfFreqB");
    auto* lpfBParam = apvts.getRawParameterValue ("lpfFreqB");

    float hpfFreqA = hpfAParam->load (std::memory_order_relaxed);
    float lpfFreqA = lpfAParam->load (std::memory_order_relaxed);
    float hpfFreqB = hpfBParam->load (std::memory_order_relaxed);
    float lpfFreqB = lpfBParam->load (std::memory_order_relaxed);

    using Coeff = juce::dsp::IIR::Coefficients<float>;

    hpfAL.coefficients = Coeff::makeHighPass (hostSampleRate, hpfFreqA);
    hpfAR.coefficients = Coeff::makeHighPass (hostSampleRate, hpfFreqA);
    lpfAL.coefficients = Coeff::makeLowPass  (hostSampleRate, lpfFreqA);
    lpfAR.coefficients = Coeff::makeLowPass  (hostSampleRate, lpfFreqA);

    hpfBL.coefficients = Coeff::makeHighPass (hostSampleRate, hpfFreqB);
    hpfBR.coefficients = Coeff::makeHighPass (hostSampleRate, hpfFreqB);
    lpfBL.coefficients = Coeff::makeLowPass  (hostSampleRate, lpfFreqB);
    lpfBR.coefficients = Coeff::makeLowPass  (hostSampleRate, lpfFreqB);


    float blockInPeak  = 0.0f;
    float blockOutPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Apply input gain
        float inL = leftChannel[i]  * inputLin;
        float inR = rightChannel[i] * inputLin;

        // ── INPUT PEAK CHECK (after input gain, before filters/reverb) ──────
        float inAbs = juce::jmax (std::abs (inL), std::abs (inR));
        if (inAbs > blockInPeak)  blockInPeak  = inAbs;
        if (inAbs > 1.0f)         inputOverload.store (true, std::memory_order_relaxed);
        // ────────────────────────────────────────────────────────────────────

        // Sum DAW input to mono once
        float inMono = 0.5f * (inL + inR);

        // Default external feeds BEFORE pre-delay and feedback
        double extA_raw = inMono; // DAW into A
        double extB_raw = inMono; // DAW into B


        // ROUTING SWITCH __________________________________________________
        // if either devices are disabled, make them act in parallel =====
        bool bothEnabled  = enableA && enableB;


        // If serial is selected but we don't actually have both engines active,
        // fall back to "no series" behavior.
        int effectiveRouting = routing;
        if (!bothEnabled && routing != 0)
            effectiveRouting = 0; // force Parallel when either or both are disabled

        switch (effectiveRouting)
        {
            case 1: // A -> B
                extA_raw = inMono;                              // DAW into A
                extB_raw = (lastWetAL + lastWetAR) * 0.5;       // A's wet into B
                break;

            case 2: // B -> A
                extB_raw = inMono;                              // DAW into B
                extA_raw = (lastWetBL + lastWetBR) * 0.5;       // B's wet into A
                break;

            case 0:
            default:
                extA_raw = inMono;                              // DAW into both
                extB_raw = inMono;
                break;
        }
        //_________________________________________________

        // Per-engine pre-delay times in samples
        float preDelayMsA = *apvts.getRawParameterValue ("preDelayA");
        float preDelayMsB = *apvts.getRawParameterValue ("preDelayB");

        int preDelaySamplesA = juce::jlimit (0,
            MAX_PREDELAY_SAMPLES - 1,
            (int) std::round (preDelayMsA * hostSampleRate / 1000.0));
        int preDelaySamplesB = juce::jlimit (0,
            MAX_PREDELAY_SAMPLES - 1,
            (int) std::round (preDelayMsB * hostSampleRate / 1000.0));

        // --- Pre-delay A on its routed input ---
        float* preABuf = preDelayBufferA.getWritePointer (0);
        preABuf[preDelayWritePosA] = (float) extA_raw;

        int readPosA = preDelayWritePosA - preDelaySamplesA;
        if (readPosA < 0)
            readPosA += MAX_PREDELAY_SAMPLES;

        float preA = preABuf[readPosA];

        preDelayWritePosA++;
        if (preDelayWritePosA >= MAX_PREDELAY_SAMPLES)
            preDelayWritePosA = 0;

        // --- Pre-delay B on its routed input ---
        float* preBBuf = preDelayBufferB.getWritePointer (0);
        preBBuf[preDelayWritePosB] = (float) extB_raw;

        int readPosB = preDelayWritePosB - preDelaySamplesB;
        if (readPosB < 0)
            readPosB += MAX_PREDELAY_SAMPLES;

        float preB = preBBuf[readPosB];

        preDelayWritePosB++;
        if (preDelayWritePosB >= MAX_PREDELAY_SAMPLES)
            preDelayWritePosB = 0;

        // These are the dry feeds into each engine before feedback:
        double monoAInDry = preA;
        double monoBInDry = preB;

        // Local feedback, single computation
        double fbA = (lastWetAL + lastWetAR) * 0.5 * feedbackA;
        double fbB = (lastWetBL + lastWetBR) * 0.5 * feedbackB;

        // Optional soft clip on feedback to keep it musical
        if (fbSoftClip)
        {
            auto softClip = [] (double x)
            {
                // gentle drive; tweak 1.5 if you want more/less saturation
                const double drive = 1.5;
                return std::tanh (drive * x);
            };

            fbA = softClip (fbA);
            fbB = softClip (fbB);
        }

        constexpr double fbMax = 1.0;
        double fbMagA = std::abs (fbA);
        if (fbMagA > fbMax) fbA *= fbMax / fbMagA;
        double fbMagB = std::abs (fbB);
        if (fbMagB > fbMax) fbB *= fbMax / fbMagB;

        // Vibrato on feedback only
        float fbModA = vibratoA.process ((float) fbA);
        float fbModB = vibratoB.process ((float) fbB);

        // engine inputs: routed+predelayed dry + modulated feedback
        double inA = monoAInDry + fbModA;
        double inB = monoBInDry + fbModB;

        // Per-engine HPF/LPF on the *total* engine input (DAW/series + feedback)
        float inA_f = (float) inA;
        float inB_f = (float) inB;

        // If you only want one filter per engine, you can just use the “L” filters
        inA_f = hpfAL.processSample (inA_f);
        inA_f = lpfAL.processSample (inA_f);

        inB_f = hpfBL.processSample (inB_f);
        inB_f = lpfBL.processSample (inB_f);

        // Replace inA/inB with filtered versions
        inA = inA_f;
        inB = inB_f;

        // --- A: downsample to effect rate ---
        double filteredA = antiAliasFilterA.process (inA);
        inputBufferA.push (filteredA);

        phaseA += phaseIncrement;
        while (phaseA >= 1.0)
        {
            phaseA -= 1.0;

            double inputFracA = 1.0 - phaseA / phaseIncrement;
            double interpA    = inputBufferA.interpolate (inputFracA);

            int16_t inIntA = (int16_t) (saturate ((float) interpA) * 0x0fff);

            int16_t aL, aR;
            effectFnA (inIntA, &aL, &aR, DRAM_A.data(), memoryPointerA++, lfo1ValueA, lfo2ValueA);

            float aLf = aL / (float) 0x0fff;
            float aRf = aR / (float) 0x0fff;

            outputBufferAL.push (aLf);
            outputBufferAR.push (aRf);

            // LFO A update as before
            if (currentLfoPatchA)
            {
                if (++lfoSampleCounterA >= LFO_UPDATE_INTERVAL)
                {
                    lfoSampleCounterA = 0;
                    lfo1ValueA = lfo1A->update (lfo1A) | (uint32_t (currentLfoPatchA->top1) << 16);
                    lfo2ValueA = lfo2A->update (lfo2A) | (uint32_t (currentLfoPatchA->top2) << 16);
                }
            }
        }

        // --- B: downsample to effect rate ---
        double filteredB = antiAliasFilterB.process (inB);
        inputBufferB.push (filteredB);

        phaseB += phaseIncrement;
        while (phaseB >= 1.0)
        {
            phaseB -= 1.0;

            double inputFracB = 1.0 - phaseB / phaseIncrement;
            double interpB    = inputBufferB.interpolate (inputFracB);

            int16_t inIntB = (int16_t) (saturate ((float) interpB) * 0x0fff);

            int16_t bL, bR;
            effectFnB (inIntB, &bL, &bR, DRAM_B.data(), memoryPointerB++, lfo1ValueB, lfo2ValueB);

            float bLf = bL / (float) 0x0fff;
            float bRf = bR / (float) 0x0fff;

            outputBufferBL.push (bLf);
            outputBufferBR.push (bRf);

            // LFO B update as before
            if (currentLfoPatchB)
            {
                if (++lfoSampleCounterB >= LFO_UPDATE_INTERVAL)
                {
                    lfoSampleCounterB = 0;
                    lfo1ValueB = lfo1B->update (lfo1B) | (uint32_t (currentLfoPatchB->top1) << 16);
                    lfo2ValueB = lfo2B->update (lfo2B) | (uint32_t (currentLfoPatchB->top2) << 16);
                }
            }
        }

        // --- Upsample / reconstruct both engines separately ---
        double wetAL = reconstructFilterAL.process (outputBufferAL.interpolate (phaseA));
        double wetAR = reconstructFilterAR.process (outputBufferAR.interpolate (phaseA));
        double wetBL = reconstructFilterBL.process (outputBufferBL.interpolate (phaseB));
        double wetBR = reconstructFilterBR.process (outputBufferBR.interpolate (phaseB));

        lastWetAL = wetAL; lastWetAR = wetAR;
        lastWetBL = wetBL; lastWetBR = wetBR;

        // Per-engine pan + level + enable (no cross-feedback anymore)
        double revAL = enableA ? wetAL * panAL * levelA : 0.0;
        double revAR = enableA ? wetAR * panAR * levelA : 0.0;
        double revBL = enableB ? wetBL * panBL * levelB : 0.0;
        double revBR = enableB ? wetBR * panBR * levelB : 0.0;

        double wetLCombined = revAL + revBL;
        double wetRCombined = revAR + revBR;

        double outL = (inL * dryGain + wetLCombined * wetGain) * outputLin;
        double outR = (inR * dryGain + wetRCombined * wetGain) * outputLin;

        // ── OUTPUT PEAK CHECK (after output gain, before writing to DAW) ────
        float outAbs = juce::jmax (std::abs ((float) outL), std::abs ((float) outR));
        if (outAbs > blockOutPeak)
            blockOutPeak = outAbs;

        // Out is clipped this sample?
        bool sampleClipped = (outAbs > 1.0f);
        if (sampleClipped)
            outputOverload.store (true, std::memory_order_relaxed);

        // ────────────────────────────────────────────────────────────────────        

        leftChannel[i]  = (float) outL;
        rightChannel[i] = (float) outR;
    }

    inputLevel.store  (blockInPeak,  std::memory_order_relaxed);
    outputLevel.store (blockOutPeak, std::memory_order_relaxed);

    // Clip-latch timing: use blockOutPeak > 1 to mean "this block is clipping"
    const bool blockClipping = (blockOutPeak > 1.0f);
    const double blockSeconds = numSamples / getSampleRate();

    if (blockClipping)
    {
        clipAccumSeconds += blockSeconds;
        if (clipAccumSeconds >= 5.0)
            clipLatched.store (true, std::memory_order_relaxed);
    }
    else
    {
        clipAccumSeconds = 0.0;
        clipLatched.store (false, std::memory_order_relaxed);
    }
}


bool MidiverbAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* MidiverbAudioProcessor::createEditor()
{
    return new MidiverbAudioProcessorEditor(*this);
}

void MidiverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MidiverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiverbAudioProcessor();
}
