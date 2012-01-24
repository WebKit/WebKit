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

namespace WebCore {

class Scrollbar;

class ScrollbarThemeGtk : public ScrollbarThemeComposite {
public:
    ScrollbarThemeGtk();
    virtual ~ScrollbarThemeGtk();

    virtual bool hasButtons(Scrollbar*) { return true; }
    virtual bool hasThumb(Scrollbar*);
    virtual IntRect backButtonRect(Scrollbar*, ScrollbarPart, bool);
    virtual IntRect forwardButtonRect(Scrollbar*, ScrollbarPart, bool);
    virtual IntRect trackRect(Scrollbar*, bool);
    IntRect thumbRect(Scrollbar*, const IntRect& unconstrainedTrackRect);
    bool paint(Scrollbar*, GraphicsContext*, const IntRect& damageRect);
    void paintScrollbarBackground(GraphicsContext*, Scrollbar*);
    void paintTrackBackground(GraphicsContext*, Scrollbar*, const IntRect&);
    void paintThumb(GraphicsContext*, Scrollbar*, const IntRect&);
    virtual void paintButton(GraphicsContext*, Scrollbar*, const IntRect&, ScrollbarPart);
    virtual bool shouldCenterOnThumb(Scrollbar*, const PlatformMouseEvent&);
    virtual int scrollbarThickness(ScrollbarControlSize);
    virtual IntSize buttonSize(Scrollbar*);
    virtual int minimumThumbLength(Scrollbar*);

    // TODO: These are the default GTK+ values. At some point we should pull these from the theme itself.
    virtual double initialAutoscrollTimerDelay() { return 0.20; }
    virtual double autoscrollTimerDelay() { return 0.02; }
    void updateThemeProperties();
    void updateScrollbarsFrameThickness();
    void registerScrollbar(Scrollbar*);
    void unregisterScrollbar(Scrollbar*);

protected:
#ifndef GTK_API_VERSION_2
    GtkStyleContext* m_context;
#endif
    int m_thumbFatness;
    int m_troughBorderWidth;
    int m_stepperSize;
    int m_stepperSpacing;
    int m_minThumbLength;
    gboolean m_troughUnderSteppers;
    gboolean m_hasForwardButtonStartPart;
    gboolean m_hasForwardButtonEndPart;
    gboolean m_hasBackButtonStartPart;
    gboolean m_hasBackButtonEndPart;
};

}
#endif
