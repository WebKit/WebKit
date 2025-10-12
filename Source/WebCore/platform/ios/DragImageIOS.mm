/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "DragImage.h"

#if PLATFORM(IOS_FAMILY)

#import "Document.h"
#import "Element.h"
#import "FloatRoundedRect.h"
#import "FontCascade.h"
#import "FontPlatformData.h"
#import "GeometryUtilities.h"
#import "GraphicsClient.h"
#import "GraphicsContext.h"
#import "GraphicsContextCG.h"
#import "Image.h"
#import "ImageBuffer.h"
#import "LocalFrameInlines.h"
#import "NativeImage.h"
#import "NotImplemented.h"
#import "Page.h"
#import "Range.h"
#import "SimpleRange.h"
#import "StringTruncator.h"
#import "TextIndicator.h"
#import "TextRun.h"
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#import <UIKit/UIColor.h>
#import <UIKit/UIFont.h>
#import <UIKit/UIGraphicsImageRenderer.h>
#import <UIKit/UIImage.h>
#import <pal/ios/UIKitSoftLink.h>
#import <wtf/NeverDestroyed.h>

namespace WebCore {

#if ENABLE(DRAG_SUPPORT)

IntSize dragImageSize(DragImageRef image)
{
    return IntSize(CGImageGetWidth(image.get()), CGImageGetHeight(image.get()));
}

DragImageRef scaleDragImage(DragImageRef image, FloatSize)
{
    // It's unnecessary to apply additional scaling to the drag image on iOS, since the drag image will always perform lift and cancel animations
    // using targeted previews which will scale to fit the bounds of the dragged element anyways.
    return image;
}

static float maximumAllowedDragImageArea = 600 * 1024;

DragImageRef createDragImageFromImage(Image* image, ImageOrientation orientation, GraphicsClient* client, float scale)
{
    if (!image)
        return nil;

    auto imageSize = image->size();
    if (imageSize.isEmpty())
        return nil;

    float adjustedImageScale = 1;
    if (imageSize.area() > maximumAllowedDragImageArea) {
        auto adjustedSize = sizeWithAreaAndAspectRatio(maximumAllowedDragImageArea, imageSize.width() / imageSize.height());
        adjustedImageScale = adjustedSize.width() / imageSize.width();
        imageSize = adjustedSize;
    }

    RefPtr buffer = ImageBuffer::create(imageSize, RenderingMode::Accelerated, RenderingPurpose::Snapshot, scale, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, client);
    if (!buffer)
        return nil;

    buffer->context().translate(0, imageSize.height());
    buffer->context().scale({ adjustedImageScale, -adjustedImageScale });
    buffer->context().drawImage(*image, FloatPoint { }, { orientation });

    RefPtr nativeImage = ImageBuffer::sinkIntoNativeImage(WTFMove(buffer));
    if (!nativeImage)
        return nil;

    return RetainPtr { nativeImage->platformImage() }.autorelease();
}

void deleteDragImage(DragImageRef)
{
}

static RetainPtr<CGImageRef> cgImageFromTextIndicator(const TextIndicator& textIndicator)
{
    RefPtr image = textIndicator.contentImage();
    if (!image)
        return { };

    RefPtr nativeImage = image->nativeImage();
    if (!nativeImage)
        return { };

    return nativeImage->platformImage();
}

DragImageData createDragImageForLink(Element& linkElement, URL&, const String&, float)
{
    constexpr OptionSet<TextIndicatorOption> defaultLinkIndicatorOptions {
        TextIndicatorOption::TightlyFitContent,
        TextIndicatorOption::RespectTextColor,
        TextIndicatorOption::UseBoundingRectAndPaintAllContentForComplexRanges,
        TextIndicatorOption::ExpandClipBeyondVisibleRect,
        TextIndicatorOption::ComputeEstimatedBackgroundColor
    };

    RefPtr textIndicator = TextIndicator::createWithRange(makeRangeSelectingNodeContents(linkElement), defaultLinkIndicatorOptions, TextIndicatorPresentationTransition::None, { });
    if (!textIndicator)
        return  { nullptr, nullptr };

    return  { cgImageFromTextIndicator(*textIndicator).autorelease(), textIndicator };
}

DragImageRef createDragImageIconForCachedImageFilename(const String&)
{
    notImplemented();
    return nullptr;
}

DragImageRef platformAdjustDragImageForDeviceScaleFactor(DragImageRef image, float)
{
    // On iOS, we just create the drag image at the right device scale factor, so we don't need to scale it by 1 / deviceScaleFactor later.
    return image;
}

constexpr OptionSet<TextIndicatorOption> defaultSelectionDragImageTextIndicatorOptions {
    TextIndicatorOption::ExpandClipBeyondVisibleRect,
    TextIndicatorOption::PaintAllContent,
    TextIndicatorOption::UseSelectionRectForSizing,
    TextIndicatorOption::ComputeEstimatedBackgroundColor
};

DragImageData createDragImageForSelection(LocalFrame& frame, bool forceBlackText)
{
    if (auto document = frame.document())
        document->updateLayout();

    auto options = defaultSelectionDragImageTextIndicatorOptions;
    if (!forceBlackText)
        options.add(TextIndicatorOption::RespectTextColor);

    RefPtr textIndicator = TextIndicator::createWithSelectionInFrame(frame, options, TextIndicatorPresentationTransition::None, FloatSize());
    if (!textIndicator)
        return { nullptr, nullptr };

    return { cgImageFromTextIndicator(*textIndicator).autorelease(), textIndicator };
}

DragImageRef dissolveDragImageToFraction(DragImageRef image, float)
{
    notImplemented();
    return image;
}

DragImageRef createDragImageForRange(LocalFrame& frame, const SimpleRange& range, bool forceBlackText)
{
    if (auto document = frame.document())
        document->updateLayout();

    if (range.collapsed())
        return nil;

    auto options = defaultSelectionDragImageTextIndicatorOptions;
    if (!forceBlackText)
        options.add(TextIndicatorOption::RespectTextColor);

    auto textIndicator = TextIndicator::createWithRange(range, options, TextIndicatorPresentationTransition::None);
    if (!textIndicator || !textIndicator->contentImage())
        return nil;

    auto& image = *textIndicator->contentImage();
    auto render = adoptNS([PAL::allocUIGraphicsImageRendererInstance() initWithSize:image.size()]);
    UIImage *finalImage = [render imageWithActions:[&image](UIGraphicsImageRendererContext *rendererContext) {
        GraphicsContextCG context(rendererContext.CGContext);
        context.drawImage(image, FloatPoint());
    }];

    return finalImage.CGImage;
}

DragImageRef createDragImageForColor(const Color& color, const FloatRect& elementRect, float pageScaleFactor, Path& visiblePath)
{
    FloatRect imageRect { 0, 0, elementRect.width() * pageScaleFactor, elementRect.height() * pageScaleFactor };
    FloatRoundedRect swatch { imageRect, FloatRoundedRect::Radii(ColorSwatchCornerRadius * pageScaleFactor) };

    auto render = adoptNS([PAL::allocUIGraphicsImageRendererInstance() initWithSize:imageRect.size()]);
    UIImage *image = [render imageWithActions:^(UIGraphicsImageRendererContext *rendererContext) {
        GraphicsContextCG context { rendererContext.CGContext };
        context.translate(0, CGRectGetHeight(imageRect));
        context.scale({ 1, -1 });
        context.fillRoundedRect(swatch, color);
    }];

    visiblePath.addRoundedRect(swatch);
    return image.CGImage;
}

#else

void deleteDragImage(RetainPtr<CGImageRef>)
{
    // Since this is a RetainPtr, there's nothing additional we need to do to
    // delete it. It will be released when it falls out of scope.
}

// FIXME: fix signature of dragImageSize() to avoid copying the argument.
IntSize dragImageSize(RetainPtr<CGImageRef> image)
{
    return IntSize(CGImageGetWidth(image.get()), CGImageGetHeight(image.get()));
}

RetainPtr<CGImageRef> scaleDragImage(RetainPtr<CGImageRef>, FloatSize)
{
    return nullptr;
}

RetainPtr<CGImageRef> createDragImageFromImage(Image*, ImageOrientation, GraphicsClient*, float)
{
    return nullptr;
}

DragImageRef createDragImageForRange(LocalFrame&, const SimpleRange&, bool)
{
    return nullptr;
}

#endif

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
