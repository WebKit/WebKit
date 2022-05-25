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
#include <WebCore/PageIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/WeakHashSet.h>

namespace IPC {
class SharedBufferReference;
}

namespace WebCore {
enum class DataOwnerType : uint8_t;
class Color;
class PasteboardCustomData;
class SelectionData;
struct PasteboardImage;
struct PasteboardItemInfo;
struct PasteboardURL;
struct PasteboardWebContent;
}

namespace WebKit {

enum class PasteboardAccessIntent : bool;
class WebFrameProxy;
class WebProcessProxy;

class WebPasteboardProxy : public IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(WebPasteboardProxy);
    friend LazyNeverDestroyed<WebPasteboardProxy>;
public:
    static WebPasteboardProxy& singleton();

    void addWebProcessProxy(WebProcessProxy&);
    void removeWebProcessProxy(WebProcessProxy&);

#if PLATFORM(COCOA)
    void revokeAccess(WebProcessProxy&);
    void grantAccessToCurrentData(WebProcessProxy&, const String& pasteboardName);
    void grantAccessToCurrentTypes(WebProcessProxy&, const String& pasteboardName);
#endif

#if PLATFORM(GTK)
    void setPrimarySelectionOwner(WebFrameProxy*);
    WebFrameProxy* primarySelectionOwner() const { return m_primarySelectionOwner; }
    void didDestroyFrame(WebFrameProxy*);
#endif

private:
    WebPasteboardProxy();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;

    WebProcessProxy* webProcessProxyForConnection(IPC::Connection&) const;

#if PLATFORM(IOS_FAMILY)
    void writeURLToPasteboard(IPC::Connection&, const WebCore::PasteboardURL&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>);
    void writeWebContentToPasteboard(IPC::Connection&, const WebCore::PasteboardWebContent&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>);
    void writeImageToPasteboard(IPC::Connection&, const WebCore::PasteboardImage&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>);
    void writeStringToPasteboard(IPC::Connection&, const String& pasteboardType, const String&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>);
    void updateSupportedTypeIdentifiers(const Vector<String>& identifiers, const String& pasteboardName, std::optional<WebCore::PageIdentifier>);
#endif
#if PLATFORM(COCOA)
    void getNumberOfFiles(IPC::Connection&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(uint64_t)>&&);
    void getPasteboardTypes(IPC::Connection&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(Vector<String>&&)>&&);
    void getPasteboardPathnamesForType(IPC::Connection&, const String& pasteboardName, const String& pasteboardType, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(Vector<String>&& pathnames, Vector<SandboxExtension::Handle>&&)>&&);
    void getPasteboardStringForType(IPC::Connection&, const String& pasteboardName, const String& pasteboardType, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(String&&)>&&);
    void getPasteboardStringsForType(IPC::Connection&, const String& pasteboardName, const String& pasteboardType, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(Vector<String>&&)>&&);
    void getPasteboardBufferForType(IPC::Connection&, const String& pasteboardName, const String& pasteboardType, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(SharedMemory::IPCHandle&&)>&&);
    void getPasteboardChangeCount(IPC::Connection&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(int64_t)>&&);
    void getPasteboardColor(IPC::Connection&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(WebCore::Color&&)>&&);
    void getPasteboardURL(IPC::Connection&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(const String&)>&&);
    void addPasteboardTypes(IPC::Connection&, const String& pasteboardName, const Vector<String>& pasteboardTypes, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(int64_t)>&&);
    void setPasteboardTypes(IPC::Connection&, const String& pasteboardName, const Vector<String>& pasteboardTypes, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(int64_t)>&&);
    void setPasteboardURL(IPC::Connection&, const WebCore::PasteboardURL&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(int64_t)>&&);
    void setPasteboardColor(IPC::Connection&, const String&, const WebCore::Color&, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(int64_t)>&&);
    void setPasteboardStringForType(IPC::Connection&, const String& pasteboardName, const String& pasteboardType, const String&, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(int64_t)>&&);
    void setPasteboardBufferForType(IPC::Connection&, const String& pasteboardName, const String& pasteboardType, const SharedMemory::IPCHandle&, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(int64_t)>&&);
#endif

    void readStringFromPasteboard(IPC::Connection&, size_t index, const String& pasteboardType, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(String&&)>&&);
    void readURLFromPasteboard(IPC::Connection&, size_t index, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(String&& url, String&& title)>&&);
    void readBufferFromPasteboard(IPC::Connection&, std::optional<size_t> index, const String& pasteboardType, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(SharedMemory::IPCHandle&&)>&&);
    void getPasteboardItemsCount(IPC::Connection&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(uint64_t)>&&);
    void informationForItemAtIndex(IPC::Connection&, size_t index, const String& pasteboardName, int64_t changeCount, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(std::optional<WebCore::PasteboardItemInfo>&&)>&&);
    void allPasteboardItemInfo(IPC::Connection&, const String& pasteboardName, int64_t changeCount, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(std::optional<Vector<WebCore::PasteboardItemInfo>>&&)>&&);

    void writeCustomData(IPC::Connection&, const Vector<WebCore::PasteboardCustomData>&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(int64_t)>&&);
    void typesSafeForDOMToReadAndWrite(IPC::Connection&, const String& pasteboardName, const String& origin, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(Vector<String>&&)>&&);
    void containsStringSafeForDOMToReadForType(IPC::Connection&, const String&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(bool)>&&);
    void containsURLStringSuitableForLoading(IPC::Connection&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(bool)>&&);
    void urlStringSuitableForLoading(IPC::Connection&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, CompletionHandler<void(String&& url, String&& title)>&&);

#if PLATFORM(GTK)
    void getTypes(const String& pasteboardName, CompletionHandler<void(Vector<String>&&)>&&);
    void readText(const String& pasteboardName, CompletionHandler<void(String&&)>&&);
    void readFilePaths(const String& pasteboardName, CompletionHandler<void(Vector<String>&&)>&&);
    void readBuffer(const String& pasteboardName, const String& pasteboardType, CompletionHandler<void(IPC::SharedBufferReference&&)>&&);
    void writeToClipboard(const String& pasteboardName, WebCore::SelectionData&&);
    void clearClipboard(const String& pasteboardName);

    WebFrameProxy* m_primarySelectionOwner { nullptr };
#endif // PLATFORM(GTK)

#if USE(LIBWPE)
    void getPasteboardTypes(CompletionHandler<void(Vector<String>&&)>&&);
    void writeWebContentToPasteboard(const WebCore::PasteboardWebContent&);
    void writeStringToPasteboard(const String& pasteboardType, const String&);
#endif

#if PLATFORM(COCOA)
    bool canAccessPasteboardTypes(IPC::Connection&, const String& pasteboardName) const;
    bool canAccessPasteboardData(IPC::Connection&, const String& pasteboardName) const;
    void didModifyContentsOfPasteboard(IPC::Connection&, const String& pasteboardName, int64_t previousChangeCount, int64_t newChangeCount);

    enum class PasteboardAccessType : uint8_t { Types, TypesAndData };
    std::optional<PasteboardAccessType> accessType(IPC::Connection&, const String& pasteboardName) const;
    void grantAccess(WebProcessProxy&, const String& pasteboardName, PasteboardAccessType);

    std::optional<WebCore::DataOwnerType> determineDataOwner(IPC::Connection&, const String& pasteboardName, std::optional<WebCore::PageIdentifier>, PasteboardAccessIntent) const;
#endif

    WeakHashSet<WebProcessProxy> m_webProcessProxySet;

#if PLATFORM(COCOA)
    struct PasteboardAccessInformation {
        int64_t changeCount { 0 };
        Vector<std::pair<WeakPtr<WebProcessProxy>, PasteboardAccessType>> processes;

        void grantAccess(WebProcessProxy&, PasteboardAccessType);
        void revokeAccess(WebProcessProxy&);
        std::optional<PasteboardAccessType> accessType(WebProcessProxy&) const;
    };
    HashMap<String, PasteboardAccessInformation> m_pasteboardNameToAccessInformationMap;
#endif
};

} // namespace WebKit
