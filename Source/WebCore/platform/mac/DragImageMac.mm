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
#import "CoreGraphicsSPI.h"
#import "Element.h"
#import "FloatRoundedRect.h"
#import "FontCascade.h"
#import "FontDescription.h"
#import "FontSelector.h"
#import "GraphicsContext.h"
#import "Image.h"
#import "SoftLinking.h"
#import "StringTruncator.h"
#import "TextIndicator.h"
#import "TextRun.h"
#import "URL.h"
#import <wtf/NeverDestroyed.h>

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
#import <LinkPresentation/LPNSURLExtras.h>
SOFT_LINK_PRIVATE_FRAMEWORK(LinkPresentation)
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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [image setScalesWhenResized:YES];
#pragma clang diagnostic pop
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


const CGFloat linkImagePadding = 10; // Keep in sync with DragController::LinkDragBorderInset.
const CGFloat linkImageDomainBaselineToTitleBaseline = 18;
const CGFloat linkImageCornerRadius = 5;
const CGFloat linkImageMaximumWidth = 400;
const CGFloat linkImageFontSize = 11;
const CFIndex linkImageTitleMaximumLineCount = 2;

struct LinkImageLayout {
    LinkImageLayout(URL&, const String& title);

    struct LabelLine {
        FloatPoint origin;
        RetainPtr<CTLineRef> line;
    };
    Vector<LabelLine> lines;

    FloatRect boundingRect;

private:
    void addLine(CTLineRef, Vector<CGPoint>& origins, CFIndex lineIndex, CGFloat x, CGFloat& y, CGFloat& maximumUsedTextWidth, bool isLastLine);
};

LinkImageLayout::LinkImageLayout(URL& url, const String& titleString)
{
    NSString *title = nsStringNilIfEmpty(titleString);
    NSURL *cocoaURL = url;
    NSString *absoluteURLString = [cocoaURL absoluteString];

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
    LinkPresentationLibrary();
    NSString *domain = [cocoaURL _lp_simplifiedDisplayString];
#else
    NSString *domain = absoluteURLString;
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

    auto buildLines = [this, maximumAvailableWidth, &maximumUsedTextWidth, &currentY] (NSString *text, NSColor *color, NSFont *font, CFIndex maximumLines, CTLineTruncationType truncationType) {
        NSDictionary *textAttributes = @{
            (id)kCTFontAttributeName: font,
            (id)kCTForegroundColorAttributeName: color
        };
        RetainPtr<NSAttributedString> attributedText = adoptNS([[NSAttributedString alloc] initWithString:text attributes:textAttributes]);
        RetainPtr<CTFramesetterRef> textFramesetter = adoptCF(CTFramesetterCreateWithAttributedString((CFAttributedStringRef)attributedText.get()));

        CFRange fitRange;
        CGSize textSize = CTFramesetterSuggestFrameSizeWithConstraints(textFramesetter.get(), CFRangeMake(0, 0), nullptr, CGSizeMake(maximumAvailableWidth, CGFLOAT_MAX), &fitRange);

        RetainPtr<CGPathRef> textPath = adoptCF(CGPathCreateWithRect(CGRectMake(0, 0, textSize.width, textSize.height), nullptr));
        RetainPtr<CTFrameRef> textFrame = adoptCF(CTFramesetterCreateFrame(textFramesetter.get(), fitRange, textPath.get(), nullptr));

        CFArrayRef ctLines = CTFrameGetLines(textFrame.get());
        CFIndex lineCount = CFArrayGetCount(ctLines);
        if (!lineCount)
            return;

        Vector<CGPoint> origins(lineCount);
        CTFrameGetLineOrigins(textFrame.get(), CFRangeMake(0, 0), origins.data());

        // Lay out and record the first (maximumLines - 1) lines.
        CFIndex lineIndex = 0;
        for (; lineIndex < std::min(maximumLines - 1, lineCount); ++lineIndex) {
            CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(ctLines, lineIndex);
            addLine(line, origins, lineIndex, linkImagePadding, currentY, maximumUsedTextWidth, lineIndex == lineCount - 1);
        }

        if (lineIndex == lineCount)
            return;

        // We had text that didn't fit in the first (maximumLines - 1) lines.
        // Combine it into one last line, and truncate it.
        CTLineRef firstRemainingLine = (CTLineRef)CFArrayGetValueAtIndex(ctLines, lineIndex);
        CFIndex remainingRangeStart = CTLineGetStringRange(firstRemainingLine).location;
        NSRange remainingRange = NSMakeRange(remainingRangeStart, [attributedText length] - remainingRangeStart);
        NSAttributedString *remainingString = [attributedText attributedSubstringFromRange:remainingRange];
        RetainPtr<CTLineRef> remainingLine = adoptCF(CTLineCreateWithAttributedString((CFAttributedStringRef)remainingString));
        RetainPtr<NSAttributedString> ellipsisString = adoptNS([[NSAttributedString alloc] initWithString:@"\u2026" attributes:textAttributes]);
        RetainPtr<CTLineRef> ellipsisLine = adoptCF(CTLineCreateWithAttributedString((CFAttributedStringRef)ellipsisString.get()));
        RetainPtr<CTLineRef> truncatedLine = adoptCF(CTLineCreateTruncatedLine(remainingLine.get(), maximumAvailableWidth, truncationType, ellipsisLine.get()));
        
        if (!truncatedLine)
            truncatedLine = remainingLine;
        
        addLine(truncatedLine.get(), origins, lineIndex, linkImagePadding, currentY, maximumUsedTextWidth, true);
    };

    if (title)
        buildLines(title, titleColor, titleFont, linkImageTitleMaximumLineCount, kCTLineTruncationEnd);

    if (title && domain)
        currentY += linkImageDomainBaselineToTitleBaseline - (domainFont.ascender - domainFont.descender);

    if (domain)
        buildLines(domain, domainColor, domainFont, 1, kCTLineTruncationMiddle);

    currentY += linkImagePadding;

    boundingRect = FloatRect(0, 0, maximumUsedTextWidth + linkImagePadding * 2, currentY);
}

void LinkImageLayout::addLine(CTLineRef ctLine, Vector<CGPoint>& origins, CFIndex lineIndex, CGFloat x, CGFloat& y, CGFloat& maximumUsedTextWidth, bool isLastLine)
{
    CGRect lineBounds = CTLineGetBoundsWithOptions(ctLine, 0);
    CGFloat trailingWhitespaceWidth = CTLineGetTrailingWhitespaceWidth(ctLine);
    CGFloat lineWidthIgnoringTrailingWhitespace = lineBounds.size.width - trailingWhitespaceWidth;
    maximumUsedTextWidth = std::max(maximumUsedTextWidth, lineWidthIgnoringTrailingWhitespace);

    if (lineIndex)
        y += origins[lineIndex - 1].y - origins[lineIndex].y;

    LinkImageLayout::LabelLine line;
    line.line = ctLine;
    line.origin = FloatPoint(x, y);
    lines.append(line);

    if (isLastLine)
        y += lineBounds.size.height;
}

DragImageRef createDragImageForLink(Element&, URL& url, const String& title, TextIndicatorData&, FontRenderingMode, float)
{
    LinkImageLayout layout(url, title);

    RetainPtr<NSImage> dragImage = adoptNS([[NSImage alloc] initWithSize:layout.boundingRect.size()]);
    [dragImage lockFocus];

    GraphicsContext context((CGContextRef)[NSGraphicsContext currentContext].graphicsPort);
    context.fillRoundedRect(FloatRoundedRect(layout.boundingRect, FloatRoundedRect::Radii(linkImageCornerRadius)), Color::white);

    for (const auto& line : layout.lines) {
        GraphicsContextStateSaver saver(context);
        context.translate(line.origin.x(), layout.boundingRect.height() - line.origin.y() - linkImagePadding);
        CGContextSetTextPosition(context.platformContext(), 0, 0);
        CTLineDraw(line.line.get(), context.platformContext());
    }

    [dragImage unlockFocus];

    return dragImage;
}
   
} // namespace WebCore

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
