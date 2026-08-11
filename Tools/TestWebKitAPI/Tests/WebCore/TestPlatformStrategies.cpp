/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TestPlatformStrategies.h"

#include <WebCore/BlobRegistry.h>
#include <WebCore/Color.h>
#include <WebCore/LoaderStrategy.h>
#include <WebCore/MediaStrategy.h>
#include <WebCore/PasteboardItemInfo.h>
#include <WebCore/PasteboardStrategy.h>
#include <WebCore/PlatformStrategies.h>
#include <WebCore/ResourceError.h>
#include <WebCore/SharedBuffer.h>
#include <mutex>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using namespace WebCore;

namespace {

// Every method here is a stub. These are never expected to be called.
class TestLoaderStrategy final : public LoaderStrategy {
public:
    void loadResource(LocalFrame&, CachedResource&, ResourceRequest&&, const ResourceLoaderOptions&, CompletionHandler<void(RefPtr<SubresourceLoader>&&)>&&) final { }
    void loadResourceSynchronously(FrameLoader&, ResourceLoaderIdentifier, const ResourceRequest&, ClientCredentialPolicy, const FetchOptions&, const HTTPHeaderMap&, ResourceError&, ResourceResponse&, Vector<uint8_t>&) final { }
    void pageLoadCompleted(Page&) final { }
    void browsingContextRemoved(LocalFrame&) final { }
    void remove(ResourceLoader*) final { }
    void setDefersLoading(ResourceLoader&, bool) final { }
    void crossOriginRedirectReceived(ResourceLoader*, const URL&) final { }
    void servePendingRequests(ResourceLoadPriority) final { }
    void suspendPendingRequests() final { }
    void resumePendingRequests() final { }
    void startPingLoad(LocalFrame&, ResourceRequest&, const HTTPHeaderMap&, const FetchOptions&, ContentSecurityPolicyImposition, PingLoadCompletionHandler&&) final { }
    void preconnectTo(FrameLoader&, ResourceRequest&&, StoredCredentialsPolicy, ShouldPreconnectAsFirstParty, PreconnectCompletionHandler&&) final { }
    void setCaptureExtraNetworkLoadMetricsEnabled(bool) final { }
    bool isOnLine() const final { return true; }
    void addOnlineStateChangeListener(Function<void(bool)>&&) final { }
    void isResourceLoadFinished(CachedResource&, CompletionHandler<void(bool)>&&) final { }

    ResourceError cancelledError(const ResourceRequest&) const final { return { }; }
    ResourceError blockedError(const ResourceRequest&) const final { return { }; }
    bool isBlockedError(const ResourceError&) const final { return false; }
    ResourceError blockedByContentBlockerError(const ResourceRequest&) const final { return { }; }
    ResourceError cannotShowURLError(const ResourceRequest&) const final { return { }; }
    ResourceError interruptedForPolicyChangeError(const ResourceRequest&) const final { return { }; }
#if ENABLE(CONTENT_FILTERING)
    ResourceError blockedByContentFilterError(const ResourceRequest&) const final { return { }; }
#endif
    ResourceError cannotShowMIMETypeError(const ResourceResponse&) const final { return { }; }
    ResourceError fileDoesNotExistError(const ResourceResponse&) const final { return { }; }
    ResourceError httpsUpgradeRedirectLoopError(const ResourceRequest&) const final { return { }; }
    ResourceError httpNavigationWithHTTPSOnlyError(const ResourceRequest&) const final { return { }; }
    bool isHttpNavigationWithHTTPSOnlyError(const ResourceError&) const final { return false; }
    ResourceError pluginWillHandleLoadError(const ResourceResponse&) const final { return { }; }
};

class TestPasteboardStrategy final : public PasteboardStrategy {
public:
#if PLATFORM(IOS_FAMILY)
    void writeToPasteboard(const PasteboardURL&, const String&, const PasteboardContext*) final { }
    void writeToPasteboard(const PasteboardWebContent&, const String&, const PasteboardContext*) final { }
    void writeToPasteboard(const PasteboardImage&, const String&, const PasteboardContext*) final { }
    void writeToPasteboard(const String&, const String&, const String&, const PasteboardContext*) final { }
    void updateSupportedTypeIdentifiers(const Vector<String>&, const String&, const PasteboardContext*) final { }
#endif
#if PLATFORM(COCOA)
    void getTypes(Vector<String>&, const String&, const PasteboardContext*) final { }
    RefPtr<SharedBuffer> bufferForType(const String&, const String&, const PasteboardContext*) final { return nullptr; }
    void getPathnamesForType(Vector<String>&, const String&, const String&, const PasteboardContext*) final { }
    String stringForType(const String&, const String&, const PasteboardContext*) final { return { }; }
    Vector<String> allStringsForType(const String&, const String&, const PasteboardContext*) final { return { }; }
    int64_t changeCount(const String&, const PasteboardContext*) final { return 0; }
    Color color(const String&, const PasteboardContext*) final { return { }; }
    URL url(const String&, const PasteboardContext*) final { return { }; }
    int getNumberOfFiles(const String&, const PasteboardContext*) final { return 0; }
    int64_t addTypes(const Vector<String>&, const String&, const PasteboardContext*) final { return 0; }
    int64_t setTypes(const Vector<String>&, const String&, const PasteboardContext*) final { return 0; }
    int64_t setBufferForType(SharedBuffer*, const String&, const String&, const PasteboardContext*) final { return 0; }
    int64_t setURL(const PasteboardURL&, const String&, const PasteboardContext*) final { return 0; }
    int64_t setColor(const Color&, const String&, const PasteboardContext*) final { return 0; }
    int64_t setStringForType(const String&, const String&, const String&, const PasteboardContext*) final { return 0; }
    int64_t writeWebArchive(LegacyWebArchive&, const String&) final { return 0; }
    bool containsURLStringSuitableForLoading(const String&, const PasteboardContext*) final { return false; }
    String urlStringSuitableForLoading(const String&, String&, const PasteboardContext*) final { return { }; }
#endif
    String readStringFromPasteboard(size_t, const String&, const String&, const PasteboardContext*) final { return { }; }
    RefPtr<SharedBuffer> readBufferFromPasteboard(std::optional<size_t>, const String&, const String&, const PasteboardContext*) final { return nullptr; }
    URL readURLFromPasteboard(size_t, const String&, String&, const PasteboardContext*) final { return { }; }
    std::optional<PasteboardItemInfo> informationForItemAtIndex(size_t, const String&, int64_t, const PasteboardContext*) final { return std::nullopt; }
    std::optional<Vector<PasteboardItemInfo>> allPasteboardItemInfo(const String&, int64_t, const PasteboardContext*) final { return std::nullopt; }
    int getPasteboardItemsCount(const String&, const PasteboardContext*) final { return 0; }
    Vector<String> typesSafeForDOMToReadAndWrite(const String&, const String&, const PasteboardContext*) final { return { }; }
    int64_t writeCustomData(const Vector<PasteboardCustomData>&, const String&, const PasteboardContext*) final { return 0; }
    bool containsStringSafeForDOMToReadForType(const String&, const String&, const PasteboardContext*) final { return false; }
#if PLATFORM(GTK) || PLATFORM(WPE)
    Vector<String> types(const String&) final { return { }; }
    String readTextFromClipboard(const String&, const String&) final { return { }; }
    Vector<String> readFilePathsFromClipboard(const String&) final { return { }; }
    RefPtr<SharedBuffer> readBufferFromClipboard(const String&, const String&) final { return nullptr; }
    void writeToClipboard(const String&, SelectionData&&) final { }
    void clearClipboard(const String&) final { }
#elif USE(LIBWPE)
    void getTypes(Vector<String>&) final { }
    void writeToPasteboard(const PasteboardWebContent&) final { }
    void writeToPasteboard(const String&, const String&) final { }
#endif
#if PLATFORM(GTK) || PLATFORM(WPE) || PLATFORM(WIN)
    int64_t changeCount(const String&) final { return 0; }
#endif
};

class TestBlobRegistry final : public BlobRegistry {
public:
    void registerInternalFileBlobURL(const URL&, Ref<BlobDataFileReference>&&, const String&, const String&) final { }
    void registerInternalBlobURL(const URL&, Vector<BlobPart>&&, const String&) final { }
    void registerBlobURL(const URL&, const URL&, const PolicyContainer&, const std::optional<SecurityOriginData>&) final { }
    void registerInternalBlobURLOptionallyFileBacked(const URL&, const URL&, RefPtr<BlobDataFileReference>&&, const String&) final { }
    void registerInternalBlobURLForSlice(const URL&, const URL&, long long, long long, const String&) final { }
    void unregisterBlobURL(const URL&, const std::optional<SecurityOriginData>&) final { }
    void registerBlobURLHandle(const URL&, const std::optional<SecurityOriginData>&) final { }
    void unregisterBlobURLHandle(const URL&, const std::optional<SecurityOriginData>&) final { }
    String blobType(const URL&) final { return emptyString(); }
    unsigned long long blobSize(const URL&) final { return 0; }
    void writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>&, CompletionHandler<void(Vector<String>&&)>&&) final { }
};

class TestMediaStrategy final : public MediaStrategy {
public:
#if ENABLE(WEB_AUDIO)
    Ref<AudioDestination> createAudioDestination(const AudioDestinationCreationOptions&) final { RELEASE_ASSERT_NOT_REACHED(); }
#endif
};

class TestPlatformStrategies final : public PlatformStrategies {
private:
    LoaderStrategy* createLoaderStrategy() final { return new TestLoaderStrategy; }
    PasteboardStrategy* createPasteboardStrategy() final { return new TestPasteboardStrategy; }
    MediaStrategy* createMediaStrategy() final { return new TestMediaStrategy; }
    BlobRegistry* createBlobRegistry() final { return new TestBlobRegistry; }
#if ENABLE(DECLARATIVE_WEB_PUSH)
    PushStrategy* createPushStrategy() final { return nullptr; }
#endif
};

} // anonymous namespace

void ensureTestPlatformStrategiesInstalled()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        setPlatformStrategies(new TestPlatformStrategies);
    });
}

} // namespace TestWebKitAPI
