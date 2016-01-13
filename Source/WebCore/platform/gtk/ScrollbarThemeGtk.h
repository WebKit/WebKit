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

#ifndef ScrollbarThemeGtk_h
#define ScrollbarThemeGtk_h

#include "ScrollbarThemeComposite.h"
#include <wtf/glib/GRefPtr.h>

namespace WebCore {

class Scrollbar;

class ScrollbarThemeGtk final : public ScrollbarThemeComposite {
public:
    virtual ~ScrollbarThemeGtk();

    virtual bool hasButtons(Scrollbar&) override { return true; }
    virtual bool hasThumb(Scrollbar&) override;
    virtual IntRect backButtonRect(Scrollbar&, ScrollbarPart, bool) override;
    virtual IntRect forwardButtonRect(Scrollbar&, ScrollbarPart, bool) override;
    virtual IntRect trackRect(Scrollbar&, bool) override;

#ifndef GTK_API_VERSION_2
    ScrollbarThemeGtk();

    using ScrollbarThemeComposite::thumbRect;
    IntRect thumbRect(Scrollbar&, const IntRect& unconstrainedTrackRect);
    virtual bool paint(Scrollbar&, GraphicsContext&, const IntRect& damageRect) override;
    virtual void paintScrollbarBackground(GraphicsContext&, Scrollbar&) override;
    virtual void paintTrackBackground(GraphicsContext&, Scrollbar&, const IntRect&) override;
    virtual void paintThumb(GraphicsContext&, Scrollbar&, const IntRect&) override;
    virtual void paintButton(GraphicsContext&, Scrollbar&, const IntRect&, ScrollbarPart) override;
    virtual bool shouldCenterOnThumb(Scrollbar&, const PlatformMouseEvent&) override;
    virtual int scrollbarThickness(ScrollbarControlSize) override;
    virtual int minimumThumbLength(Scrollbar&) override;

    // TODO: These are the default GTK+ values. At some point we should pull these from the theme itself.
    virtual double initialAutoscrollTimerDelay() override { return 0.20; }
    virtual double autoscrollTimerDelay() override { return 0.02; }
    virtual void themeChanged() override;

private:
    void updateThemeProperties();
    GRefPtr<GtkStyleContext> getOrCreateStyleContext(ScrollbarOrientation = VerticalScrollbar);

    IntSize buttonSize(Scrollbar&);

    struct Properties {
        int thumbFatness;
        int troughBorderWidth;
        int stepperSize;
        int stepperSpacing;
        gboolean troughUnderSteppers;
    };

    GRefPtr<GtkStyleContext> m_cachedStyleContext;
    Properties m_cachedProperties;
    int m_minThumbLength;
    gboolean m_hasForwardButtonStartPart;
    gboolean m_hasForwardButtonEndPart;
    gboolean m_hasBackButtonStartPart;
    gboolean m_hasBackButtonEndPart;
#endif // GTK_API_VERSION_2
};

}

#endif
