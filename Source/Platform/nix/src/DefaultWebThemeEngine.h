/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DefaultWebThemeEngine_h
#define DefaultWebThemeEngine_h

#include "public/ThemeEngine.h"

namespace Nix {

class DefaultWebThemeEngine FINAL : public ThemeEngine {
public:
    // Text selection colors.
    virtual Color activeSelectionBackgroundColor() const OVERRIDE;
    virtual Color activeSelectionForegroundColor() const OVERRIDE;
    virtual Color inactiveSelectionBackgroundColor() const OVERRIDE;
    virtual Color inactiveSelectionForegroundColor() const OVERRIDE;

    // List box selection colors
    virtual Color activeListBoxSelectionBackgroundColor() const OVERRIDE;
    virtual Color activeListBoxSelectionForegroundColor() const OVERRIDE;
    virtual Color inactiveListBoxSelectionBackgroundColor() const OVERRIDE;
    virtual Color inactiveListBoxSelectionForegroundColor() const OVERRIDE;

    virtual Color activeTextSearchHighlightColor() const OVERRIDE;
    virtual Color inactiveTextSearchHighlightColor() const OVERRIDE;

    virtual Color focusRingColor() const OVERRIDE;

    virtual Color tapHighlightColor() const OVERRIDE;

    virtual void paintButton(Canvas*, State, const Rect&, const ButtonExtraParams&) const OVERRIDE;
    virtual void paintTextField(Canvas*, State, const Rect&) const OVERRIDE;
    virtual void paintTextArea(Canvas*, State, const Rect&) const OVERRIDE;
    virtual Size getCheckboxSize() const OVERRIDE;
    virtual void paintCheckbox(Canvas*, State, const Rect&, const ButtonExtraParams&) const OVERRIDE;
    virtual Size getRadioSize() const OVERRIDE;
    virtual void paintRadio(Canvas*, State, const Rect&, const ButtonExtraParams&) const OVERRIDE;
    virtual void paintMenuList(Canvas *, State, const Rect &) const OVERRIDE;
    virtual void getMenuListPadding(int& paddingTop, int& paddingLeft, int& paddingBottom, int& paddingRight) const OVERRIDE;
    virtual void paintProgressBar(Canvas*, State, const Rect&, const ProgressBarExtraParams&) const OVERRIDE;
    virtual double getAnimationRepeatIntervalForProgressBar() const OVERRIDE;
    virtual double getAnimationDurationForProgressBar() const OVERRIDE;
    virtual void paintInnerSpinButton(Canvas *, State, const Rect &, const InnerSpinButtonExtraParams&) const OVERRIDE;
    virtual void getInnerSpinButtonPadding(int& paddingTop, int& paddingLeft, int& paddingBottom, int& paddingRight) const OVERRIDE;
    virtual void paintMeter(Canvas*, State, const Rect&, const MeterExtraParams&) const OVERRIDE;
    virtual void paintSliderTrack(Canvas*, State, const Rect&) const OVERRIDE;
    virtual void paintSliderThumb(Canvas*, State, const Rect&) const OVERRIDE;

    // Media Player
    virtual void paintMediaPlayButton(Canvas*, MediaPlayerState, const Rect&) const OVERRIDE;
    virtual void paintMediaMuteButton(Canvas*, MediaPlayerState, const Rect&) const OVERRIDE;
    virtual void paintMediaSeekBackButton(Canvas*, const Rect&) const OVERRIDE;
    virtual void paintMediaSeekForwardButton(Canvas*, const Rect&) const OVERRIDE;
    virtual void paintMediaRewindButton(Canvas*, const Rect&) const OVERRIDE;
};

} // namespace Nix

#endif // DefaultWebThemeEngine_h
