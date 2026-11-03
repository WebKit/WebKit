/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/LocalFrameLoaderClient.h>
#include <wtf/Platform.h>

namespace WebCore {

class WEBCORE_EXPORT EmptyFrameLoaderClient : public LocalFrameLoaderClient {
public:
    explicit EmptyFrameLoaderClient(FrameLoader& frameLoader)
        : LocalFrameLoaderClient(frameLoader)
    { }

private:
    Ref<DocumentLoader> createDocumentLoader(ResourceRequest&&, SubstituteData&&, ResourceRequest&&) override;
    Ref<DocumentLoader> createDocumentLoader(ResourceRequest&&, SubstituteData&&) override;

    NODELETE bool hasWebView() const final;

    NODELETE void makeRepresentation(DocumentLoader*) final;

#if PLATFORM(IOS_FAMILY)
    bool forceLayoutOnRestoreFromBackForwardCache() final;
#endif

    NODELETE void forceLayoutForNonHTML() final;

    NODELETE void setCopiesOnScroll() final;

    NODELETE void detachedFromParent2() final;
    NODELETE void detachedFromParent3() final;

    NODELETE void convertMainResourceLoadToDownload(DocumentLoader*, const ResourceRequest&, const ResourceResponse&) final;

    NODELETE void assignIdentifierToInitialRequest(ResourceLoaderIdentifier, DocumentLoader*, const ResourceRequest&) final;
    NODELETE bool shouldUseCredentialStorage(DocumentLoader*, ResourceLoaderIdentifier) override;
    NODELETE void dispatchWillSendRequest(DocumentLoader*, ResourceLoaderIdentifier, ResourceRequest&, const ResourceResponse&) final;
    NODELETE void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, ResourceLoaderIdentifier, const AuthenticationChallenge&) final;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    NODELETE bool canAuthenticateAgainstProtectionSpace(DocumentLoader*, ResourceLoaderIdentifier, const ProtectionSpace&) final;
#endif

#if PLATFORM(IOS_FAMILY)
    RetainPtr<CFDictionaryRef> connectionProperties(DocumentLoader*, ResourceLoaderIdentifier) final;
#endif

    NODELETE void dispatchDidReceiveResponse(DocumentLoader*, ResourceLoaderIdentifier, const ResourceResponse&) final;
    NODELETE void dispatchDidReceiveContentLength(DocumentLoader*, ResourceLoaderIdentifier, int) final;
    NODELETE void dispatchDidFinishLoading(DocumentLoader*, ResourceLoaderIdentifier) final;
#if ENABLE(DATA_DETECTION)
    NODELETE void dispatchDidFinishDataDetection(NSArray *) final;
#endif
    NODELETE void dispatchDidFailLoading(DocumentLoader*, ResourceLoaderIdentifier, const ResourceError&) final;
    NODELETE bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int) final;

    NODELETE void dispatchDidDispatchOnloadEvents() final;
    NODELETE void dispatchDidReceiveServerRedirectForProvisionalLoad() final;
    NODELETE void dispatchDidCancelClientRedirect() final;
    void dispatchWillPerformClientRedirect(const URL&, double, WallTime, LockBackForwardList) final;
    NODELETE void dispatchDidChangeLocationWithinPage() final;
    NODELETE void dispatchDidPushStateWithinPage() final;
    NODELETE void dispatchDidReplaceStateWithinPage() final;
    NODELETE void dispatchDidPopStateWithinPage() final;
    NODELETE void dispatchWillClose() final;
    NODELETE void dispatchDidStartProvisionalLoad() final;
    NODELETE void dispatchDidReceiveTitle(const StringWithDirection&) final;
    NODELETE void dispatchDidCommitLoad(const std::optional<BackForwardCacheCommitData>&) final;
    NODELETE void dispatchDidFailProvisionalLoad(const ResourceError&, WillContinueLoading, WillInternallyHandleFailure) final;
    NODELETE void dispatchDidFailLoad(const ResourceError&) final;
    NODELETE void dispatchDidFinishDocumentLoad() final;
    NODELETE void dispatchDidFinishLoad() final;
    NODELETE void dispatchDidReachLayoutMilestone(OptionSet<LayoutMilestone>) final;
    NODELETE void dispatchDidReachVisuallyNonEmptyState() final;

    NODELETE LocalFrame* dispatchCreatePage(const NavigationAction&, NewFrameOpenerPolicy, const String&) final;
    NODELETE void dispatchShow() final;

    NODELETE void dispatchDecidePolicyForResponse(const ResourceResponse&, const ResourceRequest&, const String&, FramePolicyFunction&&) final;
    NODELETE void dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest&, FormState*, const String&, std::optional<HitTestResult>&&, FramePolicyFunction&&) final;
    NODELETE void dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, const ResourceResponse& redirectResponse, FormState*, const String&, std::optional<NavigationIdentifier>, std::optional<HitTestResult>&&, bool, NavigationUpgradeToHTTPSBehavior, SandboxFlags, PolicyDecisionMode, FramePolicyFunction&&) final;
    NODELETE void updateSandboxFlags(SandboxFlags) final;
    NODELETE void updateOpener(std::optional<FrameIdentifier>) final;
    void setPrinting(bool, FloatSize, FloatSize, float, AdjustViewSize) final;
    NODELETE void cancelPolicyCheck() final;

    NODELETE void dispatchUnableToImplementPolicy(const ResourceError&) final;

    NODELETE void dispatchWillSendSubmitEvent(Ref<FormState>&&) final;
    void dispatchWillSubmitForm(FormState&, URL&& requestURL, String&& method, CompletionHandler<void()>&&) final;

    NODELETE void revertToProvisionalState(DocumentLoader*) final;
    NODELETE void setMainDocumentError(DocumentLoader*, const ResourceError&) final;

    NODELETE void setMainFrameDocumentReady(bool) final;

    NODELETE void startDownload(const ResourceRequest&, const String&, FromDownloadAttribute = FromDownloadAttribute::No) final;

    NODELETE void willChangeTitle(DocumentLoader*) final;
    NODELETE void didChangeTitle(DocumentLoader*) final;

    NODELETE void willReplaceMultipartContent() final;
    NODELETE void didReplaceMultipartContent() final;

    NODELETE void committedLoad(DocumentLoader*, const SharedBuffer&) final;
    NODELETE void finishedLoading(DocumentLoader*) final;

    NODELETE void loadStorageAccessQuirksIfNeeded() final;

    NODELETE bool shouldFallBack(const ResourceError&) const final;

    NODELETE bool canHandleRequest(const ResourceRequest&) const final;
    NODELETE bool canShowMIMEType(const String&) const final;
    NODELETE bool canShowMIMETypeAsHTML(const String&) const final;
    NODELETE bool representationExistsForURLScheme(StringView) const final;
    NODELETE String generatedMIMETypeForURLScheme(StringView) const final;

    NODELETE void frameLoadCompleted() final;
    NODELETE void restoreViewState() final;
    NODELETE void provisionalLoadStarted() final;
    NODELETE void didFinishLoad() final;
    NODELETE void prepareForDataSourceReplacement() final;

    NODELETE void updateCachedDocumentLoader(DocumentLoader&) final;
    NODELETE void setTitle(const StringWithDirection&, const URL&) final;

    NODELETE String userAgent(const URL&) const override;

    NODELETE void savePlatformDataToCachedFrame(CachedFrame*) final;
    NODELETE void transitionToCommittedFromCachedFrame(CachedFrame*) final;
#if PLATFORM(IOS_FAMILY)
    void didRestoreFrameHierarchyForCachedFrame() final;
#endif
    NODELETE void transitionToCommittedForNewPage(InitializingIframe) final;

    NODELETE void didRestoreFromBackForwardCache() final;

    NODELETE void updateGlobalHistory() final;
    NODELETE void updateGlobalHistoryRedirectLinks() final;
    NODELETE ShouldGoToHistoryItem shouldGoToHistoryItem(HistoryItem&, IsSameDocumentNavigation) const final;
    NODELETE bool supportsAsyncShouldGoToHistoryItem() const final;
    NODELETE void shouldGoToHistoryItemAsync(HistoryItem&, CompletionHandler<void(ShouldGoToHistoryItem)>&&) const final;
    NODELETE void dispatchGoToBackForwardItemAtIndex(int steps) final;
    NODELETE void dispatchEnqueueHistoryTraversalDelta(int delta) final;
    void dispatchEnqueueHistoryTraversalToItem(BackForwardFrameItemIdentifier, CompletionHandler<void(bool)>&&) final;

    NODELETE void saveViewStateToItem(HistoryItem&) final;
    NODELETE bool canCachePage() const final;
    RefPtr<LocalFrame> createFrame(const AtomString&, HTMLFrameOwnerElement&) final;
    RefPtr<Widget> createPlugin(HTMLPlugInElement&, const URL&, const Vector<AtomString>&, const Vector<AtomString>&, const String&, bool) final;

    NODELETE ObjectContentType objectContentType(const URL&, const String&) final;
    NODELETE AtomString overrideMediaType() const final;

    NODELETE void redirectDataToPlugin(Widget&) final;
    NODELETE void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&) final;

#if PLATFORM(COCOA)
    NODELETE RemoteAXObjectRef accessibilityRemoteObject() final;
    NODELETE IntPoint accessibilityRemoteFrameOffset() final;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    NODELETE void setIsolatedTree(Ref<WebCore::AXIsolatedTree>&&) final;
    RefPtr<WebCore::AXIsolatedTree> isolatedTree() const final;
#endif
    void willCacheResponse(DocumentLoader*, ResourceLoaderIdentifier, NSCachedURLResponse *, CompletionHandler<void(NSCachedURLResponse *)>&&) const final;
#endif

    Ref<FrameNetworkingContext> createNetworkingContext() final;

    NODELETE bool isEmptyFrameLoaderClient() const override;
    NODELETE void prefetchDNS(const String&) final;
    void sendH2Ping(const URL&, CompletionHandler<void(std::expected<Seconds, ResourceError>&&)>&&) final;

#if USE(QUICK_LOOK)
    RefPtr<LegacyPreviewLoaderClient> createPreviewLoaderClient(const String&, const String&) final;
#endif

    NODELETE bool hasFrameSpecificStorageAccess() final;
    NODELETE void revokeFrameSpecificStorageAccess() final;

    NODELETE void dispatchLoadEventToOwnerElementInAnotherProcess() final;

    RefPtr<HistoryItem> createHistoryItemTree(bool, BackForwardItemIdentifier) const final;
};

} // namespace WebCore
