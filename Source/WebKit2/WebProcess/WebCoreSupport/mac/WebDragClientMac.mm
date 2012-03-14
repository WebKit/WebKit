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
#import <WebCore/DOMElementInternal.h>
#import <WebCore/DOMPrivate.h>
#import <WebCore/DragController.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebCore/Page.h>
#import <WebCore/RenderImage.h>
#import <WebCore/ResourceHandle.h>
#import <WebCore/StringTruncator.h>
#import <WebKit/WebArchive.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebNSFileManagerExtras.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKitSystemInterface.h>
#import <wtf/StdLibExtras.h>

using namespace WebCore;
using namespace WebKit;

// Internal AppKit class. If the pasteboard handling was in the same process
// that called the dragImage method, this would be created automatically.
// Create it explicitly because dragImage is called in the UI process.
@interface NSFilePromiseDragSource : NSObject
{
    id _dragSource;
    char _unknownFields[256];
}
- (id)initWithSource:(id)dragSource;
- (void)setTypes:(NSArray *)types onPasteboard:(NSPasteboard *)pasteboard;
@end

@interface WKPasteboardFilePromiseOwner : NSFilePromiseDragSource
@end

@interface WKPasteboardOwner : NSObject
{
    CachedResourceHandle<CachedImage> _image;
}
- (id)initWithImage:(CachedImage*)image;
@end

namespace WebKit {

static PassRefPtr<ShareableBitmap> convertImageToBitmap(NSImage *image, const IntSize& size)
{
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(size, ShareableBitmap::SupportsAlpha);
    OwnPtr<GraphicsContext> graphicsContext = bitmap->createGraphicsContext();

    RetainPtr<NSGraphicsContext> savedContext = [NSGraphicsContext currentContext];

    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:graphicsContext->platformContext() flipped:YES]];
    [image drawInRect:NSMakeRect(0, 0, bitmap->size().width(), bitmap->size().height()) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];

    [NSGraphicsContext setCurrentContext:savedContext.get()];

    return bitmap.release();
}

void WebDragClient::startDrag(RetainPtr<NSImage> image, const IntPoint& point, const IntPoint&, Clipboard*, Frame* frame, bool linkDrag)
{
    IntSize bitmapSize([image.get() size]);
    bitmapSize.scale(frame->page()->deviceScaleFactor());
    RefPtr<ShareableBitmap> bitmap = convertImageToBitmap(image.get(), bitmapSize);
    ShareableBitmap::Handle handle;
    if (!bitmap->createHandle(handle))
        return;

    // FIXME: Seems this message should be named StartDrag, not SetDragImage.
    m_page->send(Messages::WebPageProxy::SetDragImage(frame->view()->contentsToWindow(point), handle, linkDrag));
}

static CachedImage* cachedImage(Element* element)
{
    RenderObject* renderer = element->renderer();
    if (!renderer)
        return 0;
    if (!renderer->isRenderImage())
        return 0;
    CachedImage* image = toRenderImage(renderer)->cachedImage();
    if (!image || image->errorOccurred()) 
        return 0;
    return image;
}

static NSArray *arrayForURLsWithTitles(NSURL *URL, NSString *title)
{
    return [NSArray arrayWithObjects:[NSArray arrayWithObject:[URL _web_originalDataAsString]],
        [NSArray arrayWithObject:[title _webkit_stringByTrimmingWhitespace]], nil];
}

void WebDragClient::declareAndWriteDragImage(NSPasteboard *pasteboard, DOMElement *element, NSURL *URL, NSString *title, WebCore::Frame*)
{
    ASSERT(element);
    ASSERT(pasteboard && pasteboard == [NSPasteboard pasteboardWithName:NSDragPboard]);

    Element* coreElement = core(element);

    CachedImage* image = cachedImage(coreElement);

    NSString *extension = @"";
    if (image) {
        extension = image->image()->filenameExtension();
        if (![extension length])
            return;
    }

    if (![title length]) {
        title = [[URL path] lastPathComponent];
        if (![title length])
            title = [URL _web_userVisibleString];
    }

    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::create(coreElement);

    RetainPtr<NSMutableArray> types(AdoptNS, [[NSMutableArray alloc] initWithObjects:NSFilesPromisePboardType, nil]);
    [types.get() addObjectsFromArray:archive ? PasteboardTypes::forImagesWithArchive() : PasteboardTypes::forImages()];

    m_pasteboardOwner.adoptNS([[WKPasteboardOwner alloc] initWithImage:image]);
    m_filePromiseOwner.adoptNS([(WKPasteboardFilePromiseOwner *)[WKPasteboardFilePromiseOwner alloc] initWithSource:m_pasteboardOwner.get()]);

    [pasteboard declareTypes:types.get() owner:m_pasteboardOwner.leakRef()];    

    [pasteboard setPropertyList:[NSArray arrayWithObject:extension] forType:NSFilesPromisePboardType];

    [m_filePromiseOwner.get() setTypes:[pasteboard propertyListForType:NSFilesPromisePboardType] onPasteboard:pasteboard];

    [URL writeToPasteboard:pasteboard];

    [pasteboard setString:[URL _web_originalDataAsString] forType:PasteboardTypes::WebURLPboardType];

    [pasteboard setString:title forType:PasteboardTypes::WebURLNamePboardType];

    [pasteboard setString:[URL _web_userVisibleString] forType:NSStringPboardType];

    [pasteboard setPropertyList:arrayForURLsWithTitles(URL, title) forType:PasteboardTypes::WebURLsWithTitlesPboardType];

    if (archive)
        [pasteboard setData:(NSData *)archive->rawDataRepresentation().get() forType:PasteboardTypes::WebArchivePboardType];
}

void WebDragClient::dragEnded()
{
    // The draggedImage method releases its responder; we must retain the WKPasteboardFilePromiseOwner an extra time to balance the release
    // inside of the function.
    [m_filePromiseOwner.get() retain];

    // The drag source we care about here is NSFilePromiseDragSource, which doesn't look at
    // the arguments. It's OK to just pass arbitrary constant values, so we just pass all zeroes.
    [m_filePromiseOwner.get() draggedImage:nil endedAt:NSZeroPoint operation:NSDragOperationNone];
    
    m_pasteboardOwner = nullptr;
    m_filePromiseOwner = nullptr;
}

} // namespace WebKit

@implementation WKPasteboardFilePromiseOwner

- (id)initWithSource:(id)dragSource
{
    self = [super initWithSource:dragSource];
    if (!self)
        return nil;
    [_dragSource retain];
    return self;
}

- (void)dealloc
{
    [_dragSource release];
    [super dealloc];
}

// The AppKit implementation of copyDropDirectory gets the current pasteboard in
// a way that only works in the process where the drag is initiated. We supply
// an implementation that gets the pasteboard by name instead.
- (CFURLRef)copyDropDirectory
{
    PasteboardRef pasteboard;
    OSStatus status = PasteboardCreate((CFStringRef)NSDragPboard, &pasteboard);
    if (status != noErr || !pasteboard)
        return 0;
    CFURLRef location = 0;
    status = PasteboardCopyPasteLocation(pasteboard, &location);
    CFRelease(pasteboard);
    if (status != noErr || !location)
        return 0;
    CFMakeCollectable(location);
    return location;
}

@end

@implementation WKPasteboardOwner

static CachedResourceClient* promisedDataClient()
{
    static CachedResourceClient* client = new CachedResourceClient;
    return client;
}

- (void)clearImage
{
    if (!_image)
        return;
    _image->removeClient(promisedDataClient());
    _image = 0;
}

- (id)initWithImage:(CachedImage*)image
{
    self = [super init];
    if (!self)
        return nil;

    _image = image;
    if (image)
        image->addClient(promisedDataClient());
    return self;
}

- (void)dealloc
{
    [self clearImage];
    [super dealloc];
}

- (void)finalize
{
    [self clearImage];
    [super finalize];
}

- (void)pasteboard:(NSPasteboard *)pasteboard provideDataForType:(NSString *)type
{
    if ([type isEqual:NSTIFFPboardType]) {
        if (_image) {
            if (Image* image = _image->image())
                [pasteboard setData:(NSData *)image->getTIFFRepresentation() forType:NSTIFFPboardType];
            [self clearImage];
        }
        return;
    }
    // FIXME: Handle RTFD here.
}

- (void)pasteboardChangedOwner:(NSPasteboard *)pasteboard
{
    [self clearImage];
    CFRelease(self); // Balanced by the leakRef that WebDragClient::declareAndWriteDragImage does when making this pasteboard owner.
}

static bool matchesExtensionOrEquivalent(NSString *filename, NSString *extension)
{
    NSString *extensionAsSuffix = [@"." stringByAppendingString:extension];
    return [filename _webkit_hasCaseInsensitiveSuffix:extensionAsSuffix]
        || ([extension _webkit_isCaseInsensitiveEqualToString:@"jpeg"]
            && [filename _webkit_hasCaseInsensitiveSuffix:@".jpg"]);
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    NSFileWrapper *wrapper = nil;
    NSURL *draggingImageURL = nil;
    
    if (_image) {
        if (SharedBuffer* buffer = _image->CachedResource::data()) {
            NSData *data = buffer->createNSData();
            NSURLResponse *response = _image->response().nsURLResponse();
            draggingImageURL = [response URL];
            wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:data] autorelease];
            NSString* filename = [response suggestedFilename];
            NSString* trueExtension(_image->image()->filenameExtension());
            if (!matchesExtensionOrEquivalent(filename, trueExtension))
                filename = [[filename stringByAppendingString:@"."] stringByAppendingString:trueExtension];
            [wrapper setPreferredFilename:filename];
        }
    }

    // FIXME: Do we need to handle the case where we do not have a CachedImage?
    // WebKit1 had code for this case.
    
    if (!wrapper) {
        LOG_ERROR("Failed to create image file.");
        return nil;
    }

    // FIXME: Report an error if we fail to create a file.
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:[wrapper preferredFilename]];
    path = [[NSFileManager defaultManager] _webkit_pathWithUniqueFilenameForPath:path];
    if (![wrapper writeToFile:path atomically:NO updateFilenames:YES])
        LOG_ERROR("Failed to create image file via -[NSFileWrapper writeToFile:atomically:updateFilenames:] at path %@", path);

    if (draggingImageURL)
        [[NSFileManager defaultManager] _webkit_setMetadataURL:[draggingImageURL absoluteString] referrer:nil atPath:path];

    return [NSArray arrayWithObject:[path lastPathComponent]];
}

@end
