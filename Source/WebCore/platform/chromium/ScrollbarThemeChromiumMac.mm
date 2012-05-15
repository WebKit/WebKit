/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
#include "ScrollbarThemeChromiumMac.h"

#include "FrameView.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "LocalCurrentGraphicsContext.h"
#include "NSScrollerImpDetails.h"
#include "PlatformContextSkia.h"
#include "PlatformSupport.h"
#include "ScrollAnimatorMac.h"
#include "ScrollView.h"
#include "skia/ext/skia_utils_mac.h"
#include <Carbon/Carbon.h>

namespace WebCore {

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    DEFINE_STATIC_LOCAL(ScrollbarThemeChromiumMac, theme, ());
    return &theme;
}

ScrollbarThemeChromiumMac::ScrollbarThemeChromiumMac()
{
    // Load the linen pattern image used for overhang drawing.
    RefPtr<Image> patternImage = Image::loadPlatformResource("overhangPattern");
    m_overhangPattern = Pattern::create(patternImage, true, true);
}

ScrollbarThemeChromiumMac::~ScrollbarThemeChromiumMac()
{
}

static PlatformSupport::ThemePaintState scrollbarStateToThemeState(ScrollbarThemeClient* scrollbar)
{
    if (!scrollbar->enabled())
        return PlatformSupport::StateDisabled;
    if (!scrollbar->isScrollableAreaActive())
        return PlatformSupport::StateInactive;
    if (scrollbar->pressedPart() == ThumbPart)
        return PlatformSupport::StatePressed;

    return PlatformSupport::StateActive;
}

static void scrollbarPainterPaintTrack(ScrollbarPainter scrollbarPainter, bool enabled, double value, CGFloat proportion, CGRect frameRect)
{
    [scrollbarPainter setEnabled:enabled];
    [scrollbarPainter setBoundsSize: NSSizeFromCGSize(frameRect.size)];
    [scrollbarPainter setDoubleValue:value];
    [scrollbarPainter setKnobProportion:proportion];

    // The scrollbar's frameRect includes a side inset for overlay scrollers, so we have to use the 
    // trackWidth for drawKnobSlotInRect
    NSRect trackRect;
    if ([scrollbarPainter isHorizontal])
        trackRect = NSMakeRect(0, 0, frameRect.size.width, [scrollbarPainter trackWidth]);
    else
        trackRect = NSMakeRect(0, 0, [scrollbarPainter trackWidth], frameRect.size.height);
    [scrollbarPainter drawKnobSlotInRect:trackRect highlight:NO];
}

// Override ScrollbarThemeMac::paint() to add support for the following:
//     - drawing using PlatformSupport functions
//     - drawing tickmarks
//     - Skia specific changes
bool ScrollbarThemeChromiumMac::paint(ScrollbarThemeClient* scrollbar, GraphicsContext* context, const IntRect& damageRect)
{
    // Get the tickmarks for the frameview.
    Vector<IntRect> tickmarks;
    scrollbar->getTickmarks(tickmarks);

    if (isScrollbarOverlayAPIAvailable()) {
        float value = 0;
        float overhang = 0;

        if (scrollbar->currentPos() < 0) {
            // Scrolled past the top.
            value = 0;
            overhang = -scrollbar->currentPos();
        } else if (scrollbar->visibleSize() + scrollbar->currentPos() > scrollbar->totalSize()) {
            // Scrolled past the bottom.
            value = 1;
            overhang = scrollbar->currentPos() + scrollbar->visibleSize() - scrollbar->totalSize();
        } else {
            // Within the bounds of the scrollable area.
            int maximum = scrollbar->maximum();
            if (maximum > 0)
                value = scrollbar->currentPos() / maximum;
            else
                value = 0;
        }

        setIsCurrentlyDrawingIntoLayer(false);

        CGFloat oldKnobAlpha = 0;
        CGFloat oldTrackAlpha = 0;
        BOOL oldIsExpanded = NO;
        bool hasTickmarks = tickmarks.size() > 0 && scrollbar->orientation() == VerticalScrollbar;
        ScrollbarPainter scrollbarPainter = painterForScrollbar(scrollbar);
        if (hasTickmarks) {
            oldKnobAlpha = [scrollbarPainter knobAlpha];
            [scrollbarPainter setKnobAlpha:1.0];
            oldTrackAlpha = [scrollbarPainter trackAlpha];
            [scrollbarPainter setTrackAlpha:1.0];
            if ([scrollbarPainter respondsToSelector:@selector(setExpanded:)]) {
              oldIsExpanded = [scrollbarPainter isExpanded];
              [scrollbarPainter setExpanded:YES];
            }
        }

        GraphicsContextStateSaver stateSaver(*context);
        context->clip(damageRect);
        context->translate(scrollbar->frameRect().x(), scrollbar->frameRect().y());
        LocalCurrentGraphicsContext localContext(context);
        scrollbarPainterPaintTrack(scrollbarPainter,
                                   scrollbar->enabled(),
                                   value,
                                   (static_cast<CGFloat>(scrollbar->visibleSize()) - overhang) / scrollbar->totalSize(),
                                   scrollbar->frameRect());

        IntRect tickmarkTrackRect(IntPoint(), trackRect(scrollbar, false).size());
        if (tickmarkTrackRect.width() <= 10) {
            // For narrow scrollbars inset by 1 on the left and 3 on the right.
            tickmarkTrackRect.setX(tickmarkTrackRect.x() + 1);
            tickmarkTrackRect.setWidth(tickmarkTrackRect.width() - 4);
        } else {
            // For wide scrollbars inset by 2 on the left and 3 on the right.
            tickmarkTrackRect.setX(tickmarkTrackRect.x() + 2);
            tickmarkTrackRect.setWidth(tickmarkTrackRect.width() - 5);
        }
        paintGivenTickmarks(context, scrollbar, tickmarkTrackRect, tickmarks);

        if (scrollbar->enabled())
            [scrollbarPainter drawKnob];

        setIsCurrentlyDrawingIntoLayer(false);

        if (hasTickmarks) {
            [scrollbarPainter setKnobAlpha:oldKnobAlpha];
            [scrollbarPainter setTrackAlpha:oldTrackAlpha];
            if ([scrollbarPainter respondsToSelector:@selector(setExpanded:)])
              [scrollbarPainter setExpanded:oldIsExpanded];
        }

        return true;
    }

    HIThemeTrackDrawInfo trackInfo;
    trackInfo.version = 0;
    trackInfo.kind = scrollbar->controlSize() == RegularScrollbar ? kThemeMediumScrollBar : kThemeSmallScrollBar;
    trackInfo.bounds = scrollbar->frameRect();
    trackInfo.min = 0;
    trackInfo.max = scrollbar->maximum();
    trackInfo.value = scrollbar->currentPos();
    trackInfo.trackInfo.scrollbar.viewsize = scrollbar->visibleSize();
    trackInfo.attributes = 0;
    if (scrollbar->orientation() == HorizontalScrollbar)
        trackInfo.attributes |= kThemeTrackHorizontal;

    if (!scrollbar->enabled())
        trackInfo.enableState = kThemeTrackDisabled;
    else
        trackInfo.enableState = scrollbar->isScrollableAreaActive() ? kThemeTrackActive : kThemeTrackInactive;

    if (!hasButtons(scrollbar))
        trackInfo.enableState = kThemeTrackNothingToScroll;
    trackInfo.trackInfo.scrollbar.pressState = scrollbarPartToHIPressedState(scrollbar->pressedPart());

    SkCanvas* canvas = context->platformContext()->canvas();
    CGAffineTransform currentCTM = gfx::SkMatrixToCGAffineTransform(canvas->getTotalMatrix());

    // The Aqua scrollbar is buggy when rotated and scaled.  We will just draw into a bitmap if we detect a scale or rotation.
    bool canDrawDirectly = currentCTM.a == 1.0f && currentCTM.b == 0.0f && currentCTM.c == 0.0f && (currentCTM.d == 1.0f || currentCTM.d == -1.0f);
    GraphicsContext* drawingContext = context;
    OwnPtr<ImageBuffer> imageBuffer;
    if (!canDrawDirectly) {
        trackInfo.bounds = IntRect(IntPoint(), scrollbar->frameRect().size());

        IntRect bufferRect(scrollbar->frameRect());
        bufferRect.intersect(damageRect);
        bufferRect.move(-scrollbar->frameRect().x(), -scrollbar->frameRect().y());

        imageBuffer = ImageBuffer::create(bufferRect.size());
        if (!imageBuffer)
            return true;

        drawingContext = imageBuffer->context();
    }

    // Draw thumbless.
    gfx::SkiaBitLocker bitLocker(drawingContext->platformContext()->canvas());
    CGContextRef cgContext = bitLocker.cgContext();
    HIThemeDrawTrack(&trackInfo, 0, cgContext, kHIThemeOrientationNormal);

    IntRect tickmarkTrackRect = trackRect(scrollbar, false);
    if (!canDrawDirectly) {
        tickmarkTrackRect.setX(0);
        tickmarkTrackRect.setY(0);
    }
    // The ends are rounded and the thumb doesn't go there.
    tickmarkTrackRect.inflateY(-tickmarkTrackRect.width());
    // Inset by 2 on the left and 3 on the right.
    tickmarkTrackRect.setX(tickmarkTrackRect.x() + 2);
    tickmarkTrackRect.setWidth(tickmarkTrackRect.width() - 5);
    paintGivenTickmarks(drawingContext, scrollbar, tickmarkTrackRect, tickmarks);

    if (hasThumb(scrollbar)) {
        PlatformSupport::ThemePaintScrollbarInfo scrollbarInfo;
        scrollbarInfo.orientation = scrollbar->orientation() == HorizontalScrollbar ? PlatformSupport::ScrollbarOrientationHorizontal : PlatformSupport::ScrollbarOrientationVertical;
        scrollbarInfo.parent = scrollbar->isScrollViewScrollbar() ? PlatformSupport::ScrollbarParentScrollView : PlatformSupport::ScrollbarParentRenderLayer;
        scrollbarInfo.maxValue = scrollbar->maximum();
        scrollbarInfo.currentValue = scrollbar->currentPos();
        scrollbarInfo.visibleSize = scrollbar->visibleSize();
        scrollbarInfo.totalSize = scrollbar->totalSize();

        PlatformSupport::paintScrollbarThumb(
            drawingContext,
            scrollbarStateToThemeState(scrollbar),
            scrollbar->controlSize() == RegularScrollbar ? PlatformSupport::SizeRegular : PlatformSupport::SizeSmall,
            scrollbar->frameRect(),
            scrollbarInfo);
    }

    if (!canDrawDirectly)
        context->drawImageBuffer(imageBuffer.get(), ColorSpaceDeviceRGB, scrollbar->frameRect().location());

    return true;
}

void ScrollbarThemeChromiumMac::paintGivenTickmarks(GraphicsContext* context, ScrollbarThemeClient* scrollbar, const IntRect& rect, const Vector<IntRect>& tickmarks)
{
    if (scrollbar->orientation() != VerticalScrollbar)
        return;

    if (rect.height() <= 0 || rect.width() <= 0)
        return;  // nothing to draw on.

    if (!tickmarks.size())
        return;

    GraphicsContextStateSaver stateSaver(*context);
    context->setShouldAntialias(false);
    context->setStrokeColor(Color(0xCC, 0xAA, 0x00, 0xFF), ColorSpaceDeviceRGB);
    context->setFillColor(Color(0xFF, 0xDD, 0x00, 0xFF), ColorSpaceDeviceRGB);

    for (Vector<IntRect>::const_iterator i = tickmarks.begin(); i != tickmarks.end(); ++i) {
        // Calculate how far down (in %) the tick-mark should appear.
        const float percent = static_cast<float>(i->y()) / scrollbar->totalSize();
        if (percent < 0.0 || percent > 1.0)
            continue;

        // Calculate how far down (in pixels) the tick-mark should appear.
        const int yPos = rect.y() + (rect.height() * percent);

        // Paint.
        FloatRect tickRect(rect.x(), yPos, rect.width(), 2);
        context->fillRect(tickRect);
        context->strokeRect(tickRect, 1);
    }
}

void ScrollbarThemeChromiumMac::paintOverhangAreas(ScrollView* view, GraphicsContext* context, const IntRect& horizontalOverhangRect, const IntRect& verticalOverhangRect, const IntRect& dirtyRect)
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
    const unsigned kNumShadowColors = sizeof(kShadowColors)/sizeof(kShadowColors[0]);

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
            } else {
                shadowCornerOrigin.setX(shadowRect.maxX());
            }
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
        } else {
            gradient = Gradient::create(FloatPoint(shadowRect.x(), 0), FloatPoint(shadowRect.maxX(), 0));
        }
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


}
