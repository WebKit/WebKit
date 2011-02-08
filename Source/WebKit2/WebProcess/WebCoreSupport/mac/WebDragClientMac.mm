/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import "WebDragClient.h"

#import "PasteboardTypes.h"
#import "ShareableBitmap.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import <WebCore/CachedImage.h>
#import <WebCore/DOMPrivate.h>
#import <WebCore/DOMElementInternal.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebCore/RenderImage.h>
#import <WebCore/StringTruncator.h>
#import <wtf/StdLibExtras.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebNSURLExtras.h>

using namespace WebCore;

namespace WebKit {

using namespace WebCore;
   
void WebDragClient::startDrag(DragImageRef dragImage, const IntPoint& at, const IntPoint& eventPos, Clipboard* clipboard, Frame* frame, bool linkDrag)
{
    if (!frame)
        return;
    ASSERT(clipboard);
    
    NSImage *dragNSImage = dragImage.get();
    RefPtr<ShareableBitmap> dragShareableImage = ShareableBitmap::createShareable(IntSize([dragNSImage size]));
    OwnPtr<GraphicsContext> graphicsContext = dragShareableImage->createGraphicsContext();

    [NSGraphicsContext saveGraphicsState];
    NSGraphicsContext* bitmapContext = [NSGraphicsContext graphicsContextWithGraphicsPort:graphicsContext->platformContext() flipped:YES];
    [NSGraphicsContext setCurrentContext: bitmapContext];
    
    [dragNSImage drawInRect:NSMakeRect(0, 0, [dragNSImage size].width , [dragNSImage size].height) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1];
    [NSGraphicsContext restoreGraphicsState];
 
    SharedMemory::Handle handle;
    if (!dragShareableImage->createHandle(handle))
        return;
    IntPoint clientPoint(at);
    m_page->send(Messages::WebPageProxy::SetDragImage(clientPoint, IntSize([dragNSImage size]), handle, linkDrag));
}

static void writeURL(NSPasteboard* pasteboard, NSURL* URL, NSString* title, NSArray* types)
{
    ASSERT(URL);
    
    if (![title length]) {
        title = [[URL path] lastPathComponent];
        if (![title length])
            title = [URL _web_userVisibleString];
    }
    
    if ([types containsObject:NSURLPboardType])
        [URL writeToPasteboard:pasteboard];
    if ([types containsObject:PasteboardTypes::WebURLPboardType])
        [pasteboard setString:[URL _web_originalDataAsString] forType:PasteboardTypes::WebURLPboardType];
    if ([types containsObject:PasteboardTypes::WebURLNamePboardType])
        [pasteboard setString:title forType:PasteboardTypes::WebURLNamePboardType];
    if ([types containsObject:NSStringPboardType])
        [pasteboard setString:[URL _web_userVisibleString] forType:NSStringPboardType];
    if ([types containsObject:PasteboardTypes::WebURLsWithTitlesPboardType]) {
        NSArray* URLs = [NSArray arrayWithObject:URL];
        unsigned count = [URLs count];
        
        if (!count || [pasteboard availableTypeFromArray:[NSArray arrayWithObject:PasteboardTypes::WebURLsWithTitlesPboardType]] == nil)
            return;

        NSArray* titles = [NSArray arrayWithObject:title];
        
        if (count != [titles count])
            titles = nil;
        
        NSMutableArray* URLStrings = [NSMutableArray arrayWithCapacity:count];
        NSMutableArray* titlesOrEmptyStrings = [NSMutableArray arrayWithCapacity:count];
        for (unsigned index = 0; index < count; ++index) {
            [URLStrings addObject:[[URLs objectAtIndex:index] _web_originalDataAsString]];
            [titlesOrEmptyStrings addObject:(titles == nil) ? @"" : [[titles objectAtIndex:index] _webkit_stringByTrimmingWhitespace]];
        }
        
        [pasteboard setPropertyList:[NSArray arrayWithObjects:URLStrings, titlesOrEmptyStrings, nil]
                            forType:PasteboardTypes::WebURLsWithTitlesPboardType];
    }
}
    
static void writeImage(NSPasteboard* pasteboard, NSImage *image, DOMElement* element, NSURL* URL, NSString* title, LegacyWebArchive* archive, NSArray* types)
{
    ASSERT(image || element);
    ASSERT(URL);
    
    writeURL(pasteboard, URL, title, types);
    
    if ([types containsObject:NSTIFFPboardType]) {
        // FIXME: we should add handling of promised types.
        if (image)
            [pasteboard setData:[image TIFFRepresentation] forType:NSTIFFPboardType];
        else if (element)
            [pasteboard setData:[element _imageTIFFRepresentation] forType:NSTIFFPboardType];
    }
    
    if (archive && [types containsObject:PasteboardTypes::WebArchivePboardType])
        [pasteboard setData:[[(NSData *)archive->rawDataRepresentation().get() retain] autorelease] forType:PasteboardTypes::WebArchivePboardType];
}
    
void WebDragClient::declareAndWriteDragImage(NSPasteboard* pasteboard, DOMElement* element, NSURL* URL, NSString* title, WebCore::Frame*)
{
    ASSERT(element);
    ASSERT(pasteboard && pasteboard == [NSPasteboard pasteboardWithName:NSDragPboard]);
    
    NSString *extension = @"";
    if (RenderObject* renderer = core(element)->renderer()) {
        if (renderer->isImage()) {
            if (CachedImage* image = toRenderImage(renderer)->cachedImage()) {
                extension = image->image()->filenameExtension();
                if (![extension length])
                    return;
            }
        }
    }
    
    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::create(core(element));
    NSMutableArray *types = [[NSMutableArray alloc] initWithObjects:NSFilesPromisePboardType, nil];
    [types addObjectsFromArray:(archive) ? PasteboardTypes::forImagesWithArchive() : PasteboardTypes::forImages()];
    [pasteboard declareTypes:types owner:nil];    
    writeImage(pasteboard, nil, element, URL, title, archive.get(), types);
    [types release];
    
    NSArray *extensions = [[NSArray alloc] initWithObjects:extension, nil];
    [pasteboard setPropertyList:extensions forType:NSFilesPromisePboardType];
    [extensions release];
}

} // namespace WebKit
