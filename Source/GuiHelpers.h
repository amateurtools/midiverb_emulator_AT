#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

//==============================================================================
// ComboBoxLookAndFeel
// Draws a clean dark combo with no border, using your background theme
//==============================================================================
struct MidiverbComboLAF : public juce::LookAndFeel_V4
{
    juce::Typeface::Ptr customTypeface;
    
    MidiverbComboLAF()
    {
        customTypeface = juce::Typeface::createSystemTypefaceFor(
            BinaryData::AnotherMansTreasureMIBRaw_ttf,
            BinaryData::AnotherMansTreasureMIBRaw_ttfSize);
        
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xfff0f5ed));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xfff0f5ed));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1c4490));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xfff0f5ed));
        
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1c4490));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xfff0f5ed));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xfff0f5ed));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xff1c4490));
    }
    
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                    int buttonX, int buttonY, int buttonW, int buttonH, 
                    juce::ComboBox& box) override
    {
        juce::ignoreUnused(buttonX, buttonY, buttonW, buttonH, isButtonDown);
        
        juce::Rectangle<float> boxBounds(0.0f, 0.0f, (float)width, (float)height);
        
        // No background, no border for the ComboBox label - just draw the arrow
        juce::Path path;
        auto arrowZone = boxBounds.removeFromRight(20.0f).reduced(4.0f, 4.0f);
        path.addTriangle(arrowZone.getX(), arrowZone.getCentreY() - 3.0f,
                        arrowZone.getRight(), arrowZone.getCentreY() - 3.0f,
                        arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
        
        g.setColour(findColour(juce::ComboBox::arrowColourId));
        g.fillPath(path);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(5, 3, box.getWidth() - 25, box.getHeight());
        label.setFont(juce::Font(juce::FontOptions(customTypeface).withHeight(18.0f)));
        label.setJustificationType(juce::Justification::centredLeft);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::Font(juce::FontOptions(customTypeface).withHeight(18.0f));
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font(juce::FontOptions(customTypeface).withHeight(18.0f));
    }

    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                int standardMenuItemHeight, int& idealWidth, int& idealHeight) override
    {
        if (isSeparator)
        {
            idealHeight = 10;
            idealWidth  = 0;
        }
        else
        {
            auto font = juce::Font(juce::FontOptions(customTypeface).withHeight(18.0f));
            idealHeight = 28;
            idealWidth = (int) juce::GlyphArrangement::getStringWidth(font, text) + 50;
        }
    }

    // Keep the dropdown background
    void drawPopupMenuBackgroundWithOptions(juce::Graphics& g, int width, int height,
                                        const juce::PopupMenu::Options& options) override
    {
        juce::ignoreUnused (width, height, options);
        g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    }
};



//==============================================================================
// SignalIndicator
// 3-state PNG indicator (horizontal filmstrip: silence / signal / clip)
// signal_indicator.png: 3 cells wide, 1 cell high
//==============================================================================
class SignalIndicator : public juce::Component
{
public:
    enum State { Silence = 0, Signal = 1, Clip = 2 };

    void setState (State s)
    {
        if (s == state) return;
        state = s;
        repaint();
    }

    void setImage (juce::Image img) { strip = img; repaint(); }

void paint (juce::Graphics& g) override
{
    if (! strip.isValid())
    {
        // fallback
        g.setColour (state == Clip   ? juce::Colours::red
                   : state == Signal ? juce::Colours::green
                                     : juce::Colours::darkgrey);
        g.fillEllipse (getLocalBounds().toFloat().reduced (2.0f));
        return;
    }

    int srcW = strip.getWidth() / 3;
    juce::Rectangle<int> srcRect ((int)state * srcW, 0, srcW, strip.getHeight());
    juce::Rectangle<int> destRect = getLocalBounds();

    auto d = destRect;
    auto s = srcRect;

    g.drawImage (strip,
                 d.getX(), d.getY(), d.getWidth(), d.getHeight(),
                 s.getX(), s.getY(), s.getWidth(), s.getHeight());
}


private:
    State state = Silence;
    juce::Image strip;
};

class RandomFilmstrip : public juce::Component,
                        private juce::Timer
{
public:
    RandomFilmstrip()
    {
        animating = false;          // start off
        currentFrame = 0;
        setVisible (false);         // start hidden
    }

    void setImage (juce::Image img, int numFrames)
    {
        filmstrip = img;
        frames    = numFrames;
        currentFrame = 0;
        repaint();
    }

    void setShouldAnimate (bool should)
    {
        if (should == animating)
            return;

        animating = should;

        if (animating)
        {
            setVisible (true);
            startTimerHz (10);      // animation rate
        }
        else
        {
            stopTimer();
            currentFrame = 0;       // idle frame if you want
            setVisible (false);     // hide when not animating
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        // Extra safety: if not animating, draw nothing
        if (! animating || ! filmstrip.isValid() || frames <= 0)
            return;

        const int frameW = filmstrip.getWidth() / frames; // 25
        const int frameH = filmstrip.getHeight();         // 66

        juce::Rectangle<int> src (currentFrame * frameW, 0, frameW, frameH);

        auto bounds = getLocalBounds();
        const int destX = bounds.getX() + (bounds.getWidth()  - frameW) / 2;
        const int destY = bounds.getY() + (bounds.getHeight() - frameH) / 2;

        g.drawImage (filmstrip,
                     destX, destY, frameW, frameH,
                     src.getX(), src.getY(), src.getWidth(), src.getHeight());
    }

private:
    void timerCallback() override
    {
        if (! animating || frames <= 0)
            return;

        int next = currentFrame;
        while (next == currentFrame && frames > 1)
            next = juce::Random::getSystemRandom().nextInt (frames);

        currentFrame = next;
        repaint();
    }

    juce::Image filmstrip;
    int   frames       = 0;
    int   currentFrame = 0;
    bool  animating    = false;
};


//==============================================================================
// Helper: load the three images and wire up RoutingToggle + APVTS
//==============================================================================
class RoutingColumn : public juce::Component
{
public:
    // 0 = Parallel, 1 = A→B, 2 = B→A
    int getValue() const { return currentValue; }

    void setValue (int v, juce::NotificationType notify = juce::sendNotification)
    {
        v = juce::jlimit (0, 2, v);
        if (v == currentValue) return;
        currentValue = v;
        repaint();
        if (notify == juce::sendNotification && onChange)
            onChange();
    }

    std::function<void()> onChange;

    // order: 0 = parallel, 1 = right, 2 = left
    void setImages (juce::Image parallel, juce::Image right, juce::Image left)
    {
        images[0] = parallel;
        images[1] = right;
        images[2] = left;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const int buttonSize = 40;
        const int gap        = 10;

        const int cellW = buttonSize;
        const int cellH = buttonSize;

        for (int i = 0; i < 3; ++i)
        {
            auto& img = images[i];
            if (! img.isValid())
                continue;

            int y = i * (cellH + gap);   // 40, 40+10+40, etc.

            juce::Rectangle<int> destRect (0, y, cellW, cellH);

            int frame = 0;
            if (i == currentValue)      frame = 2;
            else if (i == pressedIndex) frame = 1;

            int srcW = img.getWidth() / 3;
            juce::Rectangle<int> srcRect (frame * srcW, 0, srcW, img.getHeight());

            g.drawImage (img,
                        destRect.getX(), destRect.getY(),
                        destRect.getWidth(), destRect.getHeight(),
                        srcRect.getX(), srcRect.getY(),
                        srcRect.getWidth(), srcRect.getHeight());
        }
    }

    int yToIndex (int y) const
    {
        const int buttonSize = 40;
        const int gap        = 10;
        const int cellSpan   = buttonSize + gap; // 50

        int idx = y / cellSpan;
        if (idx < 0 || idx > 2)
            return -1;

        int localY = y - idx * cellSpan;
        if (localY < 0 || localY >= buttonSize)
            return -1; // in the gap

        return idx;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        pressedIndex = yToIndex (e.y);
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        int released = yToIndex (e.y);

        if (released >= 0 && released == pressedIndex)
            setValue (released);

        pressedIndex = -1;
        repaint();
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        pressedIndex = -1;
        repaint();
    }

private:
    int currentValue  = 0;   // which routing option is active
    int pressedIndex  = -1;  // which cell is currently pressed (for frame=1)
    juce::Image images[3];
};

class LedToggleButton : public juce::Button
{
public:
    LedToggleButton() : juce::Button ("")
    {
        setClickingTogglesState (true);
    }

    void setImage (juce::Image img) { strip = img; repaint(); }

    void paintButton (juce::Graphics& g,
                      bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override
    {
        if (! strip.isValid())
        {
            g.fillAll (juce::Colours::darkgrey);
            return;
        }

        // 3 frames in the filmstrip: 0 = off, 1 = pressed, 2 = on
        const int numFrames = 3;

        int frame = 0;

        if (getToggleState())
        {
            // On state: use "on" frame, but if mouse is currently down, show pressed frame
            frame = shouldDrawButtonAsDown ? 1 : 2;
        }
        else
        {
            // Off state: use "off" frame, but if mouse is currently down, show pressed frame
            frame = shouldDrawButtonAsDown ? 1 : 0;
        }

        if (shouldDrawButtonAsHighlighted && ! shouldDrawButtonAsDown)
            g.setOpacity (0.9f);
        else
            g.setOpacity (1.0f);

        int srcW = strip.getWidth() / numFrames;
        juce::Rectangle<int> srcRect (frame * srcW, 0, srcW, strip.getHeight());
        juce::Rectangle<int> destRect = getLocalBounds();

        g.drawImage (strip,
                     destRect.getX(), destRect.getY(),
                     destRect.getWidth(), destRect.getHeight(),
                     srcRect.getX(), srcRect.getY(),
                     srcRect.getWidth(), srcRect.getHeight());
    }

private:
    juce::Image strip;
};


