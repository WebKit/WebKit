/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright 2009 Maxime Simon <simon.maxime@gmail.com> All Rights Reserved
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

#ifndef ScrollbarThemeHaiku_h
#define ScrollbarThemeHaiku_h

#include "ScrollbarThemeComposite.h"

namespace WebCore {
class Scrollbar;

class ScrollbarThemeHaiku : public ScrollbarThemeComposite {
public:
    ScrollbarThemeHaiku(bool drawOuterFrame);
    virtual ~ScrollbarThemeHaiku();

    int scrollbarThickness(ScrollbarControlSize = ScrollbarControlSize::Regular,
			ScrollbarExpansionState = ScrollbarExpansionState::Expanded) override;

    bool hasButtons(Scrollbar&) override;
    bool hasThumb(Scrollbar&) override;

    IntRect backButtonRect(Scrollbar&, ScrollbarPart, bool painting) override;
    IntRect forwardButtonRect(Scrollbar&, ScrollbarPart, bool painting) override;
    IntRect trackRect(Scrollbar&, bool painting) override;

    void paintScrollbarBackground(GraphicsContext&, Scrollbar&) override;
    void paintButton(GraphicsContext&, Scrollbar&, const IntRect&, ScrollbarPart) override;
    void paintThumb(GraphicsContext&, Scrollbar&, const IntRect&) override;

    void paintScrollCorner(GraphicsContext&, const IntRect&t) override;

private:
    bool m_drawOuterFrame;
};

}
#endif
