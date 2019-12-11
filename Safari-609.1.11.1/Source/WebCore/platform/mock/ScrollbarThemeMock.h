/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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

#ifndef ScrollbarThemeMock_h
#define ScrollbarThemeMock_h

#include "ScrollbarThemeComposite.h"

namespace WebCore {

// Scrollbar theme used in image snapshots, to eliminate appearance differences between platforms.
class ScrollbarThemeMock : public ScrollbarThemeComposite {
public:
    int scrollbarThickness(ScrollbarControlSize = RegularScrollbar, ScrollbarExpansionState = ScrollbarExpansionState::Expanded) override;

protected:
    bool hasButtons(Scrollbar&) override { return false; }
    bool hasThumb(Scrollbar&) override  { return true; }

    IntRect backButtonRect(Scrollbar&, ScrollbarPart, bool /*painting*/ = false) override { return IntRect(); }
    IntRect forwardButtonRect(Scrollbar&, ScrollbarPart, bool /*painting*/ = false) override { return IntRect(); }
    IntRect trackRect(Scrollbar&, bool painting = false) override;
    
    void paintTrackBackground(GraphicsContext&, Scrollbar&, const IntRect&) override;
    void paintThumb(GraphicsContext&, Scrollbar&, const IntRect&) override;
    int maxOverlapBetweenPages() override { return 40; }

    bool usesOverlayScrollbars() const override;
private:
    bool isMockTheme() const override { return true; }
};

}
#endif // ScrollbarThemeMock_h
