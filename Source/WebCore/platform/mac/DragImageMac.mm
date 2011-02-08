/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#if ENABLE(DRAG_SUPPORT)
#import "CachedImage.h"
#import "Font.h"
#import "FontDescription.h"
#import "FontSelector.h"
#import "Image.h"
#import "KURL.h"
#import "ResourceResponse.h"
#import "Settings.h"
#import "StringTruncator.h"
#import "TextRun.h"
#import "WebCoreTextRenderer.h"

namespace WebCore {

IntSize dragImageSize(RetainPtr<NSImage> image)
{
    return (IntSize)[image.get() size];
}

void deleteDragImage(RetainPtr<NSImage>)
{
    // Since this is a RetainPtr, there's nothing additional we need to do to
    // delete it. It will be released when it falls out of scope.
}

RetainPtr<NSImage> scaleDragImage(RetainPtr<NSImage> image, FloatSize scale)
{
    NSSize originalSize = [image.get() size];
    NSSize newSize = NSMakeSize((originalSize.width * scale.width()), (originalSize.height * scale.height()));
    newSize.width = roundf(newSize.width);
    newSize.height = roundf(newSize.height);
    [image.get() setScalesWhenResized:YES];
    [image.get() setSize:newSize];
    return image;
}
    
RetainPtr<NSImage> dissolveDragImageToFraction(RetainPtr<NSImage> image, float delta)
{
    RetainPtr<NSImage> dissolvedImage(AdoptNS, [[NSImage alloc] initWithSize:[image.get() size]]);
    
    NSPoint point = [image.get() isFlipped] ? NSMakePoint(0, [image.get() size].height) : NSZeroPoint;
    
    // In this case the dragging image is always correct.
    [dissolvedImage.get() setFlipped:[image.get() isFlipped]];
    
    [dissolvedImage.get() lockFocus];
    [image.get() dissolveToPoint:point fraction: delta];
    [dissolvedImage.get() unlockFocus];
    
    [image.get() lockFocus];
    [dissolvedImage.get() compositeToPoint:point operation:NSCompositeCopy];
    [image.get() unlockFocus];
    
    return image;
}
        
RetainPtr<NSImage> createDragImageFromImage(Image* image)
{
    RetainPtr<NSImage> dragImage(AdoptNS, [image->getNSImage() copy]);
    [dragImage.get() setSize:(NSSize)(image->size())];
    return dragImage;
}
    
RetainPtr<NSImage> createDragImageIconForCachedImage(CachedImage* image)
{
    const String& filename = image->response().suggestedFilename();
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


const float DragLabelBorderX = 4;
//Keep border_y in synch with DragController::LinkDragBorderInset
const float DragLabelBorderY = 2;
const float DragLabelRadius = 5;
const float LabelBorderYOffset = 2;

const float MinDragLabelWidthBeforeClip = 120;
const float MaxDragLabelWidth = 320;

const float DragLinkLabelFontsize = 11;
const float DragLinkUrlFontSize = 10;

DragImageRef createDragImageForLink(KURL& url, const String& title, Frame* frame)
{
    if (!frame)
        return nil;
    NSString *label = 0;
    if (!title.isEmpty())
        label = title;
    NSURL *cocoaURL = url;
    NSString *urlString = [cocoaURL _web_userVisibleString];

    BOOL drawURLString = YES;
    BOOL clipURLString = NO;
    BOOL clipLabelString = NO;

    if (!label) {
        drawURLString = NO;
        label = urlString;
    }

    NSFont *labelFont = [[NSFontManager sharedFontManager] convertFont:[NSFont systemFontOfSize:DragLinkLabelFontsize]
                                                           toHaveTrait:NSBoldFontMask];
    NSFont *urlFont = [NSFont systemFontOfSize:DragLinkUrlFontSize];
    NSSize labelSize;
    labelSize.width = [label _web_widthWithFont: labelFont];
    labelSize.height = [labelFont ascender] - [labelFont descender];
    if (labelSize.width > MaxDragLabelWidth){
        labelSize.width = MaxDragLabelWidth;
        clipLabelString = YES;
    }

    NSSize imageSize;
    imageSize.width = labelSize.width + DragLabelBorderX * 2;
    imageSize.height = labelSize.height + DragLabelBorderY * 2;
    if (drawURLString) {
        NSSize urlStringSize;
        urlStringSize.width = [urlString _web_widthWithFont: urlFont];
        urlStringSize.height = [urlFont ascender] - [urlFont descender];
        imageSize.height += urlStringSize.height;
        if (urlStringSize.width > MaxDragLabelWidth) {
            imageSize.width = max(MaxDragLabelWidth + DragLabelBorderY * 2, MinDragLabelWidthBeforeClip);
            clipURLString = YES;
        } else
            imageSize.width = max(labelSize.width + DragLabelBorderX * 2, urlStringSize.width + DragLabelBorderX * 2);
    }
    NSImage *dragImage = [[[NSImage alloc] initWithSize: imageSize] autorelease];
    [dragImage lockFocus];

    [[NSColor colorWithDeviceRed: 0.7f green: 0.7f blue: 0.7f alpha: 0.8f] set];

    // Drag a rectangle with rounded corners
    NSBezierPath *path = [NSBezierPath bezierPath];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0, 0, DragLabelRadius * 2, DragLabelRadius * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0, imageSize.height - DragLabelRadius * 2, DragLabelRadius * 2, DragLabelRadius * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DragLabelRadius * 2, imageSize.height - DragLabelRadius * 2, DragLabelRadius * 2, DragLabelRadius * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DragLabelRadius * 2, 0, DragLabelRadius * 2, DragLabelRadius * 2)];

    [path appendBezierPathWithRect: NSMakeRect(DragLabelRadius, 0, imageSize.width - DragLabelRadius * 2, imageSize.height)];
    [path appendBezierPathWithRect: NSMakeRect(0, DragLabelRadius, DragLabelRadius + 10, imageSize.height - 2 * DragLabelRadius)];
    [path appendBezierPathWithRect: NSMakeRect(imageSize.width - DragLabelRadius - 20, DragLabelRadius, DragLabelRadius + 20, imageSize.height - 2 * DragLabelRadius)];
    [path fill];

    NSColor *topColor = [NSColor colorWithDeviceWhite:0.0f alpha:0.75f];
    NSColor *bottomColor = [NSColor colorWithDeviceWhite:1.0f alpha:0.5f];
    if (drawURLString) {
        if (clipURLString)
            urlString = [WebStringTruncator centerTruncateString: urlString toWidth:imageSize.width - (DragLabelBorderX * 2) withFont:urlFont];

       [urlString _web_drawDoubledAtPoint:NSMakePoint(DragLabelBorderX, DragLabelBorderY - [urlFont descender]) 
                             withTopColor:topColor bottomColor:bottomColor font:urlFont];
    }

    if (clipLabelString)
        label = [WebStringTruncator rightTruncateString: label toWidth:imageSize.width - (DragLabelBorderX * 2) withFont:labelFont];
    [label _web_drawDoubledAtPoint:NSMakePoint (DragLabelBorderX, imageSize.height - LabelBorderYOffset - [labelFont pointSize])
                      withTopColor:topColor bottomColor:bottomColor font:labelFont];

    [dragImage unlockFocus];

    return dragImage;
}
   
} // namespace WebCore

#endif // ENABLE(DRAG_SUPPORT)
