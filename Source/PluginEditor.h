#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "GuiHelpers.h"


// ComboBox that responds to mouse wheel
class WheelComboBox : public juce::ComboBox
{
public:
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override
    {
        if (wheel.deltaY != 0.0f)
        {
            int newId = juce::jlimit (1, getNumItems(),
                                      getSelectedId() + (wheel.deltaY > 0.0f ? -1 : 1));
            if (newId != getSelectedId())
                setSelectedId (newId, juce::sendNotification);
            return;
        }
        juce::ComboBox::mouseWheelMove (event, wheel);
    }
};

// Rotary knob that draws a rotating PNG
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KnobLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
private:
    juce::Image knobImage;
};

class MidiverbAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit MidiverbAudioProcessorEditor (MidiverbAudioProcessor&);
    ~MidiverbAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void mouseDown (const juce::MouseEvent&) override;



private:
    void timerCallback() override;
    void updateProgramSelectorA();
    void updateProgramSelectorB();

    MidiverbAudioProcessor& audioProcessor;
    juce::Image             bgImage;
    juce::Image             tileImage;
    KnobLookAndFeel         knobLAF;

    std::unique_ptr<juce::ResizableCornerComponent> resizer;
    juce::ComponentBoundsConstrainer resizeConstrainer;

    MidiverbComboLAF comboLAF;

    // ── Device A ──────────────────────────────────────────
    WheelComboBox deviceASelector;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> deviceAAttachment;

    WheelComboBox programASelector;

    int lastDeviceAIndex  = -1;
    int lastProgramAIndex = -1;

    juce::Slider preDelayASlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preDelayAAttachment;

    juce::Slider feedbackASlider;  
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAAttachment;

    juce::Slider panASlider;      
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAAttachment;

    juce::Slider levelASlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAAttachment;

    juce::Slider lpfASlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpfAAttachment;

    juce::Slider hpfASlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpfAAttachment;

    juce::Slider lfoDepthASlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDepthAAttachment;

    juce::Slider lfoRateASlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAAttachment;

    // ── Device B ──────────────────────────────────────────
    WheelComboBox deviceBSelector;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> deviceBAttachment;

    WheelComboBox programBSelector;

    int lastDeviceBIndex  = -1;
    int lastProgramBIndex = -1;

    juce::Slider preDelayBSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preDelayBAttachment;

    juce::Slider feedbackBSlider; 
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackBAttachment;

    juce::Slider panBSlider;  
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panBAttachment;

    juce::Slider levelBSlider;    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelBAttachment;

    juce::Slider lpfBSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpfBAttachment;

    juce::Slider hpfBSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpfBAttachment;

    juce::Slider lfoDepthBSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDepthBAttachment;

    juce::Slider lfoRateBSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateBAttachment;

    // ── Global controls ───────────────────────────────────
    juce::Slider dryWetSlider; 
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dryWetAttachment;

    juce::Slider inputGainSlider; 
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;

    juce::Slider outputGainSlider; 
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;

    RoutingColumn routingColumn;
    std::unique_ptr<juce::ParameterAttachment> routingAttachment;


    SignalIndicator inputIndicator;
    SignalIndicator outputIndicator;
    RandomFilmstrip clipFlameFilmstrip;

    LedToggleButton enableAButton;
    LedToggleButton enableBButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableBAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiverbAudioProcessorEditor)
};
