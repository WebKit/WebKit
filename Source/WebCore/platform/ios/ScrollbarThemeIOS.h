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

    virtual bool paint(ScrollbarThemeClient*, GraphicsContext*, const IntRect& damageRect) OVERRIDE;

    virtual int scrollbarThickness(ScrollbarControlSize = RegularScrollbar) OVERRIDE;
    
    virtual bool supportsControlTints() const OVERRIDE { return true; }

    virtual double initialAutoscrollTimerDelay() OVERRIDE;
    virtual double autoscrollTimerDelay() OVERRIDE;

    virtual ScrollbarButtonsPlacement buttonsPlacement() const OVERRIDE;

    virtual void registerScrollbar(ScrollbarThemeClient*) OVERRIDE;
    virtual void unregisterScrollbar(ScrollbarThemeClient*) OVERRIDE;

protected:
    virtual bool hasButtons(ScrollbarThemeClient*) OVERRIDE;
    virtual bool hasThumb(ScrollbarThemeClient*) OVERRIDE;

    virtual IntRect backButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool painting = false) OVERRIDE;
    virtual IntRect forwardButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool painting = false) OVERRIDE;
    virtual IntRect trackRect(ScrollbarThemeClient*, bool painting = false) OVERRIDE;

    virtual int minimumThumbLength(ScrollbarThemeClient*) OVERRIDE;
    
    virtual bool shouldCenterOnThumb(ScrollbarThemeClient*, const PlatformMouseEvent&) OVERRIDE;
    
public:
    void preferencesChanged();
};

}

#endif // ScrollbarThemeIOS_h
