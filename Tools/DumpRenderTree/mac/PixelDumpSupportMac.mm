/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2007 Eric Seidel <eric@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PixelDumpSupport.h"
#import "PixelDumpSupportCG.h"

#import "DumpRenderTree.h" 
#import "TestRunner.h"
#import <CoreGraphics/CGBitmapContext.h>
#import <QuartzCore/QuartzCore.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/Assertions.h>
#import <wtf/RefPtr.h>

#import <WebKit/WebCoreStatistics.h>
#import <WebKit/WebDocumentPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/WebViewPrivate.h>

@interface CATransaction ()
+ (void)synchronize;
@end

@interface WebView ()
- (BOOL)_flushCompositingChanges;
@end

static void paintRepaintRectOverlay(WebView* webView, CGContextRef context)
{
    CGRect viewRect = NSRectToCGRect([webView bounds]);

    CGContextSaveGState(context);

    // Using a transparency layer is easier than futzing with clipping.
    CGContextBeginTransparencyLayer(context, 0);

    // Flip the context.
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, 0, -viewRect.size.height);
    
    CGContextSetRGBFillColor(context, 0, 0, 0, static_cast<CGFloat>(0.66));
    CGContextFillRect(context, viewRect);

    NSArray *repaintRects = [webView trackedRepaintRects];
    if (repaintRects) {
        
        for (NSValue *value in repaintRects) {
            CGRect currRect = NSRectToCGRect([value rectValue]);
            CGContextClearRect(context, currRect);
        }
    }
    
    CGContextEndTransparencyLayer(context);
    CGContextRestoreGState(context);
}

static CGImageRef takeWindowSnapshot(CGSWindowID windowID, CGWindowImageOption imageOptions)
{
    imageOptions |= kCGWindowImageBoundsIgnoreFraming | kCGWindowImageShouldBeOpaque;
    return CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, windowID, imageOptions);
}

RefPtr<BitmapContext> createBitmapContextFromWebView(bool onscreen, bool incrementalRepaint, bool sweepHorizontally, bool drawSelectionRect)
{
    WebView* view = [mainFrame webView];

    // If the WebHTMLView uses accelerated compositing, we need for force the on-screen capture path
    // and also force Core Animation to start its animations with -display since the DRT window has autodisplay disabled.
    if ([view _isUsingAcceleratedCompositing])
        onscreen = YES;

    float deviceScaleFactor = [view _backingScaleFactor];
    NSSize webViewSize = [view frame].size;
    size_t pixelsWide = static_cast<size_t>(webViewSize.width * deviceScaleFactor);
    size_t pixelsHigh = static_cast<size_t>(webViewSize.height * deviceScaleFactor);
    size_t rowBytes = 0;
    void* buffer = nullptr;
    auto bitmapContext = createBitmapContext(pixelsWide, pixelsHigh, rowBytes, buffer);
    if (!bitmapContext)
        return nullptr;
    CGContextRef context = bitmapContext->cgContext();
    // The final scaling gets doubled on the screen capture surface when we use the hidpi backingScaleFactor value for CTM.
    // This is a workaround to push the scaling back.
    float scaleForCTM = onscreen ? 1 : [view _backingScaleFactor];
    CGContextScaleCTM(context, scaleForCTM, scaleForCTM);

    NSGraphicsContext *nsContext = [NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO];
    ASSERT(nsContext);
    
    if (incrementalRepaint) {
        if (sweepHorizontally) {
            for (NSRect column = NSMakeRect(0, 0, 1, webViewSize.height); column.origin.x < webViewSize.width; column.origin.x++)
                [view displayRectIgnoringOpacity:column inContext:nsContext];
        } else {
            for (NSRect line = NSMakeRect(0, 0, webViewSize.width, 1); line.origin.y < webViewSize.height; line.origin.y++)
                [view displayRectIgnoringOpacity:line inContext:nsContext];
        }
    } else {
      if (onscreen) {
            // FIXME: This will break repaint testing if we have compositing in repaint tests.
            // (displayWebView() painted gray over the webview, but we'll be making everything repaint again).
            [view display];
            [view _flushCompositingChanges];
            [CATransaction flush];
            [CATransaction synchronize];

            // Ask the window server to provide us a composited version of the *real* window content including surfaces (i.e. OpenGL content)
            // Note that the returned image might differ very slightly from the window backing because of dithering artifacts in the window server compositor.
            NSWindow *window = [view window];
            CGImageRef image = takeWindowSnapshot([window windowNumber], kCGWindowImageDefault);

            if (image) {
                // Work around <rdar://problem/17084993>; re-request the snapshot at kCGWindowImageNominalResolution if it was captured at the wrong scale.
                CGFloat desiredSnapshotWidth = window.frame.size.width * deviceScaleFactor;
                if (CGImageGetWidth(image) != desiredSnapshotWidth)
                    image = takeWindowSnapshot([window windowNumber], kCGWindowImageNominalResolution);
            }

            if (!image)
                return nullptr;

            CGContextDrawImage(context, CGRectMake(0, 0, CGImageGetWidth(image), CGImageGetHeight(image)), image);
            CGImageRelease(image);

            if ([view isTrackingRepaints])
                paintRepaintRectOverlay(view, context);
        } else {
            // Call displayRectIgnoringOpacity for HiDPI tests since it ensures we paint directly into the context
            // that we have appropriately sized and scaled.
            [view displayRectIgnoringOpacity:[view bounds] inContext:nsContext];
            if ([view isTrackingRepaints])
                paintRepaintRectOverlay(view, context);
        }
    }

    if (drawSelectionRect) {
        NSView *documentView = [[mainFrame frameView] documentView];
        ASSERT([documentView conformsToProtocol:@protocol(WebDocumentSelection)]);
        NSRect rect = [documentView convertRect:[(id <WebDocumentSelection>)documentView selectionRect] fromView:nil];
        CGContextSaveGState(context);
        CGContextSetLineWidth(context, 1.0);
        CGContextSetRGBStrokeColor(context, 1.0, 0.0, 0.0, 1.0);
        CGContextStrokeRect(context, CGRectMake(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height));
        CGContextRestoreGState(context);
    }
    
    return bitmapContext;
}

RefPtr<BitmapContext> createPagedBitmapContext()
{
    int pageWidthInPixels = TestRunner::viewWidth;
    int pageHeightInPixels = TestRunner::viewHeight;
    int numberOfPages = [mainFrame numberOfPagesWithPageWidth:pageWidthInPixels pageHeight:pageHeightInPixels];
    size_t rowBytes = 0;
    void* buffer = nullptr;

    int totalHeight = numberOfPages * (pageHeightInPixels + 1) - 1;

    auto bitmapContext = createBitmapContext(pageWidthInPixels, totalHeight, rowBytes, buffer);
    CGContextRef context = bitmapContext->cgContext();
    CGContextTranslateCTM(context, 0, totalHeight);
    CGContextScaleCTM(context, 1, -1);
    [mainFrame printToCGContext:context pageWidth:pageWidthInPixels pageHeight:pageHeightInPixels];
    return bitmapContext;
}
