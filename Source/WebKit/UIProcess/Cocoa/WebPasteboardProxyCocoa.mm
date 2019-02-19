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

void WebPasteboardProxy::getPasteboardTypes(const String& pasteboardName, Vector<String>& pasteboardTypes)
{
    PlatformPasteboard(pasteboardName).getTypes(pasteboardTypes);
}

void WebPasteboardProxy::getPasteboardPathnamesForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType,
    Vector<String>& pathnames, SandboxExtension::HandleArray& sandboxExtensions)
{
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
}

void WebPasteboardProxy::getPasteboardStringForType(const String& pasteboardName, const String& pasteboardType, String& string)
{
    string = PlatformPasteboard(pasteboardName).stringForType(pasteboardType);
}

void WebPasteboardProxy::getPasteboardStringsForType(const String& pasteboardName, const String& pasteboardType, Vector<String>& strings)
{
    strings = PlatformPasteboard(pasteboardName).allStringsForType(pasteboardType);
}

void WebPasteboardProxy::getPasteboardBufferForType(const String& pasteboardName, const String& pasteboardType, SharedMemory::Handle& handle, uint64_t& size)
{
    RefPtr<SharedBuffer> buffer = PlatformPasteboard(pasteboardName).bufferForType(pasteboardType);
    if (!buffer)
        return;
    size = buffer->size();
    if (!size)
        return;
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::allocate(size);
    if (!sharedMemoryBuffer)
        return;
    memcpy(sharedMemoryBuffer->data(), buffer->data(), size);
    sharedMemoryBuffer->createHandle(handle, SharedMemory::Protection::ReadOnly);
}

void WebPasteboardProxy::pasteboardCopy(const String& fromPasteboard, const String& toPasteboard, uint64_t& newChangeCount)
{
    newChangeCount = PlatformPasteboard(toPasteboard).copy(fromPasteboard);
}

void WebPasteboardProxy::getPasteboardChangeCount(const String& pasteboardName, uint64_t& changeCount)
{
    changeCount = PlatformPasteboard(pasteboardName).changeCount();
}

void WebPasteboardProxy::getPasteboardUniqueName(String& pasteboardName)
{
    pasteboardName = PlatformPasteboard::uniqueName();
}

void WebPasteboardProxy::getPasteboardColor(const String& pasteboardName, WebCore::Color& color)
{
    color = PlatformPasteboard(pasteboardName).color();    
}

void WebPasteboardProxy::getPasteboardURL(const String& pasteboardName, WTF::String& urlString)
{
    urlString = PlatformPasteboard(pasteboardName).url().string();
}

void WebPasteboardProxy::addPasteboardTypes(const String& pasteboardName, const Vector<String>& pasteboardTypes, uint64_t& newChangeCount)
{
    newChangeCount = PlatformPasteboard(pasteboardName).addTypes(pasteboardTypes);
}

void WebPasteboardProxy::setPasteboardTypes(const String& pasteboardName, const Vector<String>& pasteboardTypes, uint64_t& newChangeCount)
{
    newChangeCount = PlatformPasteboard(pasteboardName).setTypes(pasteboardTypes);
}

void WebPasteboardProxy::setPasteboardURL(IPC::Connection& connection, const PasteboardURL& pasteboardURL, const String& pasteboardName, uint64_t& newChangeCount)
{
    for (auto* webProcessProxy : m_webProcessProxyList) {
        if (!webProcessProxy->hasConnection(connection))
            continue;

        if (!webProcessProxy->checkURLReceivedFromWebProcess(pasteboardURL.url.string())) {
            newChangeCount = 0;
            return;
        }

        newChangeCount = PlatformPasteboard(pasteboardName).setURL(pasteboardURL);
        return;
    }
    newChangeCount = 0;
}

void WebPasteboardProxy::setPasteboardColor(const String& pasteboardName, const WebCore::Color& color, uint64_t& newChangeCount)
{
    newChangeCount = PlatformPasteboard(pasteboardName).setColor(color);
}

void WebPasteboardProxy::setPasteboardStringForType(const String& pasteboardName, const String& pasteboardType, const String& string, uint64_t& newChangeCount)
{
    newChangeCount = PlatformPasteboard(pasteboardName).setStringForType(string, pasteboardType);
}

void WebPasteboardProxy::setPasteboardBufferForType(const String& pasteboardName, const String& pasteboardType, const SharedMemory::Handle& handle, uint64_t size, uint64_t& newChangeCount)
{
    if (handle.isNull()) {
        newChangeCount = PlatformPasteboard(pasteboardName).setBufferForType(0, pasteboardType);
        return;
    }
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);
    auto buffer = SharedBuffer::create(static_cast<unsigned char *>(sharedMemoryBuffer->data()), size);
    newChangeCount = PlatformPasteboard(pasteboardName).setBufferForType(buffer.ptr(), pasteboardType);
}

void WebPasteboardProxy::getNumberOfFiles(const String& pasteboardName, uint64_t& numberOfFiles)
{
    numberOfFiles = PlatformPasteboard(pasteboardName).numberOfFiles();
}

void WebPasteboardProxy::typesSafeForDOMToReadAndWrite(const String& pasteboardName, const String& origin, Vector<String>& types)
{
    types = PlatformPasteboard(pasteboardName).typesSafeForDOMToReadAndWrite(origin);
}

void WebPasteboardProxy::writeCustomData(const WebCore::PasteboardCustomData& data, const String& pasteboardName, uint64_t& newChangeCount)
{
    newChangeCount = PlatformPasteboard(pasteboardName).write(data);
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

void WebPasteboardProxy::readStringFromPasteboard(uint64_t index, const String& pasteboardType, const String& pasteboardName, WTF::String& value)
{
    value = PlatformPasteboard(pasteboardName).readString(index, pasteboardType);
}

void WebPasteboardProxy::readURLFromPasteboard(uint64_t index, const String& pasteboardName, String& url, String& title)
{
    url = PlatformPasteboard(pasteboardName).readURL(index, title);
}

void WebPasteboardProxy::readBufferFromPasteboard(uint64_t index, const String& pasteboardType, const String& pasteboardName, SharedMemory::Handle& handle, uint64_t& size)
{
    RefPtr<SharedBuffer> buffer = PlatformPasteboard(pasteboardName).readBuffer(index, pasteboardType);
    if (!buffer)
        return;
    size = buffer->size();
    if (!size)
        return;
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::allocate(size);
    if (!sharedMemoryBuffer)
        return;
    memcpy(sharedMemoryBuffer->data(), buffer->data(), size);
    sharedMemoryBuffer->createHandle(handle, SharedMemory::Protection::ReadOnly);
}

void WebPasteboardProxy::getPasteboardItemsCount(const String& pasteboardName, uint64_t& itemsCount)
{
    itemsCount = PlatformPasteboard(pasteboardName).count();
}

void WebPasteboardProxy::allPasteboardItemInfo(const String& pasteboardName, Vector<PasteboardItemInfo>& allInfo)
{
    allInfo = PlatformPasteboard(pasteboardName).allPasteboardItemInfo();
}

void WebPasteboardProxy::informationForItemAtIndex(int index, const String& pasteboardName, PasteboardItemInfo& info)
{
    info = PlatformPasteboard(pasteboardName).informationForItemAtIndex(index);
}

void WebPasteboardProxy::updateSupportedTypeIdentifiers(const Vector<String>& identifiers, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).updateSupportedTypeIdentifiers(identifiers);
}

#endif

} // namespace WebKit
