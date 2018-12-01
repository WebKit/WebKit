/*
 * Copyright (C) 2007, 2009, 2012 Apple Inc. All rights reserved.
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

#if ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)

#import "BitmapImage.h"
#import "ColorMac.h"
#import "Element.h"
#import "FloatRoundedRect.h"
#import "FontCascade.h"
#import "FontDescription.h"
#import "FontSelector.h"
#import "GraphicsContext.h"
#import "Image.h"
#import "LocalDefaultSystemAppearance.h"
#import "Page.h"
#import "StringTruncator.h"
#import "TextIndicator.h"
#import "WebKitNSImageExtras.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/CoreTextSPI.h>
#import <pal/spi/cocoa/URLFormattingSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/URL.h>

#if !HAVE(URL_FORMATTING) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(LinkPresentation)
#endif

namespace WebCore {

IntSize dragImageSize(RetainPtr<NSImage> image)
{
    return (IntSize)[image size];
}

void deleteDragImage(RetainPtr<NSImage>)
{
    // Since this is a RetainPtr, there's nothing additional we need to do to
    // delete it. It will be released when it falls out of scope.
}

RetainPtr<NSImage> scaleDragImage(RetainPtr<NSImage> image, FloatSize scale)
{
    NSSize originalSize = [image size];
    NSSize newSize = NSMakeSize((originalSize.width * scale.width()), (originalSize.height * scale.height()));
    newSize.width = roundf(newSize.width);
    newSize.height = roundf(newSize.height);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [image setScalesWhenResized:YES];
    ALLOW_DEPRECATED_DECLARATIONS_END
    [image setSize:newSize];
    return image;
}
    
RetainPtr<NSImage> dissolveDragImageToFraction(RetainPtr<NSImage> image, float delta)
{
    if (!image)
        return nil;

    RetainPtr<NSImage> dissolvedImage = adoptNS([[NSImage alloc] initWithSize:[image size]]);
    
    [dissolvedImage lockFocus];
    [image drawAtPoint:NSZeroPoint fromRect:NSMakeRect(0, 0, [image size].width, [image size].height) operation:NSCompositingOperationCopy fraction:delta];
    [dissolvedImage unlockFocus];

    return dissolvedImage;
}
        
RetainPtr<NSImage> createDragImageFromImage(Image* image, ImageOrientationDescription description)
{
    FloatSize size = image->size();

    if (is<BitmapImage>(*image)) {
        ImageOrientation orientation;
        BitmapImage& bitmapImage = downcast<BitmapImage>(*image);
        IntSize sizeRespectingOrientation = bitmapImage.sizeRespectingOrientation();

        if (description.respectImageOrientation() == RespectImageOrientation)
            orientation = bitmapImage.orientationForCurrentFrame();

        if (orientation != DefaultImageOrientation) {
            // Construct a correctly-rotated copy of the image to use as the drag image.
            FloatRect destRect(FloatPoint(), sizeRespectingOrientation);

            RetainPtr<NSImage> rotatedDragImage = adoptNS([[NSImage alloc] initWithSize:(NSSize)(sizeRespectingOrientation)]);
            [rotatedDragImage lockFocus];

            // ImageOrientation uses top-left coordinates, need to flip to bottom-left, apply...
            CGAffineTransform transform = CGAffineTransformMakeTranslation(0, destRect.height());
            transform = CGAffineTransformScale(transform, 1, -1);
            transform = CGAffineTransformConcat(orientation.transformFromDefault(sizeRespectingOrientation), transform);

            if (orientation.usesWidthAsHeight())
                destRect = FloatRect(destRect.x(), destRect.y(), destRect.height(), destRect.width());

            // ...and flip back.
            transform = CGAffineTransformTranslate(transform, 0, destRect.height());
            transform = CGAffineTransformScale(transform, 1, -1);

            RetainPtr<NSAffineTransform> cocoaTransform = adoptNS([[NSAffineTransform alloc] init]);
            [cocoaTransform setTransformStruct:*(NSAffineTransformStruct*)&transform];
            [cocoaTransform concat];

            [image->snapshotNSImage() drawInRect:destRect fromRect:NSMakeRect(0, 0, size.width(), size.height()) operation:NSCompositingOperationSourceOver fraction:1.0];

            [rotatedDragImage unlockFocus];

            return rotatedDragImage;
        }
    }

    auto dragImage = image->snapshotNSImage();
    [dragImage setSize:(NSSize)size];
    return dragImage;
}
    
RetainPtr<NSImage> createDragImageIconForCachedImageFilename(const String& filename)
{
    NSString *extension = nil;
    size_t dotIndex = filename.reverseFind('.');
    
    if (dotIndex != notFound && dotIndex < (filename.length() - 1)) // require that a . exists after the first character and before the last
        extension = filename.substring(dotIndex + 1);
    else {
        // It might be worth doing a further lookup to pull the extension from the MIME type.
        extension = @"";
    }
    
    return [[NSWorkspace sharedWorkspace] iconForFileType:extension];
}

const CGFloat linkImagePadding = 10;
const CGFloat linkImageDomainBaselineToTitleBaseline = 18;
const CGFloat linkImageCornerRadius = 5;
const CGFloat linkImageMaximumWidth = 400;
const CGFloat linkImageFontSize = 11;
const CFIndex linkImageTitleMaximumLineCount = 2;
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
const int linkImageShadowRadius = 0;
const int linkImageShadowOffsetY = 0;
#else
const int linkImageShadowRadius = 9;
const int linkImageShadowOffsetY = -3;
#endif
const int linkImageDragCornerOutsetX = 6 - linkImageShadowRadius;
const int linkImageDragCornerOutsetY = 10 - linkImageShadowRadius + linkImageShadowOffsetY;

IntPoint dragOffsetForLinkDragImage(DragImageRef dragImage)
{
    IntSize size = dragImageSize(dragImage);
    return { linkImageDragCornerOutsetX, size.height() + linkImageDragCornerOutsetY };
}

FloatPoint anchorPointForLinkDragImage(DragImageRef dragImage)
{
    IntSize size = dragImageSize(dragImage);
    return { -static_cast<float>(linkImageDragCornerOutsetX) / size.width(), -static_cast<float>(linkImageDragCornerOutsetY) / size.height() };
}

struct LinkImageLayout {
    LinkImageLayout(URL&, const String& title);

    struct Label {
        FloatPoint origin;
        RetainPtr<CTFrameRef> frame;
    };
    Vector<Label> labels;

    FloatRect boundingRect;
};

LinkImageLayout::LinkImageLayout(URL& url, const String& titleString)
{
    NSString *title = nsStringNilIfEmpty(titleString);
    NSURL *cocoaURL = url;
    NSString *absoluteURLString = [cocoaURL absoluteString];

    NSString *domain = absoluteURLString;
#if HAVE(URL_FORMATTING)
    domain = [cocoaURL _lp_simplifiedDisplayString];
#elif __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    if (LinkPresentationLibrary())
        domain = [cocoaURL _lp_simplifiedDisplayString];
#endif

    if ([title isEqualToString:absoluteURLString])
        title = nil;

    NSFont *titleFont = [NSFont boldSystemFontOfSize:linkImageFontSize];
    NSFont *domainFont = [NSFont systemFontOfSize:linkImageFontSize];

    NSColor *titleColor = [NSColor labelColor];
    NSColor *domainColor = [NSColor secondaryLabelColor];

    CGFloat maximumAvailableWidth = linkImageMaximumWidth - linkImagePadding * 2;

    CGFloat currentY = linkImagePadding;
    CGFloat maximumUsedTextWidth = 0;

    auto buildLines = [this, maximumAvailableWidth, &maximumUsedTextWidth, &currentY] (NSString *text, NSColor *color, NSFont *font, CFIndex maximumLines, CTLineBreakMode lineBreakMode) {
        CTParagraphStyleSetting paragraphStyleSettings[1];
        paragraphStyleSettings[0].spec = kCTParagraphStyleSpecifierLineBreakMode;
        paragraphStyleSettings[0].valueSize = sizeof(CTLineBreakMode);
        paragraphStyleSettings[0].value = &lineBreakMode;
        RetainPtr<CTParagraphStyleRef> paragraphStyle = adoptCF(CTParagraphStyleCreate(paragraphStyleSettings, 1));

        NSDictionary *textAttributes = @{
            (id)kCTFontAttributeName: font,
            (id)kCTForegroundColorAttributeName: color,
            (id)kCTParagraphStyleAttributeName: (id)paragraphStyle.get()
        };
        NSDictionary *frameAttributes = @{
            (id)kCTFrameMaximumNumberOfLinesAttributeName: @(maximumLines)
        };

        RetainPtr<NSAttributedString> attributedText = adoptNS([[NSAttributedString alloc] initWithString:text attributes:textAttributes]);
        RetainPtr<CTFramesetterRef> textFramesetter = adoptCF(CTFramesetterCreateWithAttributedString((CFAttributedStringRef)attributedText.get()));

        CFRange fitRange;
        CGSize textSize = CTFramesetterSuggestFrameSizeWithConstraints(textFramesetter.get(), CFRangeMake(0, 0), (CFDictionaryRef)frameAttributes, CGSizeMake(maximumAvailableWidth, CGFLOAT_MAX), &fitRange);

        RetainPtr<CGPathRef> textPath = adoptCF(CGPathCreateWithRect(CGRectMake(0, 0, textSize.width, textSize.height), nullptr));
        RetainPtr<CTFrameRef> textFrame = adoptCF(CTFramesetterCreateFrame(textFramesetter.get(), fitRange, textPath.get(), (CFDictionaryRef)frameAttributes));

        CFArrayRef ctLines = CTFrameGetLines(textFrame.get());
        CFIndex lineCount = CFArrayGetCount(ctLines);
        if (!lineCount)
            return;

        Vector<CGPoint> origins(lineCount);
        CGRect lineBounds;
        CGFloat height = 0;
        CTFrameGetLineOrigins(textFrame.get(), CFRangeMake(0, 0), origins.data());
        for (CFIndex lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
            CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(ctLines, lineIndex);

            lineBounds = CTLineGetBoundsWithOptions(line, 0);
            CGFloat trailingWhitespaceWidth = CTLineGetTrailingWhitespaceWidth(line);
            CGFloat lineWidthIgnoringTrailingWhitespace = lineBounds.size.width - trailingWhitespaceWidth;
            maximumUsedTextWidth = std::max(maximumUsedTextWidth, lineWidthIgnoringTrailingWhitespace);

            if (lineIndex)
                height += origins[lineIndex - 1].y - origins[lineIndex].y;
        }

        LinkImageLayout::Label label;
        label.frame = textFrame;
        label.origin = FloatPoint(linkImagePadding, currentY + origins[0].y);
        labels.append(label);

        currentY += height + lineBounds.size.height;
    };

    if (title)
        buildLines(title, titleColor, titleFont, linkImageTitleMaximumLineCount, kCTLineBreakByTruncatingTail);

    if (title && domain)
        currentY += linkImageDomainBaselineToTitleBaseline - (domainFont.ascender - domainFont.descender);

    if (domain)
        buildLines(domain, domainColor, domainFont, 1, kCTLineBreakByTruncatingMiddle);

    currentY += linkImagePadding;

    boundingRect = FloatRect(0, 0, maximumUsedTextWidth + linkImagePadding * 2, currentY);

    // To work around blurry drag images on 1x displays, make the width and height a multiple of 2.
    // FIXME: remove this workaround when <rdar://problem/33059739> is fixed.
    boundingRect.setWidth((static_cast<int>(boundingRect.width()) / 2) * 2);
    boundingRect.setHeight((static_cast<int>(boundingRect.height() / 2) * 2));
}

DragImageRef createDragImageForLink(Element& element, URL& url, const String& title, TextIndicatorData&, FontRenderingMode, float deviceScaleFactor)
{
    LinkImageLayout layout(url, title);

    LocalDefaultSystemAppearance localAppearance(element.document().useDarkAppearance(element.computedStyle()));

    auto imageSize = layout.boundingRect.size();
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101300
    imageSize.expand(2 * linkImageShadowRadius, 2 * linkImageShadowRadius - linkImageShadowOffsetY);
#endif
    RetainPtr<NSImage> dragImage = adoptNS([[NSImage alloc] initWithSize:imageSize]);
    [dragImage _web_lockFocusWithDeviceScaleFactor:deviceScaleFactor];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    GraphicsContext context((CGContextRef)[NSGraphicsContext currentContext].graphicsPort);
    ALLOW_DEPRECATED_DECLARATIONS_END

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101300
    context.translate(linkImageShadowRadius, linkImageShadowRadius - linkImageShadowOffsetY);
    context.setShadow({ 0, linkImageShadowOffsetY }, linkImageShadowRadius, { 0.f, 0.f, 0.f, .25 });
#endif
    context.fillRoundedRect(FloatRoundedRect(layout.boundingRect, FloatRoundedRect::Radii(linkImageCornerRadius)), colorFromNSColor([NSColor controlBackgroundColor]));
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101300
    context.clearShadow();
#endif

    for (const auto& label : layout.labels) {
        GraphicsContextStateSaver saver(context);
        context.translate(label.origin.x(), layout.boundingRect.height() - label.origin.y() - linkImagePadding);
        CTFrameDraw(label.frame.get(), context.platformContext());
    }

    [dragImage unlockFocus];

    return dragImage;
}

DragImageRef createDragImageForColor(const Color& color, const FloatRect&, float, Path&)
{
    auto dragImage = adoptNS([[NSImage alloc] initWithSize:NSMakeSize(ColorSwatchWidth, ColorSwatchWidth)]);

    [dragImage lockFocus];

    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(0, 0, ColorSwatchWidth, ColorSwatchWidth) xRadius:ColorSwatchCornerRadius yRadius:ColorSwatchCornerRadius];
    [path setLineWidth:ColorSwatchStrokeSize];

    [nsColor(color) setFill];
    [path fill];

    [[NSColor quaternaryLabelColor] setStroke];
    [path stroke];

    [dragImage unlockFocus];

    return dragImage;
}
   
} // namespace WebCore

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
