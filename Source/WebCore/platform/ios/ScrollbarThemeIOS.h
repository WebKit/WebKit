/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ScrollbarThemeIOS_h
#define ScrollbarThemeIOS_h

#include "ScrollbarThemeComposite.h"

namespace WebCore {

class ScrollbarThemeIOS : public ScrollbarThemeComposite {
public:
    ScrollbarThemeIOS();
    virtual ~ScrollbarThemeIOS();

    virtual bool paint(ScrollbarThemeClient*, GraphicsContext*, const IntRect& damageRect) override;

    virtual int scrollbarThickness(ScrollbarControlSize = RegularScrollbar) override;
    
    virtual bool supportsControlTints() const override { return true; }

    virtual double initialAutoscrollTimerDelay() override;
    virtual double autoscrollTimerDelay() override;

    virtual ScrollbarButtonsPlacement buttonsPlacement() const override;

    virtual void registerScrollbar(ScrollbarThemeClient*) override;
    virtual void unregisterScrollbar(ScrollbarThemeClient*) override;

protected:
    virtual bool hasButtons(ScrollbarThemeClient*) override;
    virtual bool hasThumb(ScrollbarThemeClient*) override;

    virtual IntRect backButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool painting = false) override;
    virtual IntRect forwardButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool painting = false) override;
    virtual IntRect trackRect(ScrollbarThemeClient*, bool painting = false) override;

    virtual int minimumThumbLength(ScrollbarThemeClient*) override;
    
    virtual bool shouldCenterOnThumb(ScrollbarThemeClient*, const PlatformMouseEvent&) override;
    
public:
    void preferencesChanged();
};

}

#endif // ScrollbarThemeIOS_h
