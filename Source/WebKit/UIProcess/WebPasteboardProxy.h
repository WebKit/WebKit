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
#include <WebCore/SharedBuffer.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {
class Color;
class PasteboardCustomData;
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

#if PLATFORM(COCOA)
    void revokeAccessToAllData(WebProcessProxy&);
    void grantAccessToCurrentData(WebProcessProxy&, const String& pasteboardName);
#endif

#if PLATFORM(GTK)
    void setPrimarySelectionOwner(WebFrameProxy*);
    void didDestroyFrame(WebFrameProxy*);
#endif

private:
    WebPasteboardProxy();
    
    using WebProcessProxyList = HashSet<WebProcessProxy*>;

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;

    WebProcessProxy* webProcessProxyForConnection(IPC::Connection&) const;

#if PLATFORM(IOS_FAMILY)
    void writeURLToPasteboard(IPC::Connection&, const WebCore::PasteboardURL&, const String& pasteboardName);
    void writeWebContentToPasteboard(IPC::Connection&, const WebCore::PasteboardWebContent&, const String& pasteboardName);
    void writeImageToPasteboard(IPC::Connection&, const WebCore::PasteboardImage&, const String& pasteboardName);
    void writeStringToPasteboard(IPC::Connection&, const String& pasteboardType, const String&, const String& pasteboardName);
    void updateSupportedTypeIdentifiers(const Vector<String>& identifiers, const String& pasteboardName);
#endif
#if PLATFORM(COCOA)
    void getNumberOfFiles(const String& pasteboardName, CompletionHandler<void(uint64_t)>&&);
    void getPasteboardTypes(const String& pasteboardName, CompletionHandler<void(Vector<String>&&)>&&);
    void getPasteboardPathnamesForType(IPC::Connection&, const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(Vector<String>&& pathnames, SandboxExtension::HandleArray&&)>&&);
    void getPasteboardStringForType(IPC::Connection&, const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(String&&)>&&);
    void getPasteboardStringsForType(IPC::Connection&, const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(Vector<String>&&)>&&);
    void getPasteboardBufferForType(IPC::Connection&, const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(SharedMemory::Handle&&, uint64_t)>&&);
    void getPasteboardChangeCount(const String& pasteboardName, CompletionHandler<void(int64_t)>&&);
    void getPasteboardColor(IPC::Connection&, const String& pasteboardName, CompletionHandler<void(WebCore::Color&&)>&&);
    void getPasteboardURL(IPC::Connection&, const String& pasteboardName, CompletionHandler<void(const String&)>&&);
    void addPasteboardTypes(IPC::Connection&, const String& pasteboardName, const Vector<String>& pasteboardTypes, CompletionHandler<void(int64_t)>&&);
    void setPasteboardTypes(IPC::Connection&, const String& pasteboardName, const Vector<String>& pasteboardTypes, CompletionHandler<void(int64_t)>&&);
    void setPasteboardURL(IPC::Connection&, const WebCore::PasteboardURL&, const String& pasteboardName, CompletionHandler<void(int64_t)>&&);
    void setPasteboardColor(IPC::Connection&, const String&, const WebCore::Color&, CompletionHandler<void(int64_t)>&&);
    void setPasteboardStringForType(IPC::Connection&, const String& pasteboardName, const String& pasteboardType, const String&, CompletionHandler<void(int64_t)>&&);
    void setPasteboardBufferForType(IPC::Connection&, const String& pasteboardName, const String& pasteboardType, const SharedMemory::Handle&, uint64_t size, CompletionHandler<void(int64_t)>&&);
#endif

    void readStringFromPasteboard(IPC::Connection&, size_t index, const String& pasteboardType, const String& pasteboardName, CompletionHandler<void(String&&)>&&);
    void readURLFromPasteboard(IPC::Connection&, size_t index, const String& pasteboardName, CompletionHandler<void(String&& url, String&& title)>&&);
    void readBufferFromPasteboard(IPC::Connection&, size_t index, const String& pasteboardType, const String& pasteboardName, CompletionHandler<void(SharedMemory::Handle&&, uint64_t size)>&&);
    void getPasteboardItemsCount(const String& pasteboardName, CompletionHandler<void(uint64_t)>&&);
    void informationForItemAtIndex(size_t index, const String& pasteboardName, int64_t changeCount, CompletionHandler<void(Optional<WebCore::PasteboardItemInfo>&&)>&&);
    void allPasteboardItemInfo(const String& pasteboardName, int64_t changeCount, CompletionHandler<void(Optional<Vector<WebCore::PasteboardItemInfo>>&&)>&&);

    void writeCustomData(IPC::Connection&, const Vector<WebCore::PasteboardCustomData>&, const String& pasteboardName, CompletionHandler<void(int64_t)>&&);
    void typesSafeForDOMToReadAndWrite(IPC::Connection&, const String& pasteboardName, const String& origin, CompletionHandler<void(Vector<String>&&)>&&);
    void containsStringSafeForDOMToReadForType(const String&, const String& pasteboardName, CompletionHandler<void(bool)>&&);
    void containsURLStringSuitableForLoading(const String& pasteboardName, CompletionHandler<void(bool)>&&);
    void urlStringSuitableForLoading(IPC::Connection&, const String& pasteboardName, CompletionHandler<void(String&& url, String&& title)>&&);

#if PLATFORM(GTK)
    void writeToClipboard(const String& pasteboardName, const WebSelectionData&);
    void readFromClipboard(const String& pasteboardName, CompletionHandler<void(WebSelectionData&&)>&&);

    WebFrameProxy* m_primarySelectionOwner { nullptr };
    WebFrameProxy* m_frameWritingToClipboard { nullptr };
#endif // PLATFORM(GTK)

#if USE(LIBWPE)
    void getPasteboardTypes(CompletionHandler<void(Vector<String>&&)>&&);
    void writeWebContentToPasteboard(const WebCore::PasteboardWebContent&);
    void writeStringToPasteboard(const String& pasteboardType, const String&);
#endif

#if PLATFORM(COCOA)
    bool canAccessPasteboardData(IPC::Connection&, const String& pasteboardName) const;
    void didModifyContentsOfPasteboard(IPC::Connection&, const String& pasteboardName, int64_t previousChangeCount, int64_t newChangeCount);
#endif

    WebProcessProxyList m_webProcessProxyList;

#if PLATFORM(COCOA)
    HashMap<String, std::pair<int64_t, WeakHashSet<WebProcessProxy>>> m_pasteboardNameToChangeCountAndProcessesMap;
#endif
};

} // namespace WebKit
