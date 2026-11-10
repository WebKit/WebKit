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
#include "WebPageProxyIdentifier.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/SharedMemory.h>
#include <wtf/HashMap.h>
#include <wtf/WeakHashSet.h>

namespace IPC {
struct AsyncReplyIDType;
using AsyncReplyID = AtomicObjectIdentifier<AsyncReplyIDType>;
class SharedBufferReference;
}

namespace WebCore {
enum class DataOwnerType : uint8_t;
class Color;
class LegacyWebArchive;
class PasteboardCustomData;
class SelectionData;
struct PasteboardBuffer;
struct PasteboardImage;
struct PasteboardItemInfo;
struct PasteboardURL;
struct PasteboardWebContent;
class SharedBuffer;
}

namespace WebKit {

enum class PasteboardAccessIntent : bool;
enum class WriteWebArchiveToPasteBoardResult : uint8_t;
class WebFrameProxy;
class WebProcessProxy;

class WebPasteboardProxy : public IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(WebPasteboardProxy);
    friend NeverDestroyed<WebPasteboardProxy>;
public:
    NODELETE static WebPasteboardProxy& singleton();

    void addWebProcessProxy(WebProcessProxy&);
    void removeWebProcessProxy(WebProcessProxy&);

    // Do nothing since this is a singleton.
    void ref() const final { }
    void deref() const final { }

#if PLATFORM(COCOA)
    void revokeAccess(WebProcessProxy&);
    std::optional<IPC::AsyncReplyID> grantAccessToCurrentData(WebProcessProxy&, StringImpl* pasteboardName, CompletionHandler<void()>&&);
    void grantAccessToCurrentTypes(WebProcessProxy&, StringImpl* pasteboardName);
#endif

#if PLATFORM(GTK)
    void setPrimarySelectionOwner(WebFrameProxy*);
    WebFrameProxy* primarySelectionOwner() const { return m_primarySelectionOwner; }
    void didDestroyFrame(WebFrameProxy*);
#endif

private:
    WebPasteboardProxy();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;

    NODELETE RefPtr<WebProcessProxy> webProcessProxyForConnection(IPC::Connection&) const;

#if PLATFORM(IOS_FAMILY)
    void writeURLToPasteboard(IPC::Connection&, const WebCore::PasteboardURL&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>);
    void writeWebContentToPasteboard(IPC::Connection&, const WebCore::PasteboardWebContent&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>);
    void writeWebContentToPasteboardInternal(IPC::Connection&, const WebCore::PasteboardWebContent&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>);
    void writeImageToPasteboard(IPC::Connection&, const WebCore::PasteboardImage&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>);
    void writeStringToPasteboard(IPC::Connection&, StringImpl* pasteboardType, StringImpl*, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>);
    void updateSupportedTypeIdentifiers(const Vector<RefPtr<StringImpl>>& identifiers, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>);
#endif
#if PLATFORM(COCOA)
    void getNumberOfFiles(IPC::Connection&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(uint64_t)>&&);
    void getPasteboardTypes(IPC::Connection&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&&);
    void getPasteboardPathnamesForType(IPC::Connection&, StringImpl* pasteboardName, StringImpl* pasteboardType, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(Vector<RefPtr<StringImpl>>&& pathnames, Vector<SandboxExtension::Handle>&&)>&&);
    void getPasteboardStringForType(IPC::Connection&, StringImpl* pasteboardName, StringImpl* pasteboardType, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(RefPtr<StringImpl>&&)>&&);
    void getPasteboardStringsForType(IPC::Connection&, StringImpl* pasteboardName, StringImpl* pasteboardType, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&&);
    void getPasteboardBufferForType(IPC::Connection&, StringImpl* pasteboardName, StringImpl* pasteboardType, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(WebCore::PasteboardBuffer&&)>&&);
    void getPasteboardChangeCount(IPC::Connection&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(int64_t)>&&);
    void getPasteboardColor(IPC::Connection&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(WebCore::Color&&)>&&);
    void getPasteboardURL(IPC::Connection&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(StringImpl*)>&&);
    void addPasteboardTypes(IPC::Connection&, StringImpl* pasteboardName, const Vector<RefPtr<StringImpl>>& pasteboardTypes, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(int64_t)>&&);
    void setPasteboardTypes(IPC::Connection&, StringImpl* pasteboardName, const Vector<RefPtr<StringImpl>>& pasteboardTypes, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(int64_t)>&&);
    void setPasteboardURL(IPC::Connection&, const WebCore::PasteboardURL&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(int64_t)>&&);
    void setPasteboardColor(IPC::Connection&, StringImpl*, const WebCore::Color&, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(int64_t)>&&);
    void setPasteboardStringForType(IPC::Connection&, StringImpl* pasteboardName, StringImpl* pasteboardType, StringImpl*, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(int64_t)>&&);
    void setPasteboardBufferForType(IPC::Connection&, StringImpl* pasteboardName, StringImpl* pasteboardType, RefPtr<WebCore::SharedBuffer>&&, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(int64_t)>&&);
    void writeWebArchiveToPasteBoard(IPC::Connection&, StringImpl* pasteboardName, WebCore::FrameIdentifier, HashMap<WebCore::FrameIdentifier, Ref<WebCore::LegacyWebArchive>>&&, const Vector<WebCore::FrameIdentifier>&, CompletionHandler<void(WriteWebArchiveToPasteBoardResult, int64_t)>&&);
    void createOneWebArchiveFromFrames(WebProcessProxy&, WebCore::FrameIdentifier, HashMap<WebCore::FrameIdentifier, Ref<WebCore::LegacyWebArchive>>&&, const Vector<WebCore::FrameIdentifier>&, CompletionHandler<void(RefPtr<WebCore::LegacyWebArchive>&&)>&&);

#if ENABLE(IPC_TESTING_API)
    void testIPCSharedMemory(IPC::Connection&, StringImpl* pasteboardName, StringImpl* pasteboardType, WebCore::SharedMemory::Handle&&, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(int64_t, RefPtr<StringImpl>)>&&);
#endif

#endif

    void readStringFromPasteboard(IPC::Connection&, uint64_t index, StringImpl* pasteboardType, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(RefPtr<StringImpl>&&)>&&);
    void readURLFromPasteboard(IPC::Connection&, uint64_t index, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(RefPtr<StringImpl>&& url, RefPtr<StringImpl>&& title)>&&);
    void readBufferFromPasteboard(IPC::Connection&, std::optional<uint64_t> index, StringImpl* pasteboardType, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&);
    void getPasteboardItemsCount(IPC::Connection&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(uint64_t)>&&);
    void informationForItemAtIndex(IPC::Connection&, uint64_t index, StringImpl* pasteboardName, int64_t changeCount, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(std::optional<WebCore::PasteboardItemInfo>&&)>&&);
    void allPasteboardItemInfo(IPC::Connection&, StringImpl* pasteboardName, int64_t changeCount, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(std::optional<Vector<WebCore::PasteboardItemInfo>>&&)>&&);

    void writeCustomData(IPC::Connection&, const Vector<WebCore::PasteboardCustomData>&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(int64_t)>&&);
    void typesSafeForDOMToReadAndWrite(IPC::Connection&, StringImpl* pasteboardName, StringImpl* origin, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&&);
    void containsStringSafeForDOMToReadForType(IPC::Connection&, StringImpl*, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(bool)>&&);
    void containsURLStringSuitableForLoading(IPC::Connection&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(bool)>&&);
    void urlStringSuitableForLoading(IPC::Connection&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, CompletionHandler<void(RefPtr<StringImpl>&& url, RefPtr<StringImpl>&& title)>&&);

#if PLATFORM(GTK) || PLATFORM(WPE)
    void getTypes(StringImpl* pasteboardName, CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&&);
    void readText(IPC::Connection&, StringImpl* pasteboardName, StringImpl* pasteboardType, CompletionHandler<void(RefPtr<StringImpl>&&)>&&);
    void readFilePaths(IPC::Connection&, StringImpl* pasteboardName, CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&&);
    void readBuffer(IPC::Connection&, StringImpl* pasteboardName, StringImpl* pasteboardType, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&);
    void writeToClipboard(StringImpl* pasteboardName, WebCore::SelectionData&&);
    void clearClipboard(StringImpl* pasteboardName);
#elif USE(LIBWPE)
    void getPasteboardTypes(CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&&);
    void writeWebContentToPasteboard(const WebCore::PasteboardWebContent&);
    void writeStringToPasteboard(StringImpl* pasteboardType, StringImpl*);
#endif

#if PLATFORM(GTK) || PLATFORM(WPE) || PLATFORM(WIN)
    void getPasteboardChangeCount(IPC::Connection&, StringImpl* pasteboardName, CompletionHandler<void(int64_t)>&&);
#endif

#if PLATFORM(COCOA)
    bool canAccessPasteboardTypes(IPC::Connection&, StringImpl* pasteboardName) const;
    bool canAccessPasteboardData(IPC::Connection&, StringImpl* pasteboardName) const;
    void didModifyContentsOfPasteboard(IPC::Connection&, StringImpl* pasteboardName, int64_t previousChangeCount, int64_t newChangeCount);

    enum class PasteboardAccessType : uint8_t { Types, TypesAndData };
    std::optional<PasteboardAccessType> accessType(IPC::Connection&, StringImpl* pasteboardName) const;
    void grantAccess(WebProcessProxy&, StringImpl* pasteboardName, PasteboardAccessType);

    std::optional<WebCore::DataOwnerType> determineDataOwner(IPC::Connection&, StringImpl* pasteboardName, std::optional<WebPageProxyIdentifier>, PasteboardAccessIntent) const;
#endif

    WeakHashSet<WebProcessProxy> m_webProcessProxySet;

#if PLATFORM(GTK)
    WebFrameProxy* m_primarySelectionOwner { nullptr };
#endif

#if PLATFORM(COCOA)
    struct PasteboardAccessInformation {
        ~PasteboardAccessInformation();

        int64_t changeCount { 0 };
        Vector<std::pair<WeakPtr<WebProcessProxy>, PasteboardAccessType>> processes;

        void grantAccess(WebProcessProxy&, PasteboardAccessType);
        void revokeAccess(WebProcessProxy&);
        std::optional<PasteboardAccessType> accessType(WebProcessProxy&) const;
    };
    HashMap<RefPtr<StringImpl>, PasteboardAccessInformation> m_pasteboardNameToAccessInformationMap;
#endif
};

} // namespace WebKit
