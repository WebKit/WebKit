/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#import "WebCoreSystemInterface.h"

// FIXME: More of this should use CoreGraphics instead of AppKit.
// FIXME: More of this should move into GraphicsContextCG.cpp.

namespace WebCore {

// NSColor, NSBezierPath, and NSGraphicsContext
// calls in this file are all exception-safe, so we don't block
// exceptions for those.

class GraphicsContextPlatformPrivate {
public:
    GraphicsContextPlatformPrivate(CGContextRef);
    ~GraphicsContextPlatformPrivate();

    CGContextRef m_cgContext;
    IntRect m_focusRingClip; // Work around CG bug in focus ring clipping.
};

GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate(CGContextRef cgContext)
{
    m_cgContext = cgContext;
    CGContextRetain(m_cgContext);
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
    CGContextRelease(m_cgContext);
}

GraphicsContext::GraphicsContext(CGContextRef cgContext)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate(cgContext))
{
    setPaintingDisabled(!cgContext);
}

GraphicsContext::~GraphicsContext()
{
    destroyGraphicsContextPrivate(m_common);
    delete m_data;
}

void GraphicsContext::setFocusRingClip(const IntRect& r)
{
    // This method only exists to work around bugs in Mac focus ring clipping.
    m_data->m_focusRingClip = r;
}

void GraphicsContext::clearFocusRingClip()
{
    // This method only exists to work around bugs in Mac focus ring clipping.
    m_data->m_focusRingClip = IntRect();
}

void GraphicsContext::drawFocusRing(const Color& color)
{
    if (paintingDisabled())
        return;

    int radius = (focusRingWidth() - 1) / 2;
    int offset = radius + focusRingOffset();
    CGColorRef colorRef = color.isValid() ? cgColor(color) : 0;

    CGMutablePathRef focusRingPath = CGPathCreateMutable();
    const Vector<IntRect>& rects = focusRingRects();
    unsigned rectCount = rects.size();
    for (unsigned i = 0; i < rectCount; i++)
        CGPathAddRect(focusRingPath, 0, CGRectInset(rects[i], -offset, -offset));

    CGContextRef context = platformContext();

    // FIXME: This works only inside a NSView's drawRect method. The view must be
    // focused and this context must be the current NSGraphicsContext.
    ASSERT(context == [[NSGraphicsContext currentContext] graphicsPort]);
    NSView* view = [NSView focusView];
    ASSERT(view);

    const NSRect* drawRects;
#if defined(__LP64__)
    long count;
#else
    int count;
#endif
    [view getRectsBeingDrawn:&drawRects count:&count];

    // We have to pass in our own clip rectangles here because a bug in CG
    // seems to inflate the clip (thus allowing the focus ring to paint
    // slightly outside the clip).
    NSRect transformedClipRect = [view convertRect:m_data->m_focusRingClip toView:nil];
    for (int i = 0; i < count; ++i) {
        NSRect transformedRect = [view convertRect:drawRects[i] toView:nil];
        NSRect rectToUse = NSIntersectionRect(transformedRect, transformedClipRect);
        if (!NSIsEmptyRect(rectToUse)) {
            CGContextBeginPath(context);
            CGContextAddPath(context, focusRingPath);
            wkDrawFocusRing(context, *(CGRect *)&rectToUse, colorRef, radius);
        }
    }

    CGColorRelease(colorRef);

    CGPathRelease(focusRingPath);
}

CGContextRef GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    ASSERT(m_data->m_cgContext);
    return m_data->m_cgContext;
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    if (paintingDisabled())
        return;
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    [[NSGraphicsContext graphicsContextWithGraphicsPort:platformContext() flipped:YES]
        setCompositingOperation:(NSCompositingOperation)op];
    [pool release];
}

void GraphicsContext::drawLineForMisspelling(const IntPoint& point, int width)
{
    // Constants for pattern color
    static NSColor *spellingPatternColor = nil;
    static bool usingDot = false;
    int patternHeight = cMisspellingLineThickness;
    int patternWidth = cMisspellingLinePatternWidth;
 
    // Initialize pattern color if needed
    if (!spellingPatternColor) {
        NSImage *image = [NSImage imageNamed:@"SpellingDot"];
        assert(image); // if image is not available, we want to know
        NSColor *color = (image ? [NSColor colorWithPatternImage:image] : nil);
        if (color)
            usingDot = true;
        else
            color = [NSColor redColor];
        spellingPatternColor = [color retain];
    }

    // Make sure to draw only complete dots.
    // NOTE: Code here used to shift the underline to the left and increase the width
    // to make sure everything gets underlined, but that results in drawing out of
    // bounds (e.g. when at the edge of a view) and could make it appear that the
    // space between adjacent misspelled words was underlined.
    if (usingDot) {
        // allow slightly more considering that the pattern ends with a transparent pixel
        int widthMod = width % patternWidth;
        if (patternWidth - widthMod > cMisspellingLinePatternGapWidth)
            width -= widthMod;
    }
    
    // Draw underline
    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    CGContextRef context = (CGContextRef)[currentContext graphicsPort];
    CGContextSaveGState(context);

    [spellingPatternColor set];

    CGPoint transformedOrigin = CGPointApplyAffineTransform(point, CGContextGetCTM(context));
    CGContextSetPatternPhase(context, CGSizeMake(transformedOrigin.x, transformedOrigin.y));

    NSRectFillUsingOperation(NSMakeRect(point.x(), point.y(), width, patternHeight), NSCompositeSourceOver);
    
    CGContextRestoreGState(context);
}

}
