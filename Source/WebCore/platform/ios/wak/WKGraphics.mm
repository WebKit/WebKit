/*
 * Copyright (C) 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "WebCoreSystemInterface.h"
#import "Font.h"
#import "WebCoreThread.h"
#import <ImageIO/ImageIO.h>
#import <wtf/StdLibExtras.h>

using namespace WebCore;

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

static inline void _FillRectUsingOperation(CGContextRef context, CGRect rect, CGCompositeOperation op)
{
    if (rect.size.width > 0 && rect.size.height > 0) {
        _FillRectsUsingOperation (context, &rect, 1, op);
    }
}

void WKRectFill(CGContextRef context, CGRect aRect)
{
    if (aRect.size.width > 0 && aRect.size.height > 0) {
        CGContextSaveGState(context);
        _FillRectUsingOperation(context, aRect, kCGCompositeCopy);
        CGContextRestoreGState(context);
    }
}

void WKRectFillUsingOperation(CGContextRef context, CGRect aRect, CGCompositeOperation op)
{
    if (aRect.size.width > 0 && aRect.size.height > 0.0) {
        CGContextSaveGState(context);
        _FillRectUsingOperation(context, aRect, op);
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

static NSString *imageResourcePath(const char* imageFile, bool is2x)
{
    NSString *fileName = is2x ? [NSString stringWithFormat:@"%s@2x", imageFile] : [NSString stringWithUTF8String:imageFile];
#if PLATFORM(IOS_SIMULATOR)
    NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebCore"];
    return [bundle pathForResource:fileName ofType:@"png"];
#else
    // Workaround for <rdar://problem/7780665> CFBundleCopyResourceURL takes a long time on iPhone 3G.
    NSString *imageDirectory = @"/System/Library/PrivateFrameworks/WebCore.framework";
    return [NSString stringWithFormat:@"%@/%@.png", imageDirectory, fileName];
#endif
}

CGImageRef WKGraphicsCreateImageFromBundleWithName (const char *image_file)
{
    if (!image_file)
        return NULL;

    CGImageRef image = nullptr;
    NSData *imageData = nil;

    if (wkGetScreenScaleFactor() == 2) {
        NSString* full2xPath = imageResourcePath(image_file, true);
        imageData = [NSData dataWithContentsOfFile:full2xPath];
    }
    if (!imageData) {
        // We got here either because we didn't request hi-dpi or the @2x file doesn't exist.
        NSString* full1xPath = imageResourcePath(image_file, false);
        imageData = [NSData dataWithContentsOfFile:full1xPath];
    }
    
    if (imageData) {
        RetainPtr<CGDataProviderRef> dataProvider = adoptCF(CGDataProviderCreateWithCFData(reinterpret_cast<CFDataRef>(imageData)));
        image = CGImageCreateWithPNGDataProvider(dataProvider.get(), nullptr, NO, kCGRenderingIntentDefault);
    }

    return image;
}

static void WKDrawPatternBitmap(void *info, CGContextRef c) 
{
    CGImageRef image = (CGImageRef)info;
    CGFloat scale = wkGetScreenScaleFactor();
    CGContextDrawImage(c, CGRectMake(0, 0, CGImageGetWidth(image) / scale, CGImageGetHeight(image) / scale), image);    
}

static void WKReleasePatternBitmap(void *info) 
{
    CGImageRelease(reinterpret_cast<CGImageRef>(info));
}

static const CGPatternCallbacks WKPatternBitmapCallbacks = 
{
    0, WKDrawPatternBitmap, WKReleasePatternBitmap
};

CGPatternRef WKCreatePatternFromCGImage(CGImageRef imageRef)
{
    // retain image since it's freed by our callback
    CGImageRetain(imageRef);

    CGFloat scale = wkGetScreenScaleFactor();
    return CGPatternCreate((void*)imageRef, CGRectMake(0, 0, CGImageGetWidth(imageRef) / scale, CGImageGetHeight(imageRef) / scale), CGAffineTransformIdentity, CGImageGetWidth(imageRef) / scale, CGImageGetHeight(imageRef) / scale, kCGPatternTilingConstantSpacing, 1 /*isColored*/, &WKPatternBitmapCallbacks);
}

void WKSetPattern(CGContextRef context, CGPatternRef pattern, bool fill, bool stroke) 
{
    if (pattern == NULL)
        return;

    CGFloat patternAlpha = 1;
    CGColorSpaceRef colorspace = CGColorSpaceCreatePattern(NULL);
    if (fill) {
        CGContextSetFillColorSpace(context, colorspace);
        CGContextSetFillPattern(context, pattern, &patternAlpha);
    }
    if (stroke) {
        CGContextSetStrokeColorSpace(context, colorspace);
        CGContextSetStrokePattern(context, pattern, &patternAlpha);
    }
    CGColorSpaceRelease(colorspace);
}

void WKFontAntialiasingStateSaver::setup(bool isLandscapeOrientation)
{
#if !PLATFORM(IOS_SIMULATOR)
    m_oldAntialiasingStyle = CGContextGetFontAntialiasingStyle(m_context);

    if (m_useOrientationDependentFontAntialiasing)
        CGContextSetFontAntialiasingStyle(m_context, isLandscapeOrientation ? kCGFontAntialiasingStyleFilterLight : kCGFontAntialiasingStyleUnfiltered);
#else
    UNUSED_PARAM(isLandscapeOrientation);
#endif
}

void WKFontAntialiasingStateSaver::restore()
{
#if !PLATFORM(IOS_SIMULATOR)
    if (m_useOrientationDependentFontAntialiasing)
        CGContextSetFontAntialiasingStyle(m_context, m_oldAntialiasingStyle);
#endif
}

#endif // PLATFORM(IOS)
