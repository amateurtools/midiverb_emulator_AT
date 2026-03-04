#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include "UILayout.h"

using namespace UILayout;

//==============================================================================
// KnobLookAndFeel
//==============================================================================
KnobLookAndFeel::KnobLookAndFeel()
{
    knobImage = juce::ImageCache::getFromMemory (BinaryData::knob_png,
                                                 BinaryData::knob_pngSize);
}

void KnobLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                        int x, int y, int width, int height,
                                        float sliderPos,
                                        float startAngle, float endAngle,
                                        juce::Slider&)
{
    if (! knobImage.isValid())
        return;

    auto bounds = juce::Rectangle<float> ((float)x, (float)y,
                                          (float)width, (float)height);
    float angle = startAngle + sliderPos * (endAngle - startAngle);

    juce::AffineTransform t;
    t = t.translated (-knobImage.getWidth()  * 0.5f,
                      -knobImage.getHeight() * 0.5f);
    t = t.rotated (angle);
    t = t.translated (bounds.getCentreX(), bounds.getCentreY());

    g.drawImageTransformed (knobImage, t, false);
}

//==============================================================================
// Helper: configure a rotary slider
//==============================================================================
static void setupKnob (juce::Slider& s, KnobLookAndFeel& laf)
{
    s.setLookAndFeel (&laf);
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
}

//==============================================================================
// Constructor
//==============================================================================
MidiverbAudioProcessorEditor::MidiverbAudioProcessorEditor (MidiverbAudioProcessor& p)
    : AudioProcessorEditor (p), audioProcessor (p)
{
    auto& apvts = audioProcessor.getAPVTS();

    bgImage   = juce::ImageCache::getFromMemory (BinaryData::background_png,
                                                 BinaryData::background_pngSize);
    tileImage = juce::ImageCache::getFromMemory (BinaryData::tile_image_png,
                                                 BinaryData::tile_image_pngSize);

    // Small helper: set up + attach a rotary slider
    auto makeRotary = [&] (juce::Slider& slider,
                           std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                           const juce::String& paramID)
    {
        setupKnob (slider, knobLAF);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);
    };

    // LAF for all combo-like controls
    deviceASelector.setLookAndFeel (&comboLAF);
    programASelector.setLookAndFeel (&comboLAF);
    deviceBSelector.setLookAndFeel (&comboLAF);
    programBSelector.setLookAndFeel (&comboLAF);

    // ── Device A selectors ───────────────────────────────────────────────────
    for (int i = 0; i < MidiverbAudioProcessor::NUM_DEVICES; ++i)
        deviceASelector.addItem (MidiverbAudioProcessor::getDeviceName (i), i + 1);
    addAndMakeVisible (deviceASelector);
    deviceAAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                            apvts, "deviceA", deviceASelector);

    addAndMakeVisible (programASelector);
    programASelector.onChange = [this]
    {
        int idx = programASelector.getSelectedId() - 1;
        if (idx >= 0)
        {
            auto* param   = audioProcessor.getAPVTS().getParameter ("programA");
            int   current = (int)*audioProcessor.getAPVTS().getRawParameterValue ("programA");
            if (param != nullptr && idx != current)
                param->setValueNotifyingHost (param->convertTo0to1 ((float) idx));
        }
    };
    updateProgramSelectorA();

    // ── Device A knobs (2×4) ────────────────────────────────────────────────
    makeRotary (preDelayASlider,   preDelayAAttachment,   "preDelayA");
    makeRotary (feedbackASlider,   feedbackAAttachment,   "feedbackA");
    makeRotary (panASlider,        panAAttachment,        "panA");
    // panASlider.setRange (-1.0, 1.0, 0.01);
    makeRotary (levelASlider,      levelAAttachment,      "levelA");

    makeRotary (hpfASlider,        hpfAAttachment,        "hpfFreqA");
    makeRotary (lpfASlider,        lpfAAttachment,        "lpfFreqA");
    makeRotary (lfoDepthASlider,   lfoDepthAAttachment,   "lfoDepthA");
    makeRotary (lfoRateASlider,    lfoRateAAttachment,    "lfoRateA");

    // ── Device B selectors ───────────────────────────────────────────────────
    for (int i = 0; i < MidiverbAudioProcessor::NUM_DEVICES; ++i)
        deviceBSelector.addItem (MidiverbAudioProcessor::getDeviceName (i), i + 1);
    addAndMakeVisible (deviceBSelector);
    deviceBAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                            apvts, "deviceB", deviceBSelector);

    addAndMakeVisible (programBSelector);
    programBSelector.onChange = [this]
    {
        int idx = programBSelector.getSelectedId() - 1;
        if (idx >= 0)
        {
            auto* param   = audioProcessor.getAPVTS().getParameter ("programB");
            int   current = (int)*audioProcessor.getAPVTS().getRawParameterValue ("programB");
            if (param != nullptr && idx != current)
                param->setValueNotifyingHost (param->convertTo0to1 ((float) idx));
        }
    };
    updateProgramSelectorB();

    // ── Device B knobs (2×4) ────────────────────────────────────────────────
    makeRotary (preDelayBSlider,   preDelayBAttachment,   "preDelayB");
    makeRotary (feedbackBSlider,   feedbackBAttachment,   "feedbackB");
    makeRotary (panBSlider,        panBAttachment,        "panB");
    // panBSlider.setRange (-1.0, 1.0, 0.01);
    makeRotary (levelBSlider,      levelBAttachment,      "levelB");

    makeRotary (hpfBSlider,        hpfBAttachment,        "hpfFreqB");
    makeRotary (lpfBSlider,        lpfBAttachment,        "lpfFreqB");
    makeRotary (lfoDepthBSlider,   lfoDepthBAttachment,   "lfoDepthB");
    makeRotary (lfoRateBSlider,    lfoRateBAttachment,    "lfoRateB");

    // ── Global controls ─────────────────────────────────────────────────────
    makeRotary (inputGainSlider,   inputGainAttachment,   "inputGain");
    makeRotary (outputGainSlider,  outputGainAttachment,  "outputGain");
    makeRotary (dryWetSlider,      dryWetAttachment,      "dryWet");

    // ── Indicators ──────────────────────────────────────────────────────────
    auto sigImg = juce::ImageCache::getFromMemory (BinaryData::signal_indicator_png,
                                                   BinaryData::signal_indicator_pngSize);
    inputIndicator.setImage  (sigImg);
    outputIndicator.setImage (sigImg);
    addAndMakeVisible (inputIndicator);
    addAndMakeVisible (outputIndicator);

    // ── Window ─────────────────────────────────────────────
    setSize (800, 600);

    // No resizer, no constrainer
    setResizable (false, false);
    // remove the ResizableCornerComponent and constrainer usage

    // ── Routing column ──────────────────────────────────────────────────────
    auto imgParallel = juce::ImageCache::getFromMemory (BinaryData::route_parallel_png,
                                                        BinaryData::route_parallel_pngSize);
    auto imgRight    = juce::ImageCache::getFromMemory (BinaryData::route_right_png,
                                                        BinaryData::route_right_pngSize);
    auto imgLeft     = juce::ImageCache::getFromMemory (BinaryData::route_left_png,
                                                        BinaryData::route_left_pngSize);

    routingColumn.setImages (imgParallel, imgRight, imgLeft);
    addAndMakeVisible (routingColumn);

    routingColumn.setValue ((int)*apvts.getRawParameterValue ("routing"),
                            juce::dontSendNotification);

    routingColumn.onChange = [&]
    {
        if (auto* param = apvts.getParameter ("routing"))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) routingColumn.getValue()));
    };

    routingAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("routing"),
        [this] (float v)
        {
            routingColumn.setValue (juce::roundToInt (v), juce::dontSendNotification);
        });

    // ── Enable LEDs ─────────────────────────────────────────────────────────
    auto ledImg = juce::ImageCache::getFromMemory (BinaryData::toggle_led_png,
                                                   BinaryData::toggle_led_pngSize);
    enableAButton.setImage (ledImg);
    enableBButton.setImage (ledImg);

    addAndMakeVisible (enableAButton);
    addAndMakeVisible (enableBButton);

    enableAAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                            apvts, "enableA", enableAButton);
    enableBAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                            apvts, "enableB", enableBButton);



    juce::Image flameImage = juce::ImageCache::getFromMemory (
        BinaryData::flame_png,
        BinaryData::flame_pngSize);

    clipFlameFilmstrip.setImage (flameImage, 7);  // 7 frames horizontally
    addChildComponent (clipFlameFilmstrip);  // adds but keeps hidden


    startTimerHz (30);
}


MidiverbAudioProcessorEditor::~MidiverbAudioProcessorEditor()
{
    stopTimer();
    deviceASelector.setLookAndFeel (nullptr);
    programASelector.setLookAndFeel (nullptr);
    deviceBSelector.setLookAndFeel (nullptr);
    programBSelector.setLookAndFeel (nullptr);
}

//==============================================================================
// Program selector helpers
//==============================================================================
void MidiverbAudioProcessorEditor::updateProgramSelectorA()
{
    int deviceIndex  = (int)*audioProcessor.getAPVTS().getRawParameterValue ("deviceA");
    int currentIndex = (int)*audioProcessor.getAPVTS().getRawParameterValue ("programA");

    if (deviceIndex != lastDeviceAIndex)
    {
        lastDeviceAIndex  = deviceIndex;
        lastProgramAIndex = -1;
        programASelector.clear (juce::dontSendNotification);

        int numEffects    = MidiverbAudioProcessor::getDeviceNumEffects (deviceIndex);
        int displayOffset = MidiverbAudioProcessor::getDeviceDisplayOffset (deviceIndex);
        for (int i = 0; i < numEffects; ++i)
            programASelector.addItem (juce::String (i + displayOffset) + " "
                                      + MidiverbAudioProcessor::getEffectName (deviceIndex, i),
                                      i + 1);
    }

    int numEffects = MidiverbAudioProcessor::getDeviceNumEffects (deviceIndex);
    currentIndex   = juce::jlimit (0, numEffects - 1, currentIndex);

    if (currentIndex != lastProgramAIndex)
    {
        lastProgramAIndex = currentIndex;
        programASelector.setSelectedId (currentIndex + 1, juce::dontSendNotification);
    }
}

void MidiverbAudioProcessorEditor::updateProgramSelectorB()
{
    int deviceIndex  = (int)*audioProcessor.getAPVTS().getRawParameterValue ("deviceB");
    int currentIndex = (int)*audioProcessor.getAPVTS().getRawParameterValue ("programB");

    if (deviceIndex != lastDeviceBIndex)
    {
        lastDeviceBIndex  = deviceIndex;
        lastProgramBIndex = -1;
        programBSelector.clear (juce::dontSendNotification);

        int numEffects    = MidiverbAudioProcessor::getDeviceNumEffects (deviceIndex);
        int displayOffset = MidiverbAudioProcessor::getDeviceDisplayOffset (deviceIndex);
        for (int i = 0; i < numEffects; ++i)
            programBSelector.addItem (juce::String (i + displayOffset) + " "
                                      + MidiverbAudioProcessor::getEffectName (deviceIndex, i),
                                      i + 1);
    }

    int numEffects = MidiverbAudioProcessor::getDeviceNumEffects (deviceIndex);
    currentIndex   = juce::jlimit (0, numEffects - 1, currentIndex);

    if (currentIndex != lastProgramBIndex)
    {
        lastProgramBIndex = currentIndex;
        programBSelector.setSelectedId (currentIndex + 1, juce::dontSendNotification);
    }
}

SignalIndicator::State levelToState (float peak, bool clipFlag)
{
    if (clipFlag)
        return SignalIndicator::Clip;

    // crude thresholds: < -60 dB ≈ < 0.001 -> Silence
    if (peak < 0.001f)
        return SignalIndicator::Silence;

    return SignalIndicator::Signal;
}

void MidiverbAudioProcessorEditor::timerCallback()
{
    updateProgramSelectorA();
    updateProgramSelectorB();

    auto& proc = audioProcessor;

    // ---- Input indicator ----
    float inLevel = proc.getInputLevel();
    bool  inClip  = proc.getAndClearInputOverload();

    SignalIndicator::State inState;
    if (inClip)
        inState = SignalIndicator::Clip;      // rightmost cell
    else if (inLevel < 0.001f)               // ~ -60 dB
        inState = SignalIndicator::Silence;   // leftmost cell
    else
        inState = SignalIndicator::Signal;    // middle cell

    inputIndicator.setState (inState);

    // ---- Output indicator ----
    float outLevel = proc.getOutputLevel();
    bool  outClip  = proc.getAndClearOutputOverload();

    SignalIndicator::State outState;
    if (outClip)
        outState = SignalIndicator::Clip;
    else if (outLevel < 0.001f)
        outState = SignalIndicator::Silence;
    else
        outState = SignalIndicator::Signal;

    outputIndicator.setState (outState);


    const bool latched = audioProcessor.getClipLatched();

    clipFlameFilmstrip.setShouldAnimate (latched);
}

void MidiverbAudioProcessorEditor::paint (juce::Graphics& g)
{
    const int W = getWidth();
    const int H = getHeight();

    constexpr float baseW = UILayout::canvasW;
    constexpr float baseH = UILayout::canvasH;

    const float s  = std::min ((float) W / baseW,
                               (float) H / baseH);

    const int uiW = (int) std::round (baseW * s);
    const int uiH = (int) std::round (baseH * s);
    const int uiX = (W - uiW) / 2;
    const int uiY = (H - uiH) / 2;

    g.fillAll (juce::Colours::black);

    if (bgImage.isValid())
        g.drawImage (bgImage,
                     uiX, uiY, uiW, uiH,
                     0, 0, bgImage.getWidth(), bgImage.getHeight());
}

//==============================================================================
// Resized
//==============================================================================
void MidiverbAudioProcessorEditor::resized()
{
    using namespace UILayout;

    const int W = getWidth();
    const int H = getHeight();

    // Resizer in raw window coords
    if (resizer != nullptr)
        resizer->setBounds (W - 16, H - 16, 16, 16);

    constexpr float baseW = canvasW;
    constexpr float baseH = canvasH;

    const float s   = std::min ((float) W / baseW,
                                (float) H / baseH);
    const int uiW   = (int) std::round (baseW * s);
    const int uiH   = (int) std::round (baseH * s);
    const int uiX   = (W - uiW) / 2;
    const int uiY   = (H - uiH) / 2;

    auto toX = [uiX, s] (float baseX) { return uiX + (int) std::round (baseX * s); };
    auto toY = [uiY, s] (float baseY) { return uiY + (int) std::round (baseY * s); };

    const int knobPx      = (int) std::round (knobSize        * s);
    const int comboPxW    = (int) std::round (comboW          * s);
    const int comboPxH    = (int) std::round (comboH          * s);
    const int comboGapPxY = (int) std::round (comboGapY       * s);
    const int indPxW      = (int) std::round (indicatorW      * s);
    const int indPxH      = (int) std::round (indicatorH      * s);
    const int ledPxW      = (int) std::round (enableLedW      * s);
    const int ledPxH      = (int) std::round (enableLedH      * s);
    // const int routeCellPx = (int) std::round (routingCellSize * s);
    // const int routeGapPx  = (int) std::round (routingGapY     * s);

    auto placeKnobTopLeft = [&] (juce::Slider& slider, float baseX, float baseY)
    {
        slider.setBounds (toX (baseX), toY (baseY), knobPx, knobPx);
    };

    // auto placeKnobCenter = [&] (juce::Slider& slider, float baseCX, float baseCY)
    // {
    //     const int cx = toX (baseCX);
    //     const int cy = toY (baseCY);
    //     slider.setBounds (cx - knobPx / 2, cy - knobPx / 2, knobPx, knobPx);
    // };

    auto placeComboStack = [&] (juce::ComboBox& modelCombo,
                                juce::ComboBox& patchCombo,
                                float baseCenterX,
                                float baseTopY)
    {
        const int cx   = toX (baseCenterX);
        const int topY = toY (baseTopY);

        modelCombo.setBounds (cx - comboPxW / 2, topY, comboPxW, comboPxH);

        const int patchY = topY + comboPxH + comboGapPxY;
        patchCombo.setBounds (cx - comboPxW / 2, patchY, comboPxW, comboPxH);
    };

    // Device combos
    placeComboStack (deviceASelector, programASelector, colACenterX, colATopY);
    placeComboStack (deviceBSelector, programBSelector, colBCenterX, colBTopY);

    // Device A knobs (top-left coordinates from UILayout)
    placeKnobTopLeft (preDelayASlider, devA_knobCol1_X, devA_knobRow1_Y);
    placeKnobTopLeft (feedbackASlider, devA_knobCol2_X, devA_knobRow1_Y);
    placeKnobTopLeft (panASlider,      devA_knobCol3_X, devA_knobRow1_Y);
    placeKnobTopLeft (levelASlider,    devA_knobCol4_X, devA_knobRow1_Y);

    placeKnobTopLeft (hpfASlider,      devA_HPF_X,      devA_knobRow2_Y);
    placeKnobTopLeft (lpfASlider,      devA_LPF_X,      devA_knobRow2_Y);

    placeKnobTopLeft (lfoRateASlider,  devA_lfoRate_X,  devA_knobRow2_Y);
    placeKnobTopLeft (lfoDepthASlider, devA_lfoDepth_X, devA_knobRow2_Y);
    // Device B knobs
    placeKnobTopLeft (preDelayBSlider, devB_knobCol1_X, devB_knobRow1_Y);
    placeKnobTopLeft (feedbackBSlider, devB_knobCol2_X, devB_knobRow1_Y);
    placeKnobTopLeft (panBSlider,      devB_knobCol3_X, devB_knobRow1_Y);
    placeKnobTopLeft (levelBSlider,    devB_knobCol4_X, devB_knobRow1_Y);

    placeKnobTopLeft (hpfBSlider,      devB_HPF_X,      devB_knobRow2_Y);
    placeKnobTopLeft (lpfBSlider,      devB_LPF_X,      devB_knobRow2_Y);

    placeKnobTopLeft (lfoRateBSlider,  devB_lfoRate_X,  devB_knobRow2_Y);
    placeKnobTopLeft (lfoDepthBSlider, devB_lfoDepth_X, devB_knobRow2_Y);
    // Bottom row: these are defined using a centre-X in UILayout

    placeKnobTopLeft (inputGainSlider,  bottomKnobInX, bottomKnobY);
    placeKnobTopLeft (dryWetSlider,     bottomKnobCX - 25.0f, bottomKnobY);
    placeKnobTopLeft (outputGainSlider, bottomKnobOutX, bottomKnobY);

    // Level indicators (top-left)
    const int inputX  = toX (indicatorX);
    const int inputY  = toY (indicatorY);
    const int outputX = toX (canvasW - indicatorX - indicatorW);

    inputIndicator.setBounds  (inputX,  inputY, indPxW, indPxH);
    outputIndicator.setBounds (outputX, inputY, indPxW, indPxH);

    // Choose design-space coords and size for the flame
    constexpr float flameX = 672.0f;
    constexpr float flameY = 463.0f;
    constexpr float flameW = 25.0f;  // full strip width
    constexpr float flameH = 66.0f;

    clipFlameFilmstrip.setBounds (
        toX (flameX),
        toY (flameY),
        int (flameW * s),
        int (flameH * s));

    // Routing column
    const int routeCellPx = (int) std::round (routingCellSize * s); // 40 * s
    const int routeGapPx  = (int) std::round (routingGapY     * s); // 10 * s;

    const int routeColumnH = 3 * routeCellPx + 2 * routeGapPx;      // 3*40 + 2*10

    routingColumn.setBounds (toX (routingCenterX) - routeCellPx / 2,
                            toY (baseRoutingTopY),
                            routeCellPx,
                            routeColumnH);

    // Enable LEDs (top-left)
    const int enableAX_px = toX (enableAX);
    const int enableAY_px = toY (enableAY);
    const int enableBX_px = toX (canvasW - enableAX - enableLedW);

    enableAButton.setBounds (enableAX_px, enableAY_px, ledPxW, ledPxH);
    enableBButton.setBounds (enableBX_px, enableAY_px, ledPxW, ledPxH);
}

void MidiverbAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem (1, juce::String ("Build: ") + __DATE__ + " " + __TIME__, false);
        menu.showMenuAsync (juce::PopupMenu::Options());
    }
}
