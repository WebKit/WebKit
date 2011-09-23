/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2008, 2009 Google Inc.
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

#include "config.h"
#include "ScrollbarThemeChromium.h"

#include "PlatformMouseEvent.h"

#if ENABLE(RUBBER_BANDING)
#include "ScrollView.h"
#endif

#include "ScrollableArea.h"
#include "Scrollbar.h"
#include "ScrollbarThemeComposite.h"

// -----------------------------------------------------------------------------
// This file contains scrollbar theme code that is cross platform. Additional
// members of ScrollbarThemeChromium can be found in the platform specific files
// -----------------------------------------------------------------------------

namespace WebCore {

#if ENABLE(RUBBER_BANDING)
ScrollbarThemeChromium::ScrollbarThemeChromium()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        // Load the linen pattern image used for overhang drawing.
        RefPtr<Image> patternImage = Image::loadPlatformResource("overhangPattern");
        m_overhangPattern = Pattern::create(patternImage, true, true);
    }
}

ScrollbarThemeChromium::~ScrollbarThemeChromium()
{
}

void ScrollbarThemeChromium::paintOverhangAreas(ScrollView* view, GraphicsContext* context, const IntRect& horizontalOverhangRect, const IntRect& verticalOverhangRect, const IntRect& dirtyRect)
{
    // The extent of each shadow in pixels.
    const int kShadowSize = 4;
    // Offset of negative one pixel to make the gradient blend with the toolbar's bottom border.
    const int kToolbarShadowOffset = -1;
    const struct {
        float stop;
        Color color;
    } kShadowColors[] = {
        { 0.000, Color(0, 0, 0, 255) },
        { 0.125, Color(0, 0, 0, 57) },
        { 0.375, Color(0, 0, 0, 41) },
        { 0.625, Color(0, 0, 0, 18) },
        { 0.875, Color(0, 0, 0, 6) },
        { 1.000, Color(0, 0, 0, 0) }
    };
    const unsigned kNumShadowColors = sizeof(kShadowColors) / sizeof(kShadowColors[0]);

    const bool hasHorizontalOverhang = !horizontalOverhangRect.isEmpty();
    const bool hasVerticalOverhang = !verticalOverhangRect.isEmpty();
    // Prefer non-additive shadows, but degrade to additive shadows if there is vertical overhang.
    const bool useAdditiveShadows = hasVerticalOverhang;

    GraphicsContextStateSaver stateSaver(*context);

    context->setFillPattern(m_overhangPattern);
    if (hasHorizontalOverhang)
        context->fillRect(intersection(horizontalOverhangRect, dirtyRect));
    if (hasVerticalOverhang)
        context->fillRect(intersection(verticalOverhangRect, dirtyRect));

    IntSize scrollOffset = view->scrollOffset();
    FloatPoint shadowCornerOrigin;
    FloatPoint shadowCornerOffset;

    // Draw the shadow for the horizontal overhang.
    if (hasHorizontalOverhang) {
        int toolbarShadowHeight = kShadowSize;
        RefPtr<Gradient> gradient;
        IntRect shadowRect = horizontalOverhangRect;
        shadowRect.setHeight(kShadowSize);
        if (scrollOffset.height() < 0) {
            if (useAdditiveShadows) {
                toolbarShadowHeight = std::min(horizontalOverhangRect.height(), kShadowSize);
            } else if (horizontalOverhangRect.height() < 2 * kShadowSize + kToolbarShadowOffset) {
                // Split the overhang area between the web content shadow and toolbar shadow if it's too small.
                shadowRect.setHeight((horizontalOverhangRect.height() + 1) / 2);
                toolbarShadowHeight = horizontalOverhangRect.height() - shadowRect.height() - kToolbarShadowOffset;
            }
            shadowRect.setY(horizontalOverhangRect.maxY() - shadowRect.height());
            gradient = Gradient::create(FloatPoint(0, shadowRect.maxY()), FloatPoint(0, shadowRect.maxY() - kShadowSize));
            shadowCornerOrigin.setY(shadowRect.maxY());
            shadowCornerOffset.setY(-kShadowSize);
        } else {
            gradient = Gradient::create(FloatPoint(0, shadowRect.y()), FloatPoint(0, shadowRect.maxY()));
            shadowCornerOrigin.setY(shadowRect.y());
        }
        if (hasVerticalOverhang) {
            shadowRect.setWidth(shadowRect.width() - verticalOverhangRect.width());
            if (scrollOffset.width() < 0) {
                shadowRect.setX(shadowRect.x() + verticalOverhangRect.width());
                shadowCornerOrigin.setX(shadowRect.x());
                shadowCornerOffset.setX(-kShadowSize);
            } else
                shadowCornerOrigin.setX(shadowRect.maxX());
        }
        for (unsigned i = 0; i < kNumShadowColors; i++)
            gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
        context->setFillGradient(gradient);
        context->fillRect(intersection(shadowRect, dirtyRect));

        // Draw a drop-shadow from the toolbar.
        if (scrollOffset.height() < 0) {
            shadowRect.setY(kToolbarShadowOffset);
            shadowRect.setHeight(toolbarShadowHeight);
            gradient = Gradient::create(FloatPoint(0, shadowRect.y()), FloatPoint(0, shadowRect.y() + kShadowSize));
            for (unsigned i = 0; i < kNumShadowColors; i++)
                gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
            context->setFillGradient(gradient);
            context->fillRect(intersection(shadowRect, dirtyRect));
        }
    }

    // Draw the shadow for the vertical overhang.
    if (hasVerticalOverhang) {
        RefPtr<Gradient> gradient;
        IntRect shadowRect = verticalOverhangRect;
        shadowRect.setWidth(kShadowSize);
        if (scrollOffset.width() < 0) {
            shadowRect.setX(verticalOverhangRect.maxX() - shadowRect.width());
            gradient = Gradient::create(FloatPoint(shadowRect.maxX(), 0), FloatPoint(shadowRect.x(), 0));
        } else
            gradient = Gradient::create(FloatPoint(shadowRect.x(), 0), FloatPoint(shadowRect.maxX(), 0));

        for (unsigned i = 0; i < kNumShadowColors; i++)
            gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
        context->setFillGradient(gradient);
        context->fillRect(intersection(shadowRect, dirtyRect));

        // Draw a drop-shadow from the toolbar.
        shadowRect = verticalOverhangRect;
        shadowRect.setY(kToolbarShadowOffset);
        shadowRect.setHeight(kShadowSize);
        gradient = Gradient::create(FloatPoint(0, shadowRect.y()), FloatPoint(0, shadowRect.maxY()));
        for (unsigned i = 0; i < kNumShadowColors; i++)
            gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
        context->setFillGradient(gradient);
        context->fillRect(intersection(shadowRect, dirtyRect));
    }

    // If both rectangles present, draw a radial gradient for the corner.
    if (hasHorizontalOverhang && hasVerticalOverhang) {
        RefPtr<Gradient> gradient = Gradient::create(shadowCornerOrigin, 0, shadowCornerOrigin, kShadowSize);
        for (unsigned i = 0; i < kNumShadowColors; i++)
            gradient->addColorStop(kShadowColors[i].stop, kShadowColors[i].color);
        context->setFillGradient(gradient);
        context->fillRect(FloatRect(shadowCornerOrigin.x() + shadowCornerOffset.x(), shadowCornerOrigin.y() + shadowCornerOffset.y(), kShadowSize, kShadowSize));
    }
}
#endif

bool ScrollbarThemeChromium::hasThumb(Scrollbar* scrollbar)
{
    // This method is just called as a paint-time optimization to see if
    // painting the thumb can be skipped.  We don't have to be exact here.
    return thumbLength(scrollbar) > 0;
}

IntRect ScrollbarThemeChromium::backButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool)
{
    // Windows and Linux just have single arrows.
    if (part == BackButtonEndPart)
        return IntRect();

    IntSize size = buttonSize(scrollbar);
    return IntRect(scrollbar->x(), scrollbar->y(), size.width(), size.height());
}

IntRect ScrollbarThemeChromium::forwardButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool)
{
    // Windows and Linux just have single arrows.
    if (part == ForwardButtonStartPart)
        return IntRect();

    IntSize size = buttonSize(scrollbar);
    int x, y;
    if (scrollbar->orientation() == HorizontalScrollbar) {
        x = scrollbar->x() + scrollbar->width() - size.width();
        y = scrollbar->y();
    } else {
        x = scrollbar->x();
        y = scrollbar->y() + scrollbar->height() - size.height();
    }
    return IntRect(x, y, size.width(), size.height());
}

IntRect ScrollbarThemeChromium::trackRect(Scrollbar* scrollbar, bool)
{
    IntSize bs = buttonSize(scrollbar);
    // The buttons at the top and bottom of the scrollbar are square, so the
    // thickness of the scrollbar is also their height.
    int thickness = scrollbarThickness(scrollbar->controlSize());
    if (scrollbar->orientation() == HorizontalScrollbar) {
        // Once the scrollbar becomes smaller than the natural size of the
        // two buttons, the track disappears.
        if (scrollbar->width() < 2 * thickness)
            return IntRect();
        return IntRect(scrollbar->x() + bs.width(), scrollbar->y(), scrollbar->width() - 2 * bs.width(), thickness);
    }
    if (scrollbar->height() < 2 * thickness)
        return IntRect();
    return IntRect(scrollbar->x(), scrollbar->y() + bs.height(), thickness, scrollbar->height() - 2 * bs.height());
}

void ScrollbarThemeChromium::paintTrackBackground(GraphicsContext* context, Scrollbar* scrollbar, const IntRect& rect)
{
    // Just assume a forward track part.  We only paint the track as a single piece when there is no thumb.
    if (!hasThumb(scrollbar))
        paintTrackPiece(context, scrollbar, rect, ForwardTrackPart);
}

void ScrollbarThemeChromium::paintTickmarks(GraphicsContext* context, Scrollbar* scrollbar, const IntRect& rect)
{
    if (scrollbar->orientation() != VerticalScrollbar)
        return;

    if (rect.height() <= 0 || rect.width() <= 0)
        return;  // nothing to draw on.

    // Get the tickmarks for the frameview.
    Vector<IntRect> tickmarks;
    scrollbar->scrollableArea()->getTickmarks(tickmarks);
    if (!tickmarks.size())
        return;

    // Get the image for the tickmarks.
    static RefPtr<Image> dash = Image::loadPlatformResource("tickmarkDash");
    if (dash->isNull()) {
        ASSERT_NOT_REACHED();
        return;
    }

    context->save();

    for (Vector<IntRect>::const_iterator i = tickmarks.begin(); i != tickmarks.end(); ++i) {
        // Calculate how far down (in %) the tick-mark should appear.
        const float percent = static_cast<float>(i->y()) / scrollbar->totalSize();

        // Calculate how far down (in pixels) the tick-mark should appear.
        const int yPos = rect.y() + (rect.height() * percent);

        IntPoint tick(scrollbar->x(), yPos);
        context->drawImage(dash.get(), ColorSpaceDeviceRGB, tick);
    }

    context->restore();
}

} // namespace WebCore
