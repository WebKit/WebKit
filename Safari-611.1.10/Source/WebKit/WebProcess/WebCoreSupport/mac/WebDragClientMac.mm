/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#if ENABLE(DRAG_SUPPORT)

#import "PasteboardTypes.h"
#import "ShareableBitmap.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import <WebCore/CachedImage.h>
#import <WebCore/Document.h>
#import <WebCore/DragController.h>
#import <WebCore/Editor.h>
#import <WebCore/Element.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameDestructionObserver.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebCore/LocalCurrentGraphicsContext.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/Page.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/RenderImage.h>
#import <WebCore/StringTruncator.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/NSURLExtras.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

namespace WebKit {
using namespace WebCore;

#if USE(APPKIT)
using DragImage = NSImage *;
#else
using DragImage = CGImageRef;
#endif

static RefPtr<ShareableBitmap> convertDragImageToBitmap(DragImage image, const IntSize& size, Frame& frame)
{
    ShareableBitmap::Configuration bitmapConfiguration;
    bitmapConfiguration.colorSpace.cgColorSpace = screenColorSpace(frame.mainFrame().view());
    auto bitmap = ShareableBitmap::createShareable(size, bitmapConfiguration);
    if (!bitmap)
        return nullptr;

    auto graphicsContext = bitmap->createGraphicsContext();
    if (!graphicsContext)
        return nullptr;

    LocalCurrentGraphicsContext savedContext(*graphicsContext);
#if USE(APPKIT)
    [image drawInRect:NSMakeRect(0, 0, bitmap->size().width(), bitmap->size().height()) fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:nil];
#else
    CGContextDrawImage(graphicsContext->platformContext(), CGRectMake(0, 0, size.width(), size.height()), image);
#endif

    return bitmap;
}

void WebDragClient::startDrag(DragItem dragItem, DataTransfer&, Frame& frame)
{
    auto& image = dragItem.image;

#if USE(APPKIT)
    IntSize bitmapSize([image.get() size]);
#else
    IntSize bitmapSize(CGImageGetWidth(image.get().get()), CGImageGetHeight(image.get().get()));
#endif
    auto bitmap = convertDragImageToBitmap(image.get().get(), bitmapSize, frame);
    ShareableBitmap::Handle handle;
    if (!bitmap || !bitmap->createHandle(handle))
        return;

    m_page->willStartDrag();
    m_page->send(Messages::WebPageProxy::StartDrag(dragItem, handle));
}

void WebDragClient::didConcludeEditDrag()
{
#if PLATFORM(IOS_FAMILY)
    m_page->didConcludeEditDrag();
#endif
}

#if USE(APPKIT)

static WebCore::CachedImage* cachedImage(Element& element)
{
    auto* renderer = element.renderer();
    if (!is<WebCore::RenderImage>(renderer))
        return nullptr;
    WebCore::CachedImage* image = downcast<WebCore::RenderImage>(*renderer).cachedImage();
    if (!image || image->errorOccurred()) 
        return nullptr;
    return image;
}

void WebDragClient::declareAndWriteDragImage(const String& pasteboardName, Element& element, const URL& url, const String& label, Frame*)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    ASSERT(pasteboardName == String(NSDragPboard));
    ALLOW_DEPRECATED_DECLARATIONS_END

    WebCore::CachedImage* image = cachedImage(element);

    String extension;
    if (image) {
        extension = image->image()->filenameExtension();
        if (extension.isEmpty())
            return;
    }

    String title = label;
    if (title.isEmpty()) {
        title = url.lastPathComponent().toString();
        if (title.isEmpty())
            title = WTF::userVisibleString(url);
    }

    auto archive = LegacyWebArchive::create(element);

    NSURLResponse *response = image->response().nsURLResponse();
    
    RefPtr<SharedBuffer> imageBuffer = image->image()->data();
    size_t imageSize = imageBuffer->size();
    SharedMemory::Handle imageHandle;
    
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::allocate(imageBuffer->size());
    if (!sharedMemoryBuffer)
        return;
    memcpy(sharedMemoryBuffer->data(), imageBuffer->data(), imageSize);
    sharedMemoryBuffer->createHandle(imageHandle, SharedMemory::Protection::ReadOnly);
    
    RetainPtr<CFDataRef> data = archive ? archive->rawDataRepresentation() : 0;
    SharedMemory::Handle archiveHandle;
    size_t archiveSize = 0;
    if (data) {
        auto archiveBuffer = SharedBuffer::create((__bridge NSData *)data.get());
        RefPtr<SharedMemory> archiveSharedMemoryBuffer = SharedMemory::allocate(archiveBuffer->size());
        if (!archiveSharedMemoryBuffer)
            return;
        archiveSize = archiveBuffer->size();
        memcpy(archiveSharedMemoryBuffer->data(), archiveBuffer->data(), archiveSize);
        archiveSharedMemoryBuffer->createHandle(archiveHandle, SharedMemory::Protection::ReadOnly);
    }

    String filename = String([response suggestedFilename]);
    if (m_page->isInspectorPage()) {
        String downloadFilename = ResourceResponseBase::sanitizeSuggestedFilename(element.attributeWithoutSynchronization(HTMLNames::filenameAttr));
        if (!downloadFilename.isEmpty())
            filename = downloadFilename;
    }

    m_page->send(Messages::WebPageProxy::SetPromisedDataForImage(pasteboardName, SharedMemory::IPCHandle { WTFMove(imageHandle), imageSize }, filename, extension, title, String([[response URL] absoluteString]), WTF::userVisibleString(url), SharedMemory::IPCHandle { WTFMove(archiveHandle), archiveSize }));
}

#else

void WebDragClient::declareAndWriteDragImage(const String& pasteboardName, Element& element, const URL& url, const String& label, Frame*)
{
    if (auto frame = element.document().frame())
        frame->editor().writeImageToPasteboard(*Pasteboard::createForDragAndDrop(), element, url, label);
}

#endif // USE(APPKIT)

} // namespace WebKit

#endif // ENABLE(DRAG_SUPPORT)
