/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "ApplePayLogoSystemImage.h"

#if ENABLE(APPLE_PAY)

#import "FloatRect.h"
#import "GeometryUtilities.h"
#import "GraphicsContext.h"
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>

namespace WebCore {

static NSBundle *passKitBundle()
{
    static NSBundle *passKitBundle;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        passKitBundle = [NSBundle bundleWithURL:[NSURL fileURLWithPath:[FileSystem::systemDirectoryPath() stringByAppendingPathComponent:@"Library/Frameworks/PassKit.framework"] isDirectory:YES]];
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
}

static RetainPtr<CGPDFPageRef> applePayLogoWhite()
{
    static NeverDestroyed<RetainPtr<CGPDFPageRef>> logoPage;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        logoPage.get() = loadPassKitPDFPage(@"PayButtonLogoWhite");
    });
    return logoPage;
}

static RetainPtr<CGPDFPageRef> applePayLogoBlack()
{
    static NeverDestroyed<RetainPtr<CGPDFPageRef>> logoPage;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        logoPage.get() = loadPassKitPDFPage(@"PayButtonLogoBlack");
    });
    return logoPage;
}

static RetainPtr<CGPDFPageRef> applePayLogoForStyle(ApplePayLogoStyle style)
{
    switch (style) {
    case ApplePayLogoStyle::White:
        return applePayLogoWhite();

    case ApplePayLogoStyle::Black:
        return applePayLogoBlack();
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void ApplePayLogoSystemImage::draw(GraphicsContext& context, const FloatRect& destinationRect) const
{
    auto page = applePayLogoForStyle(m_applePayLogoStyle);
    if (!page)
        return;

    CGSize pdfSize = CGPDFPageGetBoxRect(page.get(), kCGPDFMediaBox).size;

    auto largestRect = largestRectWithAspectRatioInsideRect(pdfSize.width / pdfSize.height, destinationRect);
    context.translate(largestRect.x(), largestRect.y());
    context.scale(largestRect.width() / pdfSize.width);

    context.translate(0, pdfSize.height);
    context.scale(FloatSize(1, -1));

    CGContextDrawPDFPage(context.platformContext(), page.get());
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
