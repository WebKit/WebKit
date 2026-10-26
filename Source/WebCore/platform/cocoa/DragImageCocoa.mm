/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
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

#import "ColorMac.h"
#import "NativeImage.h"

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

IntPoint platformAdjustDragImageHotSpot(const IntPoint& hotSpot, const IntSize& dragImageSize)
{
    // The hot spot is relative to the image's lower-left corner.
    return { hotSpot.x(), dragImageSize.height() - hotSpot.y() };
}

RetainPtr<NSImage> scaleDragImage(RetainPtr<NSImage> image, FloatSize scale)
{
    NSSize originalSize = [image size];
    NSSize newSize = NSMakeSize((originalSize.width * scale.width()), (originalSize.height * scale.height()));
    newSize.width = roundf(newSize.width);
    newSize.height = roundf(newSize.height);
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

RetainPtr<NSImage> createDragImageFromImage(NativeImage& image, GraphicsClient*, float)
{
    // An NSBitmapImageRep, unlike the representation -[NSImage initWithCGImage:size:] makes, reports the image's own pixel size.
    RetainPtr imageRep = adoptNS([[NSBitmapImageRep alloc] initWithCGImage:image.platformImage().get()]);
    RetainPtr dragImage = adoptNS([[NSImage alloc] initWithSize:image.size()]);
    [dragImage addRepresentation:imageRep.get()];
    return dragImage;
}
    
RetainPtr<NSImage> createDragImageIconForCachedImageFilename(const String& filename)
{
    RetainPtr<NSString> extension;
    size_t dotIndex = filename.reverseFind('.');
    
    if (dotIndex != notFound && dotIndex < (filename.length() - 1)) // require that a . exists after the first character and before the last
        extension = StringView(filename).substring(dotIndex + 1).createNSString();
    else {
        // It might be worth doing a further lookup to pull the extension from the MIME type.
        extension = @"";
    }
    
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [[NSWorkspace sharedWorkspace] iconForFileType:extension.get()];
ALLOW_DEPRECATED_DECLARATIONS_END
}

const int linkImageShadowRadius = 0;
const int linkImageShadowOffsetY = 0;
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

DragImageRef createDragImageForColor(const Color& color, const FloatRect&, float, Path&)
{
    auto dragImage = adoptNS([[NSImage alloc] initWithSize:NSMakeSize(ColorSwatchWidth, ColorSwatchWidth)]);

    [dragImage lockFocus];

    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(0, 0, ColorSwatchWidth, ColorSwatchWidth) xRadius:ColorSwatchCornerRadius yRadius:ColorSwatchCornerRadius];
    [path setLineWidth:ColorSwatchStrokeSize];

    [cocoaColor(color) setFill];
    [path fill];

    [[NSColor quaternaryLabelColor] setStroke];
    [path stroke];

    [dragImage unlockFocus];

    return dragImage;
}

} // namespace WebCore

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
