/*
 * Copyright (C) 2005-2021 Apple Inc. All rights reserved.
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
#import "UIKitSPIForTesting.h"

#define COMMON_DIGEST_FOR_OPENSSL
#import <CommonCrypto/CommonDigest.h>
#import <MobileCoreServices/UTCoreTypes.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/DestinationColorSpace.h>
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

extern RetainPtr<DumpRenderTreeWindow> gDrtWindow;
extern RetainPtr<DumpRenderTreeBrowserView> gWebBrowserView;

RefPtr<BitmapContext> createBitmapContextFromWebView(bool onscreen, bool incrementalRepaint, bool sweepHorizontally, bool drawSelectionRect)
{
    WebThreadLock();
    [CATransaction flush];

    CGFloat deviceScaleFactor = 2; // FIXME: hardcode 2x for now. In future we could respect 1x and 3x as we do on Mac.
    
    CGSize viewSize = [[mainFrame webView] frame].size;
    int bufferWidth = ceil(viewSize.width * deviceScaleFactor);
    int bufferHeight = ceil(viewSize.height * deviceScaleFactor);

    WebCore::FloatSize snapshotSize(viewSize);
    snapshotSize.scale(deviceScaleFactor);

#if HAVE(IOSURFACE_RGB10)
    WebCore::IOSurface::Format snapshotFormat = WebCore::screenSupportsExtendedColor() ? WebCore::IOSurface::Format::RGB10 : WebCore::IOSurface::Format::BGRA;
#else
    WebCore::IOSurface::Format snapshotFormat = WebCore::IOSurface::Format::BGRA;
#endif
    auto surface = WebCore::IOSurface::create(nullptr, WebCore::expandedIntSize(snapshotSize), WebCore::DestinationColorSpace::SRGB(), WebCore::IOSurface::Name::Snapshot, snapshotFormat);
    // FIXME: Something is missing here, nothing draws to surface.
    auto context = surface->createPlatformContext();
    RetainPtr<CGImageRef> cgImage = surface->createImage(context.get());

    size_t bitmapRowBytes = 0;
    auto bitmapContext = createBitmapContext(bufferWidth, bufferHeight, bitmapRowBytes);
    if (!bitmapContext)
        return nullptr;

    bitmapContext->setScaleFactor(deviceScaleFactor);

    CGContextDrawImage(bitmapContext->cgContext(), CGRectMake(0, 0, bufferWidth, bufferHeight), cgImage.get());
    return bitmapContext;
}
