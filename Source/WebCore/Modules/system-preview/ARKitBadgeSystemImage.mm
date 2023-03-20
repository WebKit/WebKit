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
#import "ARKitBadgeSystemImage.h"

#if USE(SYSTEM_PREVIEW)

#import "ColorSpaceCG.h"
#import "FloatRect.h"
#import "GeometryUtilities.h"
#import "GraphicsContext.h"
#import "IOSurfacePool.h"
#import <CoreGraphics/CoreGraphics.h>
#import <CoreImage/CoreImage.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>

namespace WebCore {

static NSBundle *arKitBundle()
{
    static NSBundle *arKitBundle;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        arKitBundle = []() {
#if PLATFORM(IOS_FAMILY_SIMULATOR)
            dlopen("/System/Library/PrivateFrameworks/AssetViewer.framework/AssetViewer", RTLD_NOW);
            return [NSBundle bundleForClass:NSClassFromString(@"ASVThumbnailView")];
#else
            return [NSBundle bundleWithURL:[NSURL fileURLWithPath:@"/System/Library/PrivateFrameworks/AssetViewer.framework"]];
#endif
        }();
    });
    return arKitBundle;
}

static RetainPtr<CGPDFPageRef> loadARKitPDFPage(NSString *imageName)
{
    NSURL *url = [arKitBundle() URLForResource:imageName withExtension:@"pdf"];
    if (!url)
        return nullptr;
    auto document = adoptCF(CGPDFDocumentCreateWithURL((CFURLRef)url));
    if (!document)
        return nullptr;
    if (!CGPDFDocumentGetNumberOfPages(document.get()))
        return nullptr;
    return CGPDFDocumentGetPage(document.get(), 1);
}

static RetainPtr<CGPDFPageRef> systemPreviewLogo()
{
    static NeverDestroyed<RetainPtr<CGPDFPageRef>> logoPage;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        logoPage.get() = loadARKitPDFPage(@"ARKitBadge");
    });
    return logoPage;
}

RenderingResourceIdentifier ARKitBadgeSystemImage::imageIdentifier() const
{
    return m_image ? m_image->nativeImage()->renderingResourceIdentifier() : m_renderingResourceIdentifier;
}

void ARKitBadgeSystemImage::draw(GraphicsContext& graphicsContext, const FloatRect& rect) const
{
    auto page = systemPreviewLogo();
    if (!page)
        return;

    static const int largeBadgeDimension = 70;
    static const int largeBadgeOffset = 20;

    static const int smallBadgeDimension = 35;
    static const int smallBadgeOffset = 8;

    static const int minimumSizeForLargeBadge = 240;

    bool useSmallBadge = rect.width() < minimumSizeForLargeBadge || rect.height() < minimumSizeForLargeBadge;
    int badgeOffset = useSmallBadge ? smallBadgeOffset : largeBadgeOffset;
    int badgeDimension = useSmallBadge ? smallBadgeDimension : largeBadgeDimension;

    int minimumDimension = badgeDimension + 2 * badgeOffset;
    if (rect.width() < minimumDimension || rect.height() < minimumDimension)
        return;

    CGRect absoluteBadgeRect = CGRectMake(rect.x() + rect.width() - badgeDimension - badgeOffset, rect.y() + badgeOffset, badgeDimension, badgeDimension);
    CGRect insetBadgeRect = CGRectMake(rect.width() - badgeDimension - badgeOffset, badgeOffset, badgeDimension, badgeDimension);
    CGRect badgeRect = CGRectMake(0, 0, badgeDimension, badgeDimension);

    if (!m_image || !m_image->nativeImage())
        return;

    CIImage *inputImage = [CIImage imageWithCGImage:m_image->nativeImage()->platformImage().get()];

    // Create a circle to be used for the clipping path in the badge, as well as the drop shadow.
    RetainPtr<CGPathRef> circle = adoptCF(CGPathCreateWithRoundedRect(absoluteBadgeRect, badgeDimension / 2, badgeDimension / 2, nullptr));

    if (graphicsContext.paintingDisabled())
        return;

    GraphicsContextStateSaver stateSaver(graphicsContext);

    CGContextRef ctx = graphicsContext.platformContext();
    if (!ctx)
        return;

    CGContextSaveGState(ctx);

    // Draw a drop shadow around the circle.
    // Use the GraphicsContext function, because it calculates the blur radius in context space,
    // rather than screen space.
    constexpr auto shadowColor = Color::black.colorWithAlphaByte(26);
    graphicsContext.setShadow(FloatSize { }, 16, shadowColor);

    // The circle must have an alpha channel value of 1 for the shadow color to appear.
    CGFloat circleColorComponents[4] = { 0, 0, 0, 1 };
    RetainPtr<CGColorRef> circleColor = adoptCF(CGColorCreate(sRGBColorSpaceRef(), circleColorComponents));
    CGContextSetFillColorWithColor(ctx, circleColor.get());

    // Clip out the circle to only show the shadow.
    CGContextBeginPath(ctx);
    CGContextAddRect(ctx, rect);
    CGContextAddPath(ctx, circle.get());
    CGContextClosePath(ctx);
    CGContextEOClip(ctx);

    // Draw a slightly smaller circle with a shadow, otherwise we'll see a fringe of the solid
    // black circle around the edges of the clipped path below.
    CGContextBeginPath(ctx);
    CGRect slightlySmallerAbsoluteBadgeRect = CGRectMake(absoluteBadgeRect.origin.x + 0.5, absoluteBadgeRect.origin.y + 0.5, badgeDimension - 1, badgeDimension - 1);
    RetainPtr<CGPathRef> slightlySmallerCircle = adoptCF(CGPathCreateWithRoundedRect(slightlySmallerAbsoluteBadgeRect, slightlySmallerAbsoluteBadgeRect.size.width / 2, slightlySmallerAbsoluteBadgeRect.size.height / 2, nullptr));
    CGContextAddPath(ctx, slightlySmallerCircle.get());
    CGContextClosePath(ctx);
    CGContextFillPath(ctx);

    CGContextRestoreGState(ctx);

    // Draw the blurred backdrop. Scale from intrinsic size to render size.
    CGAffineTransform transform = CGAffineTransformIdentity;
    transform = CGAffineTransformScale(transform, rect.width() / m_imageSize.width(), rect.height() / m_imageSize.height());
    CIImage *scaledImage = [inputImage imageByApplyingTransform:transform];

    // CoreImage coordinates are y-up, so we need to flip the badge rectangle within the image frame.
    CGRect flippedInsetBadgeRect = CGRectMake(insetBadgeRect.origin.x, rect.height() - insetBadgeRect.origin.y - insetBadgeRect.size.height, badgeDimension, badgeDimension);

    // Create a cropped region with pixel values extending outwards.
    CIImage *clampedImage = [scaledImage imageByClampingToRect:flippedInsetBadgeRect];

    // Blur.
    CIImage *blurredImage = [clampedImage imageByApplyingGaussianBlurWithSigma:10];

    // Saturate.
    CIFilter *saturationFilter = [CIFilter filterWithName:@"CIColorControls"];
    [saturationFilter setValue:blurredImage forKey:kCIInputImageKey];
    [saturationFilter setValue:@1.8 forKey:kCIInputSaturationKey];

    // Tint.
    CIFilter *tintFilter1 = [CIFilter filterWithName:@"CIConstantColorGenerator"];
    CIColor *tintColor1 = [CIColor colorWithRed:1 green:1 blue:1 alpha:0.18];
    [tintFilter1 setValue:tintColor1 forKey:kCIInputColorKey];

    // Blend the tint with the saturated output.
    CIFilter *sourceOverFilter = [CIFilter filterWithName:@"CISourceOverCompositing"];
    [sourceOverFilter setValue:tintFilter1.outputImage forKey:kCIInputImageKey];
    [sourceOverFilter setValue:saturationFilter.outputImage forKey:kCIInputBackgroundImageKey];

    RetainPtr<CIContext> ciContext = [CIContext context];

    RetainPtr<CGImageRef> cgImage;
#if HAVE(IOSURFACE_COREIMAGE_SUPPORT)
    // Crop the result to the badge location.
    CIImage *croppedImage = [sourceOverFilter.outputImage imageByCroppingToRect:flippedInsetBadgeRect];
    CIImage *translatedImage = [croppedImage imageByApplyingTransform:CGAffineTransformMakeTranslation(-flippedInsetBadgeRect.origin.x, -flippedInsetBadgeRect.origin.y)];

    auto surfaceDimension = useSmallBadge ? smallBadgeDimension : largeBadgeDimension;
    std::unique_ptr<IOSurface> badgeSurface = IOSurface::create(&IOSurfacePool::sharedPool(), { surfaceDimension, surfaceDimension }, DestinationColorSpace::SRGB());
    IOSurfaceRef surface = badgeSurface->surface();
    [ciContext render:translatedImage toIOSurface:surface bounds:badgeRect colorSpace:sRGBColorSpaceRef()];
    cgImage = useSmallBadge ? badgeSurface->createImage() : badgeSurface->createImage();
#else
    cgImage = adoptCF([ciContext createCGImage:sourceOverFilter.outputImage fromRect:flippedInsetBadgeRect]);
#endif

    // Before we render the result, we should clip to a circle around the badge rectangle.
    CGContextSaveGState(ctx);
    CGContextBeginPath(ctx);
    CGContextAddPath(ctx, circle.get());
    CGContextClosePath(ctx);
    CGContextClip(ctx);

    CGContextTranslateCTM(ctx, absoluteBadgeRect.origin.x, absoluteBadgeRect.origin.y);
    CGContextTranslateCTM(ctx, 0, badgeDimension);
    CGContextScaleCTM(ctx, 1, -1);
    CGContextDrawImage(ctx, badgeRect, cgImage.get());

    CGSize pdfSize = CGPDFPageGetBoxRect(page.get(), kCGPDFMediaBox).size;
    CGFloat scaleX = badgeDimension / pdfSize.width;
    CGFloat scaleY = badgeDimension / pdfSize.height;
    CGContextScaleCTM(ctx, scaleX, scaleY);
    CGContextDrawPDFPage(ctx, page.get());

    CGContextFlush(ctx);
    CGContextRestoreGState(ctx);
}

} // namespace WebCore

#endif // USE(SYSTEM_PREVIEW)
