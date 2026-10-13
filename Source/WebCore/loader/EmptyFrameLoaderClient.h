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

    bool LEGACY_NODELETE hasWebView() const final;

    void LEGACY_NODELETE makeRepresentation(DocumentLoader*) final;

#if PLATFORM(IOS_FAMILY)
    bool forceLayoutOnRestoreFromBackForwardCache() final;
#endif

    void LEGACY_NODELETE forceLayoutForNonHTML() final;

    void LEGACY_NODELETE setCopiesOnScroll() final;

    void LEGACY_NODELETE detachedFromParent2() final;
    void LEGACY_NODELETE detachedFromParent3() final;

    void LEGACY_NODELETE convertMainResourceLoadToDownload(DocumentLoader*, const ResourceRequest&, const ResourceResponse&) final;

    void LEGACY_NODELETE assignIdentifierToInitialRequest(ResourceLoaderIdentifier, DocumentLoader*, const ResourceRequest&) final;
    bool LEGACY_NODELETE shouldUseCredentialStorage(DocumentLoader*, ResourceLoaderIdentifier) override;
    void LEGACY_NODELETE dispatchWillSendRequest(DocumentLoader*, ResourceLoaderIdentifier, ResourceRequest&, const ResourceResponse&) final;
    void LEGACY_NODELETE dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, ResourceLoaderIdentifier, const AuthenticationChallenge&) final;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    bool LEGACY_NODELETE canAuthenticateAgainstProtectionSpace(DocumentLoader*, ResourceLoaderIdentifier, const ProtectionSpace&) final;
#endif

#if PLATFORM(IOS_FAMILY)
    RetainPtr<CFDictionaryRef> connectionProperties(DocumentLoader*, ResourceLoaderIdentifier) final;
#endif

    void LEGACY_NODELETE dispatchDidReceiveResponse(DocumentLoader*, ResourceLoaderIdentifier, const ResourceResponse&) final;
    void LEGACY_NODELETE dispatchDidReceiveContentLength(DocumentLoader*, ResourceLoaderIdentifier, int) final;
    void LEGACY_NODELETE dispatchDidFinishLoading(DocumentLoader*, ResourceLoaderIdentifier) final;
#if ENABLE(DATA_DETECTION)
    void LEGACY_NODELETE dispatchDidFinishDataDetection(NSArray *) final;
#endif
    void LEGACY_NODELETE dispatchDidFailLoading(DocumentLoader*, ResourceLoaderIdentifier, const ResourceError&) final;
    bool LEGACY_NODELETE dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int) final;

    void LEGACY_NODELETE dispatchDidDispatchOnloadEvents() final;
    void LEGACY_NODELETE dispatchDidReceiveServerRedirectForProvisionalLoad() final;
    void LEGACY_NODELETE dispatchDidCancelClientRedirect() final;
    void dispatchWillPerformClientRedirect(const URL&, double, WallTime, LockBackForwardList) final;
    void LEGACY_NODELETE dispatchDidChangeLocationWithinPage() final;
    void LEGACY_NODELETE dispatchDidPushStateWithinPage() final;
    void LEGACY_NODELETE dispatchDidReplaceStateWithinPage() final;
    void LEGACY_NODELETE dispatchDidPopStateWithinPage() final;
    void LEGACY_NODELETE dispatchWillClose() final;
    void LEGACY_NODELETE dispatchDidStartProvisionalLoad() final;
    void LEGACY_NODELETE dispatchDidReceiveTitle(const StringWithDirection&) final;
    void LEGACY_NODELETE dispatchDidCommitLoad(const std::optional<BackForwardCacheCommitData>&) final;
    void LEGACY_NODELETE dispatchDidFailProvisionalLoad(const ResourceError&, WillContinueLoading, WillInternallyHandleFailure) final;
    void LEGACY_NODELETE dispatchDidFailLoad(const ResourceError&) final;
    void LEGACY_NODELETE dispatchDidFinishDocumentLoad() final;
    void LEGACY_NODELETE dispatchDidFinishLoad() final;
    void LEGACY_NODELETE dispatchDidReachLayoutMilestone(OptionSet<LayoutMilestone>) final;
    void LEGACY_NODELETE dispatchDidReachVisuallyNonEmptyState() final;

    LocalFrame* LEGACY_NODELETE dispatchCreatePage(const NavigationAction&, NewFrameOpenerPolicy, const String&) final;
    void LEGACY_NODELETE dispatchShow() final;

    void LEGACY_NODELETE dispatchDecidePolicyForResponse(const ResourceResponse&, const ResourceRequest&, const String&, FramePolicyFunction&&) final;
    void LEGACY_NODELETE dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest&, FormState*, const String&, std::optional<HitTestResult>&&, FramePolicyFunction&&) final;
    void LEGACY_NODELETE dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, const ResourceResponse& redirectResponse, FormState*, const String&, std::optional<NavigationIdentifier>, std::optional<HitTestResult>&&, bool, NavigationUpgradeToHTTPSBehavior, SandboxFlags, PolicyDecisionMode, FramePolicyFunction&&) final;
    void LEGACY_NODELETE updateSandboxFlags(SandboxFlags) final;
    void LEGACY_NODELETE updateOpener(std::optional<FrameIdentifier>) final;
    void setPrinting(bool, FloatSize, FloatSize, float, AdjustViewSize) final;
    void LEGACY_NODELETE cancelPolicyCheck() final;

    void LEGACY_NODELETE dispatchUnableToImplementPolicy(const ResourceError&) final;

    void LEGACY_NODELETE dispatchWillSendSubmitEvent(Ref<FormState>&&) final;
    void dispatchWillSubmitForm(FormState&, URL&& requestURL, String&& method, CompletionHandler<void()>&&) final;

    void LEGACY_NODELETE revertToProvisionalState(DocumentLoader*) final;
    void LEGACY_NODELETE setMainDocumentError(DocumentLoader*, const ResourceError&) final;

    void LEGACY_NODELETE setMainFrameDocumentReady(bool) final;

    void LEGACY_NODELETE startDownload(const ResourceRequest&, const String&, FromDownloadAttribute = FromDownloadAttribute::No) final;

    void LEGACY_NODELETE willChangeTitle(DocumentLoader*) final;
    void LEGACY_NODELETE didChangeTitle(DocumentLoader*) final;

    void LEGACY_NODELETE willReplaceMultipartContent() final;
    void LEGACY_NODELETE didReplaceMultipartContent() final;

    void LEGACY_NODELETE committedLoad(DocumentLoader*, const SharedBuffer&) final;
    void LEGACY_NODELETE finishedLoading(DocumentLoader*) final;

    void LEGACY_NODELETE loadStorageAccessQuirksIfNeeded() final;

    bool LEGACY_NODELETE shouldFallBack(const ResourceError&) const final;

    bool LEGACY_NODELETE canHandleRequest(const ResourceRequest&) const final;
    bool LEGACY_NODELETE canShowMIMEType(const String&) const final;
    bool LEGACY_NODELETE canShowMIMETypeAsHTML(const String&) const final;
    bool LEGACY_NODELETE representationExistsForURLScheme(StringView) const final;
    String LEGACY_NODELETE generatedMIMETypeForURLScheme(StringView) const final;

    void LEGACY_NODELETE frameLoadCompleted() final;
    void LEGACY_NODELETE restoreViewState() final;
    void LEGACY_NODELETE provisionalLoadStarted() final;
    void LEGACY_NODELETE didFinishLoad() final;
    void LEGACY_NODELETE prepareForDataSourceReplacement() final;

    void LEGACY_NODELETE updateCachedDocumentLoader(DocumentLoader&) final;
    void LEGACY_NODELETE setTitle(const StringWithDirection&, const URL&) final;

    String LEGACY_NODELETE userAgent(const URL&) const override;

    void LEGACY_NODELETE savePlatformDataToCachedFrame(CachedFrame*) final;
    void LEGACY_NODELETE transitionToCommittedFromCachedFrame(CachedFrame*) final;
#if PLATFORM(IOS_FAMILY)
    void didRestoreFrameHierarchyForCachedFrame() final;
#endif
    void LEGACY_NODELETE transitionToCommittedForNewPage(InitializingIframe) final;

    void LEGACY_NODELETE didRestoreFromBackForwardCache() final;

    void LEGACY_NODELETE updateGlobalHistory() final;
    void LEGACY_NODELETE updateGlobalHistoryRedirectLinks() final;
    ShouldGoToHistoryItem LEGACY_NODELETE shouldGoToHistoryItem(HistoryItem&, IsSameDocumentNavigation) const final;
    bool LEGACY_NODELETE supportsAsyncShouldGoToHistoryItem() const final;
    void LEGACY_NODELETE shouldGoToHistoryItemAsync(HistoryItem&, CompletionHandler<void(ShouldGoToHistoryItem)>&&) const final;
    void LEGACY_NODELETE dispatchGoToBackForwardItemAtIndex(int steps) final;
    void LEGACY_NODELETE dispatchEnqueueHistoryTraversalDelta(int delta) final;

    void LEGACY_NODELETE saveViewStateToItem(HistoryItem&) final;
    bool LEGACY_NODELETE canCachePage() const final;
    RefPtr<LocalFrame> createFrame(const AtomString&, HTMLFrameOwnerElement&) final;
    RefPtr<Widget> createPlugin(HTMLPlugInElement&, const URL&, const Vector<AtomString>&, const Vector<AtomString>&, const String&, bool) final;

    ObjectContentType LEGACY_NODELETE objectContentType(const URL&, const String&) final;
    AtomString LEGACY_NODELETE overrideMediaType() const final;

    void LEGACY_NODELETE redirectDataToPlugin(Widget&) final;
    void LEGACY_NODELETE dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&) final;

#if PLATFORM(COCOA)
    RemoteAXObjectRef LEGACY_NODELETE accessibilityRemoteObject() final;
    IntPoint LEGACY_NODELETE accessibilityRemoteFrameOffset() final;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void LEGACY_NODELETE setIsolatedTree(Ref<WebCore::AXIsolatedTree>&&) final;
    RefPtr<WebCore::AXIsolatedTree> isolatedTree() const final;
#endif
    void willCacheResponse(DocumentLoader*, ResourceLoaderIdentifier, NSCachedURLResponse *, CompletionHandler<void(NSCachedURLResponse *)>&&) const final;
#endif

    Ref<FrameNetworkingContext> createNetworkingContext() final;

    bool LEGACY_NODELETE isEmptyFrameLoaderClient() const override;
    void LEGACY_NODELETE prefetchDNS(const String&) final;
    void sendH2Ping(const URL&, CompletionHandler<void(std::expected<Seconds, ResourceError>&&)>&&) final;

#if USE(QUICK_LOOK)
    RefPtr<LegacyPreviewLoaderClient> createPreviewLoaderClient(const String&, const String&) final;
#endif

    bool LEGACY_NODELETE hasFrameSpecificStorageAccess() final;
    void LEGACY_NODELETE revokeFrameSpecificStorageAccess() final;

    void LEGACY_NODELETE dispatchLoadEventToOwnerElementInAnotherProcess() final;

    RefPtr<HistoryItem> createHistoryItemTree(bool, BackForwardItemIdentifier) const final;
};

} // namespace WebCore
