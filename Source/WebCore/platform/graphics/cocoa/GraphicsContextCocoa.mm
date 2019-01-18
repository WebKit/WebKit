/*
 * Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "GraphicsContext.h"

#import "DisplayListRecorder.h"
#import "GraphicsContextCG.h"
#import "GraphicsContextPlatformPrivateCG.h"
#import "IntRect.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/NSGraphicsSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/StdLibExtras.h>

#if USE(APPKIT)
#import <AppKit/AppKit.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "Color.h"
#import "WKGraphics.h"
#import <pal/spi/ios/UIKitSPI.h>
#endif

#if PLATFORM(MAC)
#import "LocalCurrentGraphicsContext.h"
#endif

@class NSColor;

// FIXME: More of this should use CoreGraphics instead of AppKit.
// FIXME: More of this should move into GraphicsContextCG.cpp.

namespace WebCore {

// NSColor, NSBezierPath, and NSGraphicsContext calls do not raise exceptions
// so we don't block exceptions.

#if ENABLE(FULL_KEYBOARD_ACCESS)

static bool drawFocusRingAtTime(CGContextRef context, NSTimeInterval timeOffset, const Color& color)
{
#if USE(APPKIT)
    CGFocusRingStyle focusRingStyle;
    BOOL needsRepaint = NSInitializeCGFocusRingStyleForTime(NSFocusRingOnly, &focusRingStyle, timeOffset);
#else
    BOOL needsRepaint = NO;
    UNUSED_PARAM(timeOffset);

    CGFocusRingStyle focusRingStyle;
    focusRingStyle.version = 0;
    focusRingStyle.tint = kCGFocusRingTintBlue;
    focusRingStyle.ordering = kCGFocusRingOrderingNone;
    focusRingStyle.alpha = kCGFocusRingAlphaDefault;
    focusRingStyle.radius = kCGFocusRingRadiusDefault;
    focusRingStyle.threshold = kCGFocusRingThresholdDefault;
    focusRingStyle.bounds = CGRectZero;
#endif

    // We want to respect the CGContext clipping and also not overpaint any
    // existing focus ring. The way to do this is set accumulate to
    // -1. According to CoreGraphics, the reasoning for this behavior has been
    // lost in time.
    focusRingStyle.accumulate = -1;
    auto style = adoptCF(CGStyleCreateFocusRingWithColor(&focusRingStyle, cachedCGColor(color)));

    CGContextStateSaver stateSaver(context);

    CGContextSetStyle(context, style.get());
    CGContextFillPath(context);

    return needsRepaint;
}

static void drawFocusRing(CGContextRef context, const Color& color)
{
    drawFocusRingAtTime(context, std::numeric_limits<double>::max(), color);
}

static void drawFocusRingToContext(CGContextRef context, CGPathRef focusRingPath, const Color& color)
{
    CGContextBeginPath(context);
    CGContextAddPath(context, focusRingPath);
    drawFocusRing(context, color);
}

static bool drawFocusRingToContextAtTime(CGContextRef context, CGPathRef focusRingPath, double timeOffset, const Color& color)
{
    UNUSED_PARAM(timeOffset);
    CGContextBeginPath(context);
    CGContextAddPath(context, focusRingPath);
    return drawFocusRingAtTime(context, std::numeric_limits<double>::max(), color);
}

#endif // ENABLE(FULL_KEYBOARD_ACCESS)

void GraphicsContext::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
#if ENABLE(FULL_KEYBOARD_ACCESS)
    if (paintingDisabled() || path.isNull())
        return;

    if (m_impl) {
        m_impl->drawFocusRing(path, width, offset, color);
        return;
    }

    drawFocusRingToContext(platformContext(), path.platformPath(), color);
#else
    UNUSED_PARAM(path);
    UNUSED_PARAM(width);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(color);
#endif
}

// FIXME: The following functions should only be compiled for Mac. See <https://bugs.webkit.org/show_bug.cgi?id=193591>.
#if ENABLE(FULL_KEYBOARD_ACCESS)

void GraphicsContext::drawFocusRing(const Path& path, double timeOffset, bool& needsRedraw, const Color& color)
{
    if (paintingDisabled() || path.isNull())
        return;

    if (m_impl) // FIXME: implement animated focus ring drawing.
        return;

    needsRedraw = drawFocusRingToContextAtTime(platformContext(), path.platformPath(), timeOffset, color);
}

void GraphicsContext::drawFocusRing(const Vector<FloatRect>& rects, double timeOffset, bool& needsRedraw, const Color& color)
{
    if (paintingDisabled())
        return;

    if (m_impl) // FIXME: implement animated focus ring drawing.
        return;

    RetainPtr<CGMutablePathRef> focusRingPath = adoptCF(CGPathCreateMutable());
    for (const auto& rect : rects)
        CGPathAddRect(focusRingPath.get(), 0, CGRect(rect));

    needsRedraw = drawFocusRingToContextAtTime(platformContext(), focusRingPath.get(), timeOffset, color);
}

#endif

void GraphicsContext::drawFocusRing(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
#if ENABLE(FULL_KEYBOARD_ACCESS)
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawFocusRing(rects, width, offset, color);
        return;
    }

    RetainPtr<CGMutablePathRef> focusRingPath = adoptCF(CGPathCreateMutable());
    for (auto& rect : rects)
        CGPathAddRect(focusRingPath.get(), 0, CGRectInset(rect, -offset, -offset));

    drawFocusRingToContext(platformContext(), focusRingPath.get(), color);
#else
    UNUSED_PARAM(rects);
    UNUSED_PARAM(width);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(color);
#endif
}

static inline void setPatternPhaseInUserSpace(CGContextRef context, CGPoint phasePoint)
{
    CGAffineTransform userToBase = getUserToBaseCTM(context);
    CGPoint phase = CGPointApplyAffineTransform(phasePoint, userToBase);

    CGContextSetPatternPhase(context, CGSizeMake(phase.x, phase.y));
}

static CGColorRef colorForMarkerLineStyle(DocumentMarkerLineStyle::Mode style, bool useDarkMode)
{
    switch (style) {
    // Red
    case DocumentMarkerLineStyle::Mode::Spelling:
        return cachedCGColor(useDarkMode ? Color { 255, 140, 140, 217 } : Color { 255, 59, 48, 191 });
    // Blue
    case DocumentMarkerLineStyle::Mode::DictationAlternatives:
    case DocumentMarkerLineStyle::Mode::TextCheckingDictationPhraseWithAlternatives:
    case DocumentMarkerLineStyle::Mode::AutocorrectionReplacement:
        return cachedCGColor(useDarkMode ? Color { 40, 145, 255, 217 } : Color { 0, 122, 255, 191 });
    // Green
    case DocumentMarkerLineStyle::Mode::Grammar:
        return cachedCGColor(useDarkMode ? Color { 50, 215, 75, 217 } : Color { 25, 175, 50, 191 });
    }
}

void GraphicsContext::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    if (paintingDisabled())
        return;

    // We want to find the number of full dots, so we're solving the equations:
    // dotDiameter = height
    // dotDiameter / dotGap = 13.247 / 9.457
    // numberOfGaps = numberOfDots - 1
    // dotDiameter * numberOfDots + dotGap * numberOfGaps = width

    auto width = rect.width();
    auto dotDiameter = rect.height();
    auto dotGap = dotDiameter * 9.457 / 13.247;
    auto numberOfDots = (width + dotGap) / (dotDiameter + dotGap);
    auto numberOfWholeDots = static_cast<unsigned>(numberOfDots);
    auto numberOfWholeGaps = numberOfWholeDots - 1;

    // Center the dots
    auto offset = (width - (dotDiameter * numberOfWholeDots + dotGap * numberOfWholeGaps)) / 2;

    auto circleColor = colorForMarkerLineStyle(style.mode, style.shouldUseDarkAppearance);

    CGContextRef platformContext = this->platformContext();
    CGContextStateSaver stateSaver { platformContext };
    CGContextSetFillColorWithColor(platformContext, circleColor);
    for (unsigned i = 0; i < numberOfWholeDots; ++i) {
        auto location = rect.location();
        location.move(offset + i * (dotDiameter + dotGap), 0);
        auto size = FloatSize(dotDiameter, dotDiameter);
        CGContextAddEllipseInRect(platformContext, FloatRect(location, size));
    }
    CGContextSetCompositeOperation(platformContext, kCGCompositeSover);
    CGContextFillPath(platformContext);
}

} // namespace WebCore
