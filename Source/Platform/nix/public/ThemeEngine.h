/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Nix_ThemeEngine_h
#define Nix_ThemeEngine_h

#include "Canvas.h"
#include "Color.h"
#include "Size.h"

namespace Nix {

class Rect;

class ThemeEngine {
public:
    // The current state of the associated Part.
    enum State {
        StateDisabled,
        StateHover,
        StateNormal,
        StatePressed
    };

    enum MediaPlayerState {
        StatePlaying,
        StatePaused,
        StateMuted,
        StateNotMuted
    };

    // Extra parameters for drawing the PartScrollbarHorizontalTrack and
    // PartScrollbarVerticalTrack.
    struct ScrollbarTrackExtraParams {
        // The bounds of the entire track, as opposed to the part being painted.
        int trackX;
        int trackY;
        int trackWidth;
        int trackHeight;
    };

    // Extra parameters for PartCheckbox, PartPushButton and PartRadio.
    struct ButtonExtraParams {
        bool checked;
        bool indeterminate; // Whether the button state is indeterminate.
        bool isDefault; // Whether the button is default button.
        bool hasBorder;
    };

    // Extra parameters for PartInnerSpinButton
    struct InnerSpinButtonExtraParams {
        bool spinUp;
        bool readOnly;
    };

    // Extra parameters for PartProgressBar
    struct ProgressBarExtraParams {
        bool isDeterminate;
        double position;
        double animationProgress;
        double animationStartTime;
    };

    struct MeterExtraParams {
        double min;
        double max;
        double value;
        double low;
        double high;
        double optimum;
        double valueRatio() const
        {
            if (max <= min)
                return 0;
            return (value - min) / (max - min);
        }
    };

    virtual const char* extraDefaultStyleSheet() const { return ""; }
    virtual const char* extraQuirksStyleSheet() const { return ""; }
    virtual const char* extraPlugInsStyleSheet() const { return ""; }

    // Text selection colors.
    virtual Color activeSelectionBackgroundColor() const = 0;
    virtual Color activeSelectionForegroundColor() const = 0;
    virtual Color inactiveSelectionBackgroundColor() const = 0;
    virtual Color inactiveSelectionForegroundColor() const = 0;

    // List box selection colors
    virtual Color activeListBoxSelectionBackgroundColor() const = 0;
    virtual Color activeListBoxSelectionForegroundColor() const = 0;
    virtual Color inactiveListBoxSelectionBackgroundColor() const = 0;
    virtual Color inactiveListBoxSelectionForegroundColor() const = 0;

    virtual Color activeTextSearchHighlightColor() const = 0;
    virtual Color inactiveTextSearchHighlightColor() const = 0;

    virtual Color focusRingColor() const = 0;

    virtual Color tapHighlightColor() const = 0;

    virtual void paintButton(Canvas*, State, const Rect&, const ButtonExtraParams&) const = 0;
    virtual void paintTextField(Canvas*, State, const Rect&) const = 0;
    virtual Size getCheckboxSize() const = 0;
    virtual void paintCheckbox(Canvas*, State, const Rect&, const ButtonExtraParams&) const = 0;
    virtual Size getRadioSize() const = 0;
    virtual void paintRadio(Canvas*, State, const Rect&, const ButtonExtraParams&) const = 0;
    virtual void paintTextArea(Canvas*, State, const Rect&) const = 0;
    virtual void getMenuListPadding(int& paddingTop, int& paddingLeft, int& paddingBottom, int& paddingRight) const = 0;
    virtual void paintMenuList(Canvas*, State, const Rect&) const = 0;
    virtual void paintProgressBar(Canvas*, State, const Rect&, const ProgressBarExtraParams&) const = 0;
    virtual double getAnimationRepeatIntervalForProgressBar() const = 0;
    virtual double getAnimationDurationForProgressBar() const = 0;
    virtual void getInnerSpinButtonPadding(int& paddingTop, int& paddingLeft, int& paddingBottom, int& paddingRight) const = 0;
    virtual void paintInnerSpinButton(Canvas*, State, const Rect&, const InnerSpinButtonExtraParams&) const = 0;
    virtual void paintMeter(Canvas*, State, const Rect&, const MeterExtraParams&) const = 0;
    virtual void paintSliderTrack(Canvas*, State, const Rect&) const = 0;
    virtual void paintSliderThumb(Canvas*, State, const Rect&) const = 0;

    // MediaPlayer (audio and video elements)
    virtual void paintMediaPlayButton(Canvas*, MediaPlayerState, const Rect&) const = 0;
    virtual void paintMediaMuteButton(Canvas*, MediaPlayerState, const Rect&) const = 0;
    virtual void paintMediaSeekBackButton(Canvas*, const Rect&) const = 0;
    virtual void paintMediaSeekForwardButton(Canvas*, const Rect&) const = 0;
    virtual void paintMediaRewindButton(Canvas*, const Rect&) const = 0;
};

} // namespace Nix

#endif // Nix_ThemeEngine_h
