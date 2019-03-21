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
#import "WebPasteboardProxy.h"

#import "SandboxExtension.h"
#import "WebProcessProxy.h"
#import <WebCore/Color.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PasteboardItemInfo.h>
#import <WebCore/PlatformPasteboard.h>
#import <WebCore/SharedBuffer.h>
#import <wtf/URL.h>

namespace WebKit {
using namespace WebCore;

void WebPasteboardProxy::getPasteboardTypes(const String& pasteboardName, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    Vector<String> pasteboardTypes;
    PlatformPasteboard(pasteboardName).getTypes(pasteboardTypes);
    completionHandler(WTFMove(pasteboardTypes));
}

void WebPasteboardProxy::getPasteboardPathnamesForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType,
    CompletionHandler<void(Vector<String>&& pathnames, SandboxExtension::HandleArray&& sandboxExtensions)>&& completionHandler)
{
    Vector<String> pathnames;
    SandboxExtension::HandleArray sandboxExtensions;
    for (auto* webProcessProxy : m_webProcessProxyList) {
        if (!webProcessProxy->hasConnection(connection))
            continue;

        PlatformPasteboard(pasteboardName).getPathnamesForType(pathnames, pasteboardType);

#if PLATFORM(MAC)
        // On iOS, files are copied into app's container upon paste.
        sandboxExtensions.allocate(pathnames.size());
        for (size_t i = 0; i < pathnames.size(); i++) {
            auto& filename = pathnames[i];
            if (![[NSFileManager defaultManager] fileExistsAtPath:filename])
                continue;
            SandboxExtension::createHandle(filename, SandboxExtension::Type::ReadOnly, sandboxExtensions[i]);
        }
#endif
    }
    completionHandler(WTFMove(pathnames), WTFMove(sandboxExtensions));
}

void WebPasteboardProxy::getPasteboardStringForType(const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(String&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).stringForType(pasteboardType));
}

void WebPasteboardProxy::getPasteboardStringsForType(const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).allStringsForType(pasteboardType));
}

void WebPasteboardProxy::getPasteboardBufferForType(const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(SharedMemory::Handle&&, uint64_t)>&& completionHandler)
{
    RefPtr<SharedBuffer> buffer = PlatformPasteboard(pasteboardName).bufferForType(pasteboardType);
    if (!buffer)
        return completionHandler({ }, 0);
    uint64_t size = buffer->size();
    if (!size)
        return completionHandler({ }, 0);
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::allocate(size);
    if (!sharedMemoryBuffer)
        return completionHandler({ }, 0);
    memcpy(sharedMemoryBuffer->data(), buffer->data(), size);
    SharedMemory::Handle handle;
    sharedMemoryBuffer->createHandle(handle, SharedMemory::Protection::ReadOnly);
    completionHandler(WTFMove(handle), size);
}

void WebPasteboardProxy::pasteboardCopy(const String& fromPasteboard, const String& toPasteboard, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(toPasteboard).copy(fromPasteboard));
}

void WebPasteboardProxy::getPasteboardChangeCount(const String& pasteboardName, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).changeCount());
}

void WebPasteboardProxy::getPasteboardUniqueName(CompletionHandler<void(String&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard::uniqueName());
}

void WebPasteboardProxy::getPasteboardColor(const String& pasteboardName, CompletionHandler<void(WebCore::Color&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).color());
}

void WebPasteboardProxy::getPasteboardURL(const String& pasteboardName, CompletionHandler<void(const String&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).url().string());
}

void WebPasteboardProxy::addPasteboardTypes(const String& pasteboardName, const Vector<String>& pasteboardTypes, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).addTypes(pasteboardTypes));
}

void WebPasteboardProxy::setPasteboardTypes(const String& pasteboardName, const Vector<String>& pasteboardTypes, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).setTypes(pasteboardTypes));
}

void WebPasteboardProxy::setPasteboardURL(IPC::Connection& connection, const PasteboardURL& pasteboardURL, const String& pasteboardName, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    for (auto* webProcessProxy : m_webProcessProxyList) {
        if (!webProcessProxy->hasConnection(connection))
            continue;

        if (!webProcessProxy->checkURLReceivedFromWebProcess(pasteboardURL.url.string()))
            return completionHandler(0);

        return completionHandler(PlatformPasteboard(pasteboardName).setURL(pasteboardURL));
    }
    completionHandler(0);
}

void WebPasteboardProxy::setPasteboardColor(const String& pasteboardName, const WebCore::Color& color, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).setColor(color));
}

void WebPasteboardProxy::setPasteboardStringForType(const String& pasteboardName, const String& pasteboardType, const String& string, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).setStringForType(string, pasteboardType));
}

void WebPasteboardProxy::setPasteboardBufferForType(const String& pasteboardName, const String& pasteboardType, const SharedMemory::Handle& handle, uint64_t size, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    if (handle.isNull())
        return completionHandler(PlatformPasteboard(pasteboardName).setBufferForType(0, pasteboardType));
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);
    auto buffer = SharedBuffer::create(static_cast<unsigned char *>(sharedMemoryBuffer->data()), size);
    completionHandler(PlatformPasteboard(pasteboardName).setBufferForType(buffer.ptr(), pasteboardType));
}

void WebPasteboardProxy::getNumberOfFiles(const String& pasteboardName, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).numberOfFiles());
}

void WebPasteboardProxy::typesSafeForDOMToReadAndWrite(const String& pasteboardName, const String& origin, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).typesSafeForDOMToReadAndWrite(origin));
}

void WebPasteboardProxy::writeCustomData(const WebCore::PasteboardCustomData& data, const String& pasteboardName, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).write(data));
}

#if PLATFORM(IOS_FAMILY)

void WebPasteboardProxy::writeURLToPasteboard(const PasteboardURL& url, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).write(url);
}

void WebPasteboardProxy::writeWebContentToPasteboard(const WebCore::PasteboardWebContent& content, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).write(content);
}

void WebPasteboardProxy::writeImageToPasteboard(const WebCore::PasteboardImage& pasteboardImage, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).write(pasteboardImage);
}

void WebPasteboardProxy::writeStringToPasteboard(const String& pasteboardType, const String& text, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).write(pasteboardType, text);
}

void WebPasteboardProxy::readStringFromPasteboard(uint64_t index, const String& pasteboardType, const String& pasteboardName, CompletionHandler<void(String&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).readString(index, pasteboardType));
}

void WebPasteboardProxy::readURLFromPasteboard(uint64_t index, const String& pasteboardName, CompletionHandler<void(String&& url, String&& title)>&& completionHandler)
{
    String title;
    String url = PlatformPasteboard(pasteboardName).readURL(index, title);
    completionHandler(WTFMove(url), WTFMove(title));
}

void WebPasteboardProxy::readBufferFromPasteboard(uint64_t index, const String& pasteboardType, const String& pasteboardName, CompletionHandler<void(SharedMemory::Handle&&, uint64_t size)>&& completionHandler)
{
    RefPtr<SharedBuffer> buffer = PlatformPasteboard(pasteboardName).readBuffer(index, pasteboardType);
    if (!buffer)
        return completionHandler({ }, 0);
    uint64_t size = buffer->size();
    if (!size)
        return completionHandler({ }, 0);
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::allocate(size);
    if (!sharedMemoryBuffer)
        return completionHandler({ }, 0);
    memcpy(sharedMemoryBuffer->data(), buffer->data(), size);
    SharedMemory::Handle handle;
    sharedMemoryBuffer->createHandle(handle, SharedMemory::Protection::ReadOnly);
    completionHandler(WTFMove(handle), size);
}

void WebPasteboardProxy::getPasteboardItemsCount(const String& pasteboardName, CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).count());
}

void WebPasteboardProxy::allPasteboardItemInfo(const String& pasteboardName, CompletionHandler<void(Vector<PasteboardItemInfo>&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).allPasteboardItemInfo());
}

void WebPasteboardProxy::informationForItemAtIndex(int index, const String& pasteboardName, CompletionHandler<void(PasteboardItemInfo&&)>&& completionHandler)
{
    completionHandler(PlatformPasteboard(pasteboardName).informationForItemAtIndex(index));
}

void WebPasteboardProxy::updateSupportedTypeIdentifiers(const Vector<String>& identifiers, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).updateSupportedTypeIdentifiers(identifiers);
}

#endif

} // namespace WebKit
