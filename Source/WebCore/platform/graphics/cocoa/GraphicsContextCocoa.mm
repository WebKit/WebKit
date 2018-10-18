/*
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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
#import <wtf/StdLibExtras.h>

#if USE(APPKIT)
#import <AppKit/AppKit.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "Color.h"
#import "WKGraphics.h"
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

#if PLATFORM(MAC)

static bool drawFocusRingAtTime(CGContextRef context, NSTimeInterval timeOffset, const Color& color)
{
    CGFocusRingStyle focusRingStyle;
    BOOL needsRepaint = NSInitializeCGFocusRingStyleForTime(NSFocusRingOnly, &focusRingStyle, timeOffset);

    // We want to respect the CGContext clipping and also not overpaint any
    // existing focus ring. The way to do this is set accumulate to
    // -1. According to CoreGraphics, the reasoning for this behavior has been
    // lost in time.
    focusRingStyle.accumulate = -1;
    auto style = adoptCF(CGStyleCreateFocusRingWithColor(&focusRingStyle, cachedCGColor(color)));

    CGContextSaveGState(context);
    CGContextSetStyle(context, style.get());
    CGContextFillPath(context);
    CGContextRestoreGState(context);

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

#endif // PLATFORM(MAC)

void GraphicsContext::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
#if PLATFORM(MAC)
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

#if PLATFORM(MAC)
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
#if PLATFORM(MAC)
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

// FIXME: We need to keep this function since it is referenced by DrawLineForDocumentMarker::apply().
void GraphicsContext::drawLineForDocumentMarker(const FloatPoint&, float, DocumentMarkerLineStyle)
{
    // Cocoa platforms use RenderTheme::drawLineForDocumentMarker() to paint the platform document markers.
}

} // namespace WebCore
