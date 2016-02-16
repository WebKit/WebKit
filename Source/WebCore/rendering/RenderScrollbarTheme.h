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

#ifndef RenderScrollbarTheme_h
#define RenderScrollbarTheme_h

#include "ScrollbarThemeComposite.h"

namespace WebCore {

class PlatformMouseEvent;
class Scrollbar;
class ScrollView;

class RenderScrollbarTheme final : public ScrollbarThemeComposite {
public:
    virtual ~RenderScrollbarTheme() { }
    
    virtual int scrollbarThickness(ScrollbarControlSize controlSize) override { return ScrollbarTheme::theme().scrollbarThickness(controlSize); }

    virtual ScrollbarButtonsPlacement buttonsPlacement() const override { return ScrollbarTheme::theme().buttonsPlacement(); }

    virtual bool supportsControlTints() const override { return true; }

    virtual void paintScrollCorner(ScrollView*, GraphicsContext&, const IntRect& cornerRect) override;

    virtual ScrollbarButtonPressAction handleMousePressEvent(Scrollbar& scrollbar, const PlatformMouseEvent& event, ScrollbarPart pressedPart) override { return ScrollbarTheme::theme().handleMousePressEvent(scrollbar, event, pressedPart); }

    virtual double initialAutoscrollTimerDelay() override { return ScrollbarTheme::theme().initialAutoscrollTimerDelay(); }
    virtual double autoscrollTimerDelay() override { return ScrollbarTheme::theme().autoscrollTimerDelay(); }

    virtual void registerScrollbar(Scrollbar& scrollbar) override { return ScrollbarTheme::theme().registerScrollbar(scrollbar); }
    virtual void unregisterScrollbar(Scrollbar& scrollbar) override { return ScrollbarTheme::theme().unregisterScrollbar(scrollbar); }

    virtual int minimumThumbLength(Scrollbar&) override;

    void buttonSizesAlongTrackAxis(Scrollbar&, int& beforeSize, int& afterSize);
    
    static RenderScrollbarTheme* renderScrollbarTheme();

protected:
    virtual bool hasButtons(Scrollbar&) override;
    virtual bool hasThumb(Scrollbar&) override;

    virtual IntRect backButtonRect(Scrollbar&, ScrollbarPart, bool painting = false) override;
    virtual IntRect forwardButtonRect(Scrollbar&, ScrollbarPart, bool painting = false) override;
    virtual IntRect trackRect(Scrollbar&, bool painting = false) override;

    virtual void willPaintScrollbar(GraphicsContext&, Scrollbar&) override;
    virtual void didPaintScrollbar(GraphicsContext&, Scrollbar&) override;
    
    virtual void paintScrollbarBackground(GraphicsContext&, Scrollbar&) override;
    virtual void paintTrackBackground(GraphicsContext&, Scrollbar&, const IntRect&) override;
    virtual void paintTrackPiece(GraphicsContext&, Scrollbar&, const IntRect&, ScrollbarPart) override;
    virtual void paintButton(GraphicsContext&, Scrollbar&, const IntRect&, ScrollbarPart) override;
    virtual void paintThumb(GraphicsContext&, Scrollbar&, const IntRect&) override;
    virtual void paintTickmarks(GraphicsContext&, Scrollbar&, const IntRect&) override;

    virtual IntRect constrainTrackRectToTrackPieces(Scrollbar&, const IntRect&) override;
};

} // namespace WebCore

#endif // RenderScrollbarTheme_h
