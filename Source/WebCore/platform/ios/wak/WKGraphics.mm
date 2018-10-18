/*
 * Copyright (C) 2005-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKGraphics.h"

#if PLATFORM(IOS_FAMILY)

#import "WebCoreThreadInternal.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>

static inline void _FillRectsUsingOperation(CGContextRef context, const CGRect* rects, int count, CGCompositeOperation op)
{
    int i;
    CGRect *integralRects = reinterpret_cast<CGRect *>(alloca(sizeof(CGRect) * count));
    
    assert (integralRects);
    
    for (i = 0; i < count; i++) {
        integralRects[i] = CGRectApplyAffineTransform (rects[i], CGContextGetCTM(context));
        integralRects[i] = CGRectIntegral (integralRects[i]);
        integralRects[i] = CGRectApplyAffineTransform (integralRects[i], CGAffineTransformInvert(CGContextGetCTM(context)));
    }
    CGCompositeOperation oldOp = CGContextGetCompositeOperation(context);
    CGContextSetCompositeOperation(context, op);
    CGContextFillRects(context, integralRects, count);
    CGContextSetCompositeOperation(context, oldOp);
}

void WKRectFill(CGContextRef context, CGRect aRect)
{
    if (aRect.size.width > 0 && aRect.size.height > 0) {
        CGContextSaveGState(context);
        if (aRect.size.width > 0 && aRect.size.height > 0)
            _FillRectsUsingOperation(context, &aRect, 1, kCGCompositeCopy);
        CGContextRestoreGState(context);
    }
}

void WKSetCurrentGraphicsContext(CGContextRef context)
{
    WebThreadContext* threadContext =  WebThreadCurrentContext();
    threadContext->currentCGContext = context;
}

CGContextRef WKGetCurrentGraphicsContext(void)
{
    WebThreadContext* threadContext =  WebThreadCurrentContext();
    return threadContext->currentCGContext;
}

#endif // PLATFORM(IOS_FAMILY)
