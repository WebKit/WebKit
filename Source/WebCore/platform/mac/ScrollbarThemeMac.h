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

#ifndef ScrollbarThemeMac_h
#define ScrollbarThemeMac_h

#include "ScrollbarThemeComposite.h"

namespace WebCore {

class ScrollbarThemeMac : public ScrollbarThemeComposite {
public:
    ScrollbarThemeMac();
    virtual ~ScrollbarThemeMac();

    void preferencesChanged();

    virtual void updateEnabledState(Scrollbar&) override;

    virtual bool paint(Scrollbar&, GraphicsContext&, const IntRect& damageRect) override;

    virtual int scrollbarThickness(ScrollbarControlSize = RegularScrollbar) override;
    
    virtual bool supportsControlTints() const override { return true; }
    virtual bool usesOverlayScrollbars() const  override;
    virtual void usesOverlayScrollbarsChanged() override;
    virtual void updateScrollbarOverlayStyle(Scrollbar&)  override;

    virtual double initialAutoscrollTimerDelay() override;
    virtual double autoscrollTimerDelay() override;

    virtual ScrollbarButtonsPlacement buttonsPlacement() const override;

    virtual void registerScrollbar(Scrollbar&) override;
    virtual void unregisterScrollbar(Scrollbar&) override;

    void setNewPainterForScrollbar(Scrollbar&, ScrollbarPainter);
    ScrollbarPainter painterForScrollbar(Scrollbar&);

    void setPaintCharacteristicsForScrollbar(Scrollbar&);

    static bool isCurrentlyDrawingIntoLayer();
    static void setIsCurrentlyDrawingIntoLayer(bool);

#if ENABLE(RUBBER_BANDING)
    WEBCORE_EXPORT static void setUpOverhangAreaBackground(CALayer *, const Color& customBackgroundColor = Color());
    WEBCORE_EXPORT static void removeOverhangAreaBackground(CALayer *);

    WEBCORE_EXPORT static void setUpOverhangAreaShadow(CALayer *);
    WEBCORE_EXPORT static void removeOverhangAreaShadow(CALayer *);
#endif

protected:
    virtual bool hasButtons(Scrollbar&) override;
    virtual bool hasThumb(Scrollbar&) override;

    virtual IntRect backButtonRect(Scrollbar&, ScrollbarPart, bool painting = false) override;
    virtual IntRect forwardButtonRect(Scrollbar&, ScrollbarPart, bool painting = false) override;
    virtual IntRect trackRect(Scrollbar&, bool painting = false) override;

    virtual int maxOverlapBetweenPages() override { return 40; }

    virtual int minimumThumbLength(Scrollbar&) override;
    
    virtual ScrollbarButtonPressAction handleMousePressEvent(Scrollbar&, const PlatformMouseEvent&, ScrollbarPart) override;
    virtual bool shouldDragDocumentInsteadOfThumb(Scrollbar&, const PlatformMouseEvent&) override;
    int scrollbarPartToHIPressedState(ScrollbarPart);

#if ENABLE(RUBBER_BANDING)
    virtual void setUpOverhangAreasLayerContents(GraphicsLayer*, const Color&) override;
    virtual void setUpContentShadowLayer(GraphicsLayer*) override;
#endif
};

}

#endif
