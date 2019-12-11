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

#pragma once

#include "ScrollbarThemeComposite.h"

namespace WebCore {

class Scrollbar;

class ScrollbarThemeGtk final : public ScrollbarThemeComposite {
public:
    virtual ~ScrollbarThemeGtk();

    bool hasButtons(Scrollbar&) override;
    bool hasThumb(Scrollbar&) override;
    IntRect backButtonRect(Scrollbar&, ScrollbarPart, bool) override;
    IntRect forwardButtonRect(Scrollbar&, ScrollbarPart, bool) override;
    IntRect trackRect(Scrollbar&, bool) override;

    ScrollbarThemeGtk();

    bool paint(Scrollbar&, GraphicsContext&, const IntRect& damageRect) override;
    ScrollbarButtonPressAction handleMousePressEvent(Scrollbar&, const PlatformMouseEvent&, ScrollbarPart) override;
    int scrollbarThickness(ScrollbarControlSize, ScrollbarExpansionState = ScrollbarExpansionState::Expanded) override;
    int minimumThumbLength(Scrollbar&) override;

    // TODO: These are the default GTK+ values. At some point we should pull these from the theme itself.
    Seconds initialAutoscrollTimerDelay() override { return 200_ms; }
    Seconds autoscrollTimerDelay() override { return 20_ms; }
    void themeChanged() override;
    bool usesOverlayScrollbars() const override { return m_usesOverlayScrollbars; }
    // When using overlay scrollbars, always invalidate the whole scrollbar when entering/leaving.
    bool invalidateOnMouseEnterExit() override { return m_usesOverlayScrollbars; }

private:
    void updateThemeProperties();

    bool m_hasForwardButtonStartPart : 1;
    bool m_hasForwardButtonEndPart : 1;
    bool m_hasBackButtonStartPart : 1;
    bool m_hasBackButtonEndPart : 1;
    bool m_usesOverlayScrollbars { false };
};

}

