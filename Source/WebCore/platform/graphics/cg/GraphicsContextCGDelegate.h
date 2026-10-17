/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if USE(CG)

#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class GraphicsContext;
class GStateScope;

class GraphicsContextCGDelegate {
    WTF_MAKE_TZONE_ALLOCATED(GraphicsContextCGDelegate);
    WTF_MAKE_NONCOPYABLE(GraphicsContextCGDelegate);
public:
    // `target` must outlive the returned CGContext.
    WEBCORE_EXPORT static RetainPtr<CGContextRef> createCGContext(GraphicsContext& target);

    GraphicsContext& target() const { return m_target; }

private:
    friend class GStateScope;

    explicit GraphicsContextCGDelegate(GraphicsContext&);

    static GraphicsContextCGDelegate& fromDelegate(CGContextDelegateRef);
    static void installCallbacks(CGContextDelegateRef);

    static void finalize(CGContextDelegateRef);

    static CGColorTransformRef getColorTransform(CGContextDelegateRef, CGRenderingStateRef, CGGStateRef);
    static CGAffineTransform getTransform(CGContextDelegateRef, CGRenderingStateRef, CGGStateRef);
    static CGRect getBounds(CGContextDelegateRef, CGRenderingStateRef, CGGStateRef);
    static CGColorSpaceRef getColorSpace(CGContextDelegateRef, CGRenderingStateRef, CGGStateRef);
    static const char* getName(CGContextDelegateRef);

    static void drawLines(CGContextDelegateRef, CGRenderingStateRef, CGGStateRef, const CGPoint[], size_t);
    static CGError drawRects(CGContextDelegateRef, CGRenderingStateRef, CGGStateRef, CGPathDrawingMode, const CGRect[], size_t);
    static CGError drawPath(CGContextDelegateRef, CGRenderingStateRef, CGGStateRef, CGPathDrawingMode, CGPathRef);
    static CGError drawPathDirect(CGContextDelegateRef, CGRenderingStateRef, CGGStateRef, CGPathDrawingMode, CGPathRef, const CGRect*);
    static CGError strokeArc(CGContextDelegateRef, CGRenderingStateRef, CGGStateRef, CGFloat x, CGFloat y, CGFloat radius, CGFloat startAngle, CGFloat endAngle, bool clockwise);

    static CGError operation(CGContextDelegateRef, CGRenderingStateRef, CGGStateRef, CFStringRef, CFDictionaryRef);

    void logUnimplementedCallback(const char* callbackName);

    CGRect deviceBounds() const;

    GraphicsContext& m_target;
    RetainPtr<CGColorTransformRef> m_colorTransform;
};

} // namespace WebCore

#endif // USE(CG)
