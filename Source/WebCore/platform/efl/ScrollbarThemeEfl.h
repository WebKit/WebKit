/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
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

#ifndef ScrollbarThemeEfl_h
#define ScrollbarThemeEfl_h

#include "ScrollbarThemeComposite.h"

namespace WebCore {

class RenderThemeEfl;

class ScrollbarThemeEfl final : public ScrollbarThemeComposite {
public:
    ~ScrollbarThemeEfl();

    int scrollbarThickness(ScrollbarControlSize = RegularScrollbar) override;
    void setScrollbarThickness(int thickness) { m_scrollbarThickness = thickness; }

protected:
    bool usesOverlayScrollbars() const override;
    bool hasButtons(Scrollbar&) override { return false; }
    bool hasThumb(Scrollbar&) override;

    IntRect backButtonRect(Scrollbar&, ScrollbarPart, bool) override;
    IntRect forwardButtonRect(Scrollbar&, ScrollbarPart, bool) override;
    IntRect trackRect(Scrollbar&, bool) override;

    int minimumThumbLength(Scrollbar&) override;

    void paintTrackBackground(GraphicsContext&, Scrollbar&, const IntRect&) override;
    void paintThumb(GraphicsContext&, Scrollbar&, const IntRect&) override;

    void registerScrollbar(Scrollbar&) override;
    void unregisterScrollbar(Scrollbar&) override;

private:
    void loadThemeIfNeeded(Scrollbar&);

    RefPtr<RenderThemeEfl> m_theme;
    int m_scrollbarThickness;
};

}
#endif // ScrollbarThemeEfl_h

