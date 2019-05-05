/*
 * Copyright (C) 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#import "DumpRenderTree.h"
#import "DumpRenderTreeWindow.h"
#import "PixelDumpSupportCG.h"
#import "UIKitSPI.h"

#define COMMON_DIGEST_FOR_OPENSSL
#import <CommonCrypto/CommonDigest.h>
#import <MobileCoreServices/UTCoreTypes.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/IOSurface.h>
#import <WebCore/PlatformScreen.h>
#import <WebKit/WebCoreThread.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/RefCounted.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>

extern DumpRenderTreeWindow *gDrtWindow;
extern DumpRenderTreeBrowserView *gWebBrowserView;

RefPtr<BitmapContext> createBitmapContextFromWebView(bool onscreen, bool incrementalRepaint, bool sweepHorizontally, bool drawSelectionRect)
{
    // TODO: <rdar://problem/6558366> DumpRenderTree: Investigate testRepaintSweepHorizontally and dumpSelectionRect
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebThreadLock();
    [CATransaction flush];

    CGFloat deviceScaleFactor = 2; // FIXME: hardcode 2x for now. In future we could respect 1x and 3x as we do on Mac.
    
    CGSize viewSize = [[mainFrame webView] frame].size;
    int bufferWidth = ceil(viewSize.width * deviceScaleFactor);
    int bufferHeight = ceil(viewSize.height * deviceScaleFactor);

#if HAVE(IOSURFACE)
    WebCore::FloatSize snapshotSize(viewSize);
    snapshotSize.scale(deviceScaleFactor);

    WebCore::IOSurface::Format snapshotFormat = WebCore::screenSupportsExtendedColor() ? WebCore::IOSurface::Format::RGB10 : WebCore::IOSurface::Format::RGBA;
    auto surface = WebCore::IOSurface::create(WebCore::expandedIntSize(snapshotSize), WebCore::sRGBColorSpaceRef(), snapshotFormat);
    RetainPtr<CGImageRef> cgImage = surface->createImage();

    void* bitmapBuffer = nullptr;
    size_t bitmapRowBytes = 0;
    auto bitmapContext = createBitmapContext(bufferWidth, bufferHeight, bitmapRowBytes, bitmapBuffer);
    if (!bitmapContext)
        return nullptr;

    CGContextDrawImage(bitmapContext->cgContext(), CGRectMake(0, 0, bufferWidth, bufferHeight), cgImage.get());
    return bitmapContext;
#else
    CATransform3D transform = CATransform3DMakeScale(deviceScaleFactor, deviceScaleFactor, 1);
    static CGColorSpaceRef sRGBSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);

    CARenderServerBufferRef buffer = CARenderServerCreateBuffer(bufferWidth, bufferHeight);
    if (!buffer) {
        WTFLogAlways("CARenderServerCreateBuffer failed for buffer with width %d height %d\n", bufferWidth, bufferHeight);
        return nullptr;
    }

    CARenderServerRenderLayerWithTransform(MACH_PORT_NULL, [gWebBrowserView layer].context.contextId, reinterpret_cast<uint64_t>([gWebBrowserView layer]), buffer, 0, 0, &transform);

    uint8_t* data = CARenderServerGetBufferData(buffer);
    size_t rowBytes = CARenderServerGetBufferRowBytes(buffer);

    RetainPtr<CGDataProviderRef> provider = adoptCF(CGDataProviderCreateWithData(0, data, CARenderServerGetBufferDataSize(buffer), nullptr));
    
    RetainPtr<CGImageRef> cgImage = adoptCF(CGImageCreate(bufferWidth, bufferHeight, 8, 32, rowBytes, sRGBSpace, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host, provider.get(), 0, false, kCGRenderingIntentDefault));

    void* bitmapBuffer = nullptr;
    size_t bitmapRowBytes = 0;
    auto bitmapContext = createBitmapContext(bufferWidth, bufferHeight, bitmapRowBytes, bitmapBuffer);
    if (!bitmapContext) {
        CARenderServerDestroyBuffer(buffer);
        return nullptr;
    }

    CGContextDrawImage(bitmapContext->cgContext(), CGRectMake(0, 0, bufferWidth, bufferHeight), cgImage.get());
    CARenderServerDestroyBuffer(buffer);

    return bitmapContext;
#endif

    END_BLOCK_OBJC_EXCEPTIONS;
}
