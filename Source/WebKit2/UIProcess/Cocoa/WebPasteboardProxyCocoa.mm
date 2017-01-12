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
#import "WebProcessProxy.h"

#import <WebCore/Color.h>
#import <WebCore/PlatformPasteboard.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/URL.h>

using namespace WebCore;

namespace WebKit {

void WebPasteboardProxy::getPasteboardTypes(const String& pasteboardName, Vector<String>& pasteboardTypes)
{
    PlatformPasteboard(pasteboardName).getTypes(pasteboardTypes);
}

void WebPasteboardProxy::getPasteboardPathnamesForType(const String& pasteboardName, const String& pasteboardType, Vector<String>& pathnames)
{
    PlatformPasteboard(pasteboardName).getPathnamesForType(pathnames, pasteboardType);
}

void WebPasteboardProxy::getPasteboardStringForType(const String& pasteboardName, const String& pasteboardType, String& string)
{
    string = PlatformPasteboard(pasteboardName).stringForType(pasteboardType);
}

void WebPasteboardProxy::getPasteboardBufferForType(const String& pasteboardName, const String& pasteboardType, SharedMemory::Handle& handle, uint64_t& size)
{
    RefPtr<SharedBuffer> buffer = PlatformPasteboard(pasteboardName).bufferForType(pasteboardType);
    if (!buffer)
        return;
    size = buffer->size();
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

void WebPasteboardProxy::setPasteboardPathnamesForType(IPC::Connection& connection, const String& pasteboardName, const String& pasteboardType, const Vector<String>& pathnames, uint64_t& newChangeCount)
{
    for (auto* webProcessProxy : m_webProcessProxyList) {
        if (webProcessProxy->connection() != &connection)
            continue;
        
        for (const auto& pathname : pathnames) {
            if (!webProcessProxy->checkURLReceivedFromWebProcess(pathname)) {
                connection.markCurrentlyDispatchedMessageAsInvalid();
                newChangeCount = 0;
                return;
            }
        }
        newChangeCount = PlatformPasteboard(pasteboardName).setPathnamesForType(pathnames, pasteboardType);
        return;
    }
    newChangeCount = 0;
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
    RefPtr<SharedBuffer> buffer = SharedBuffer::create(static_cast<unsigned char *>(sharedMemoryBuffer->data()), size);
    newChangeCount = PlatformPasteboard(pasteboardName).setBufferForType(buffer.get(), pasteboardType);
}

#if PLATFORM(IOS)
void WebPasteboardProxy::writeWebContentToPasteboard(const WebCore::PasteboardWebContent& content)
{
    PlatformPasteboard().write(content);
}

void WebPasteboardProxy::writeImageToPasteboard(const WebCore::PasteboardImage& pasteboardImage)
{
    PlatformPasteboard().write(pasteboardImage);
}

void WebPasteboardProxy::writeStringToPasteboard(const String& pasteboardType, const String& text)
{
    PlatformPasteboard().write(pasteboardType, text);
}

void WebPasteboardProxy::readStringFromPasteboard(uint64_t index, const String& pasteboardType, WTF::String& value)
{
    value = PlatformPasteboard().readString(index, pasteboardType);
}

void WebPasteboardProxy::readURLFromPasteboard(uint64_t index, const String& pasteboardType, String& url)
{
    url = PlatformPasteboard().readURL(index, pasteboardType);
}

void WebPasteboardProxy::readBufferFromPasteboard(uint64_t index, const String& pasteboardType, SharedMemory::Handle& handle, uint64_t& size)
{
    RefPtr<SharedBuffer> buffer = PlatformPasteboard().readBuffer(index, pasteboardType);
    if (!buffer)
        return;
    size = buffer->size();
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::allocate(size);
    if (!sharedMemoryBuffer)
        return;
    memcpy(sharedMemoryBuffer->data(), buffer->data(), size);
    sharedMemoryBuffer->createHandle(handle, SharedMemory::Protection::ReadOnly);
}

void WebPasteboardProxy::getPasteboardItemsCount(uint64_t& itemsCount)
{
    itemsCount = PlatformPasteboard().count();
}

#endif

} // namespace WebKit
