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

#ifndef WebPasteboardProxy_h
#define WebPasteboardProxy_h

#include "MessageReceiver.h"
#include "SharedMemory.h"
#include <wtf/Forward.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

namespace WebCore {
class Color;
struct PasteboardImage;
struct PasteboardWebContent;
}

namespace WebKit {

class WebProcessProxy;

class WebPasteboardProxy : public IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(WebPasteboardProxy);
    friend class LazyNeverDestroyed<WebPasteboardProxy>;
public:
    static WebPasteboardProxy& singleton();

    void addWebProcessProxy(WebProcessProxy&);

private:
    WebPasteboardProxy();

    void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&) override;

#if PLATFORM(IOS)
    void writeWebContentToPasteboard(const WebCore::PasteboardWebContent&);
    void writeImageToPasteboard(const WebCore::PasteboardImage&);
    void writeStringToPasteboard(const String& pasteboardType, const String&);
    void readStringFromPasteboard(uint64_t index, const String& pasteboardType, WTF::String&);
    void readURLFromPasteboard(uint64_t index, const String& pasteboardType, String&);
    void readBufferFromPasteboard(uint64_t index, const String& pasteboardType, SharedMemory::Handle&, uint64_t& size);
    void getPasteboardItemsCount(uint64_t& itemsCount);
#endif
#if PLATFORM(COCOA)
    void getPasteboardTypes(const String& pasteboardName, Vector<String>& pasteboardTypes);
    void getPasteboardPathnamesForType(const String& pasteboardName, const String& pasteboardType, Vector<String>& pathnames);
    void getPasteboardStringForType(const String& pasteboardName, const String& pasteboardType, String&);
    void getPasteboardBufferForType(const String& pasteboardName, const String& pasteboardType, SharedMemory::Handle&, uint64_t& size);
    void pasteboardCopy(const String& fromPasteboard, const String& toPasteboard, uint64_t& newChangeCount);
    void getPasteboardChangeCount(const String& pasteboardName, uint64_t& changeCount);
    void getPasteboardUniqueName(String& pasteboardName);
    void getPasteboardColor(const String& pasteboardName, WebCore::Color&);
    void getPasteboardURL(const String& pasteboardName, WTF::String&);
    void addPasteboardTypes(const String& pasteboardName, const Vector<String>& pasteboardTypes, uint64_t& newChangeCount);
    void setPasteboardTypes(const String& pasteboardName, const Vector<String>& pasteboardTypes, uint64_t& newChangeCount);
    void setPasteboardPathnamesForType(const String& pasteboardName, const String& pasteboardType, const Vector<String>& pathnames, uint64_t& newChangeCount);
    void setPasteboardStringForType(const String& pasteboardName, const String& pasteboardType, const String&, uint64_t& newChangeCount);
    void setPasteboardBufferForType(const String& pasteboardName, const String& pasteboardType, const SharedMemory::Handle&, uint64_t size, uint64_t& newChangeCount);
#endif
};

} // namespace WebKit

#endif // WebPasteboardProxy_h
