/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#import "ThemeCocoa.h"

#import "FontCascade.h"
#import "GraphicsContext.h"
#import <dlfcn.h>

namespace WebCore {

static void fitContextToBox(GraphicsContext& context, const FloatSize& srcImageSize, const FloatSize& dstSize)
{
    float srcRatio = srcImageSize.aspectRatio();
    float dstRatio = dstSize.aspectRatio();

    float scale;
    float translationX = 0;
    float translationY = 0;
    if (srcRatio > dstRatio) {
        scale = dstSize.width() / srcImageSize.width();
        translationY = (dstSize.height() - scale * srcImageSize.height()) / 2;
    } else {
        scale = dstSize.height() / srcImageSize.height();
        translationX = (dstSize.width() - scale * srcImageSize.width()) / 2;
    }
    context.translate(translationX, translationY);
    context.scale(scale);
}

#if ENABLE(APPLE_PAY)

static NSBundle *passKitBundle()
{
    static NSBundle *passKitBundle;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
#if PLATFORM(MAC)
        passKitBundle = [NSBundle bundleWithURL:[NSURL fileURLWithPath:@"/System/Library/PrivateFrameworks/PassKit.framework" isDirectory:YES]];
#else
        dlopen("/System/Library/Frameworks/PassKit.framework/PassKit", RTLD_NOW);
        passKitBundle = [NSBundle bundleForClass:NSClassFromString(@"PKPaymentAuthorizationViewController")];
#endif
    });

    return passKitBundle;
}

static RetainPtr<CGPDFPageRef> loadPassKitPDFPage(NSString *imageName)
{
    NSURL *url = [passKitBundle() URLForResource:imageName withExtension:@"pdf"];
    if (!url)
        return nullptr;

    auto document = adoptCF(CGPDFDocumentCreateWithURL((CFURLRef)url));
    if (!document)
        return nullptr;

    if (!CGPDFDocumentGetNumberOfPages(document.get()))
        return nullptr;

    return CGPDFDocumentGetPage(document.get(), 1);
};

static CGPDFPageRef applePayButtonLogoBlack()
{
    static CGPDFPageRef logoPage;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        logoPage = loadPassKitPDFPage(@"PayButtonLogoBlack").leakRef();
    });

    return logoPage;
};

static CGPDFPageRef applePayButtonLogoWhite()
{
    static CGPDFPageRef logoPage;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        logoPage = loadPassKitPDFPage(@"PayButtonLogoWhite").leakRef();
    });

    return logoPage;
};

static void drawApplePayButton(GraphicsContext& context, CGPDFPageRef page, const FloatRect& rect)
{
    CGSize pdfSize = CGPDFPageGetBoxRect(page, kCGPDFMediaBox).size;
    GraphicsContextStateSaver stateSaver(context);
    fitContextToBox(context, FloatSize(pdfSize), rect.size());

    CGContextTranslateCTM(context.platformContext(), 0, pdfSize.height);
    CGContextScaleCTM(context.platformContext(), 1, -1);

    CGContextDrawPDFPage(context.platformContext(), page);
};

#endif

void ThemeCocoa::drawNamedImage(const String& name, GraphicsContext& context, const FloatRect& rect) const
{
    if (name == "wireless-playback") {
        GraphicsContextStateSaver stateSaver(context);
        context.setFillColor(Color::black);

        FloatSize wirelessPlaybackSrcSize(32, 24.016);
        fitContextToBox(context, wirelessPlaybackSrcSize, rect.size());

        Path outline;
        outline.moveTo(FloatPoint(24.066, 18));
        outline.addLineTo(FloatPoint(22.111, 16));
        outline.addLineTo(FloatPoint(30, 16));
        outline.addLineTo(FloatPoint(30, 2));
        outline.addLineTo(FloatPoint(2, 2));
        outline.addLineTo(FloatPoint(2, 16));
        outline.addLineTo(FloatPoint(9.908, 16));
        outline.addLineTo(FloatPoint(7.953, 18));
        outline.addLineTo(FloatPoint(0, 18));
        outline.addLineTo(FloatPoint(0, 0));
        outline.addLineTo(FloatPoint(32, 0));
        outline.addLineTo(FloatPoint(32, 18));
        outline.addLineTo(FloatPoint(24.066, 18));
        outline.closeSubpath();
        outline.moveTo(FloatPoint(26.917, 24.016));
        outline.addLineTo(FloatPoint(5.040, 24.016));
        outline.addLineTo(FloatPoint(15.978, 12.828));
        outline.addLineTo(FloatPoint(26.917, 24.016));
        outline.closeSubpath();

        context.fillPath(outline);
        return;
    }

#if ENABLE(APPLE_PAY)
    if (name == "apple-pay-logo-black") {
        if (auto logo = applePayButtonLogoBlack()) {
            drawApplePayButton(context, logo, rect);
            return;
        }
    }

    if (name == "apple-pay-logo-white") {
        if (auto logo = applePayButtonLogoWhite()) {
            drawApplePayButton(context, logo, rect);
            return;
        }
    }
#endif

    Theme::drawNamedImage(name, context, rect);
}

}
