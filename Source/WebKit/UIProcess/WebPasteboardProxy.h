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

#pragma once

#include "MessageReceiver.h"
#include "SandboxExtension.h"
#include "SharedMemory.h"
#include <wtf/Forward.h>
#include <wtf/HashSet.h>

namespace WebCore {
class Color;
struct PasteboardCustomData;
struct PasteboardImage;
struct PasteboardItemInfo;
struct PasteboardURL;
struct PasteboardWebContent;
}

namespace WebKit {

class WebFrameProxy;
class WebProcessProxy;
struct WebSelectionData;

class WebPasteboardProxy : public IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(WebPasteboardProxy);
    friend LazyNeverDestroyed<WebPasteboardProxy>;
public:
    static WebPasteboardProxy& singleton();

    void addWebProcessProxy(WebProcessProxy&);
    void removeWebProcessProxy(WebProcessProxy&);

#if PLATFORM(GTK)
    void setPrimarySelectionOwner(WebFrameProxy*);
    void didDestroyFrame(WebFrameProxy*);
#endif

private:
    WebPasteboardProxy();
    
    typedef HashSet<WebProcessProxy*> WebProcessProxyList;

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;

#if PLATFORM(IOS_FAMILY)
    void getPasteboardTypesByFidelityForItemAtIndex(uint64_t index, const String& pasteboardName, Vector<String>& types);
    void writeURLToPasteboard(const WebCore::PasteboardURL&, const String& pasteboardName);
    void writeWebContentToPasteboard(const WebCore::PasteboardWebContent&, const String& pasteboardName);
    void writeImageToPasteboard(const WebCore::PasteboardImage&, const String& pasteboardName);
    void writeStringToPasteboard(const String& pasteboardType, const String&, const String& pasteboardName);
    void readStringFromPasteboard(uint64_t index, const String& pasteboardType, const String& pasteboardName, WTF::String&);
    void readURLFromPasteboard(uint64_t index, const String& pasteboardName, String& url, String& title);
    void readBufferFromPasteboard(uint64_t index, const String& pasteboardType, const String& pasteboardName, SharedMemory::Handle&, uint64_t& size);
    void getPasteboardItemsCount(const String& pasteboardName, uint64_t& itemsCount);
    void allPasteboardItemInfo(const String& pasteboardName, Vector<WebCore::PasteboardItemInfo>&);
    void informationForItemAtIndex(int index, const String& pasteboardName, WebCore::PasteboardItemInfo& filename);
    void updateSupportedTypeIdentifiers(const Vector<String>& identifiers, const String& pasteboardName);
#endif
#if PLATFORM(COCOA)
    void getNumberOfFiles(const String& pasteboardName, uint64_t& numberOfFiles);
    void getPasteboardTypes(const String& pasteboardName, Vector<String>& pasteboardTypes);
    void getPasteboardPathnamesForType(IPC::Connection&, const String& pasteboardName, const String& pasteboardType, Vector<String>& pathnames, SandboxExtension::HandleArray&);
    void getPasteboardStringForType(const String& pasteboardName, const String& pasteboardType, String&);
    void getPasteboardStringsForType(const String& pasteboardName, const String& pasteboardType, Vector<String>&);
    void getPasteboardBufferForType(const String& pasteboardName, const String& pasteboardType, SharedMemory::Handle&, uint64_t& size);
    void pasteboardCopy(const String& fromPasteboard, const String& toPasteboard, uint64_t& newChangeCount);
    void getPasteboardChangeCount(const String& pasteboardName, uint64_t& changeCount);
    void getPasteboardUniqueName(String& pasteboardName);
    void getPasteboardColor(const String& pasteboardName, WebCore::Color&);
    void getPasteboardURL(const String& pasteboardName, WTF::String&);
    void addPasteboardTypes(const String& pasteboardName, const Vector<String>& pasteboardTypes, uint64_t& newChangeCount);
    void setPasteboardTypes(const String& pasteboardName, const Vector<String>& pasteboardTypes, uint64_t& newChangeCount);
    void setPasteboardURL(IPC::Connection&, const WebCore::PasteboardURL&, const String& pasteboardName, uint64_t& newChangeCount);
    void setPasteboardColor(const String&, const WebCore::Color&, uint64_t&);
    void setPasteboardStringForType(const String& pasteboardName, const String& pasteboardType, const String&, uint64_t& newChangeCount);
    void setPasteboardBufferForType(const String& pasteboardName, const String& pasteboardType, const SharedMemory::Handle&, uint64_t size, uint64_t& newChangeCount);
#endif

    void writeCustomData(const WebCore::PasteboardCustomData&, const String& pasteboardName, uint64_t& newChangeCount);
    void typesSafeForDOMToReadAndWrite(const String& pasteboardName, const String& origin, Vector<String>& types);

#if PLATFORM(GTK)
    void writeToClipboard(const String& pasteboardName, const WebSelectionData&);
    void readFromClipboard(const String& pasteboardName, WebSelectionData&);

    WebFrameProxy* m_primarySelectionOwner { nullptr };
    WebFrameProxy* m_frameWritingToClipboard { nullptr };
#endif // PLATFORM(GTK)

#if USE(LIBWPE)
    void getPasteboardTypes(Vector<String>& pasteboardTypes);
    void readStringFromPasteboard(uint64_t index, const String& pasteboardType, WTF::String&);
    void writeWebContentToPasteboard(const WebCore::PasteboardWebContent&);
    void writeStringToPasteboard(const String& pasteboardType, const String&);
#endif

    WebProcessProxyList m_webProcessProxyList;
};

} // namespace WebKit
