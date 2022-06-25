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

#ifndef ScrollbarThemeWin_h
#define ScrollbarThemeWin_h

#include "ScrollbarThemeComposite.h"

namespace WebCore {

class ScrollbarThemeWin : public ScrollbarThemeComposite {
public:
    ScrollbarThemeWin();
    virtual ~ScrollbarThemeWin();

    int scrollbarThickness(ScrollbarControlSize = ScrollbarControlSize::Regular, ScrollbarExpansionState = ScrollbarExpansionState::Expanded) override;

    void themeChanged() override;
    
    bool invalidateOnMouseEnterExit() override;

    IntRect backButtonRect(Scrollbar&, ScrollbarPart, bool painting = false) override;
    IntRect forwardButtonRect(Scrollbar&, ScrollbarPart, bool painting = false) override;
    IntRect trackRect(Scrollbar&, bool painting = false) override;

protected:
    bool hasButtons(Scrollbar&) override { return true; }
    bool hasThumb(Scrollbar&) override;

    ScrollbarButtonPressAction handleMousePressEvent(Scrollbar&, const PlatformMouseEvent&, ScrollbarPart) override;
    bool shouldSnapBackToDragOrigin(Scrollbar&, const PlatformMouseEvent&) override;

    void paintTrackBackground(GraphicsContext&, Scrollbar&, const IntRect&) override;
    void paintTrackPiece(GraphicsContext&, Scrollbar&, const IntRect&, ScrollbarPart) override;
    void paintButton(GraphicsContext&, Scrollbar&, const IntRect&, ScrollbarPart) override;
    void paintThumb(GraphicsContext&, Scrollbar&, const IntRect&) override;
};

}
#endif
