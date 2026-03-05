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

    bool NODELETE hasWebView() const final;

    void NODELETE makeRepresentation(DocumentLoader*) final;

#if PLATFORM(IOS_FAMILY)
    bool forceLayoutOnRestoreFromBackForwardCache() final;
#endif

    void NODELETE forceLayoutForNonHTML() final;

    void NODELETE setCopiesOnScroll() final;

    void NODELETE detachedFromParent2() final;
    void NODELETE detachedFromParent3() final;

    void NODELETE convertMainResourceLoadToDownload(DocumentLoader*, const ResourceRequest&, const ResourceResponse&) final;

    void NODELETE assignIdentifierToInitialRequest(ResourceLoaderIdentifier, DocumentLoader*, const ResourceRequest&) final;
    bool NODELETE shouldUseCredentialStorage(DocumentLoader*, ResourceLoaderIdentifier) override;
    void NODELETE dispatchWillSendRequest(DocumentLoader*, ResourceLoaderIdentifier, ResourceRequest&, const ResourceResponse&) final;
    void NODELETE dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, ResourceLoaderIdentifier, const AuthenticationChallenge&) final;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    bool NODELETE canAuthenticateAgainstProtectionSpace(DocumentLoader*, ResourceLoaderIdentifier, const ProtectionSpace&) final;
#endif

#if PLATFORM(IOS_FAMILY)
    RetainPtr<CFDictionaryRef> connectionProperties(DocumentLoader*, ResourceLoaderIdentifier) final;
#endif

    void NODELETE dispatchDidReceiveResponse(DocumentLoader*, ResourceLoaderIdentifier, const ResourceResponse&) final;
    void NODELETE dispatchDidReceiveContentLength(DocumentLoader*, ResourceLoaderIdentifier, int) final;
    void NODELETE dispatchDidFinishLoading(DocumentLoader*, ResourceLoaderIdentifier) final;
#if ENABLE(DATA_DETECTION)
    void NODELETE dispatchDidFinishDataDetection(NSArray *) final;
#endif
    void NODELETE dispatchDidFailLoading(DocumentLoader*, ResourceLoaderIdentifier, const ResourceError&) final;
    bool NODELETE dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int) final;

    void NODELETE dispatchDidDispatchOnloadEvents() final;
    void NODELETE dispatchDidReceiveServerRedirectForProvisionalLoad() final;
    void NODELETE dispatchDidCancelClientRedirect() final;
    void dispatchWillPerformClientRedirect(const URL&, double, WallTime, LockBackForwardList) final;
    void NODELETE dispatchDidChangeLocationWithinPage() final;
    void NODELETE dispatchDidPushStateWithinPage() final;
    void NODELETE dispatchDidReplaceStateWithinPage() final;
    void NODELETE dispatchDidPopStateWithinPage() final;
    void NODELETE dispatchWillClose() final;
    void NODELETE dispatchDidStartProvisionalLoad() final;
    void NODELETE dispatchDidReceiveTitle(const StringWithDirection&) final;
    void NODELETE dispatchDidCommitLoad(std::optional<HasInsecureContent>, std::optional<UsedLegacyTLS>, std::optional<WasPrivateRelayed>) final;
    void NODELETE dispatchDidFailProvisionalLoad(const ResourceError&, WillContinueLoading, WillInternallyHandleFailure) final;
    void NODELETE dispatchDidFailLoad(const ResourceError&) final;
    void NODELETE dispatchDidFinishDocumentLoad() final;
    void NODELETE dispatchDidFinishLoad() final;
    void NODELETE dispatchDidReachLayoutMilestone(OptionSet<LayoutMilestone>) final;
    void NODELETE dispatchDidReachVisuallyNonEmptyState() final;

    LocalFrame* NODELETE dispatchCreatePage(const NavigationAction&, NewFrameOpenerPolicy, const String&) final;
    void NODELETE dispatchShow() final;

    void NODELETE dispatchDecidePolicyForResponse(const ResourceResponse&, const ResourceRequest&, const String&, FramePolicyFunction&&) final;
    void NODELETE dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest&, FormState*, const String&, std::optional<HitTestResult>&&, FramePolicyFunction&&) final;
    void NODELETE dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, const ResourceResponse& redirectResponse, FormState*, const String&, std::optional<NavigationIdentifier>, std::optional<HitTestResult>&&, bool, NavigationUpgradeToHTTPSBehavior, SandboxFlags, PolicyDecisionMode, FramePolicyFunction&&) final;
    void NODELETE updateSandboxFlags(SandboxFlags) final;
    void NODELETE updateOpener(std::optional<FrameIdentifier>) final;
    void setPrinting(bool, FloatSize, FloatSize, float, AdjustViewSize) final;
    void NODELETE cancelPolicyCheck() final;

    void NODELETE dispatchUnableToImplementPolicy(const ResourceError&) final;

    void NODELETE dispatchWillSendSubmitEvent(Ref<FormState>&&) final;
    void dispatchWillSubmitForm(FormState&, URL&& requestURL, String&& method, CompletionHandler<void()>&&) final;

    void NODELETE revertToProvisionalState(DocumentLoader*) final;
    void NODELETE setMainDocumentError(DocumentLoader*, const ResourceError&) final;

    void NODELETE setMainFrameDocumentReady(bool) final;

    void NODELETE startDownload(const ResourceRequest&, const String&, FromDownloadAttribute = FromDownloadAttribute::No) final;

    void NODELETE willChangeTitle(DocumentLoader*) final;
    void NODELETE didChangeTitle(DocumentLoader*) final;

    void NODELETE willReplaceMultipartContent() final;
    void NODELETE didReplaceMultipartContent() final;

    void NODELETE committedLoad(DocumentLoader*, const SharedBuffer&) final;
    void NODELETE finishedLoading(DocumentLoader*) final;

    void NODELETE loadStorageAccessQuirksIfNeeded() final;

    bool NODELETE shouldFallBack(const ResourceError&) const final;

    bool NODELETE canHandleRequest(const ResourceRequest&) const final;
    bool NODELETE canShowMIMEType(const String&) const final;
    bool NODELETE canShowMIMETypeAsHTML(const String&) const final;
    bool NODELETE representationExistsForURLScheme(StringView) const final;
    String NODELETE generatedMIMETypeForURLScheme(StringView) const final;

    void NODELETE frameLoadCompleted() final;
    void NODELETE restoreViewState() final;
    void NODELETE provisionalLoadStarted() final;
    void NODELETE didFinishLoad() final;
    void NODELETE prepareForDataSourceReplacement() final;

    void NODELETE updateCachedDocumentLoader(DocumentLoader&) final;
    void NODELETE setTitle(const StringWithDirection&, const URL&) final;

    String NODELETE userAgent(const URL&) const override;

    void NODELETE savePlatformDataToCachedFrame(CachedFrame*) final;
    void NODELETE transitionToCommittedFromCachedFrame(CachedFrame*) final;
#if PLATFORM(IOS_FAMILY)
    void didRestoreFrameHierarchyForCachedFrame() final;
#endif
    void NODELETE transitionToCommittedForNewPage(InitializingIframe) final;

    void NODELETE didRestoreFromBackForwardCache() final;

    void NODELETE updateGlobalHistory() final;
    void NODELETE updateGlobalHistoryRedirectLinks() final;
    ShouldGoToHistoryItem NODELETE shouldGoToHistoryItem(HistoryItem&, IsSameDocumentNavigation) const final;
    bool NODELETE supportsAsyncShouldGoToHistoryItem() const final;
    void NODELETE shouldGoToHistoryItemAsync(HistoryItem&, CompletionHandler<void(ShouldGoToHistoryItem)>&&) const final;

    void NODELETE saveViewStateToItem(HistoryItem&) final;
    bool NODELETE canCachePage() const final;
    RefPtr<LocalFrame> createFrame(const AtomString&, HTMLFrameOwnerElement&) final;
    RefPtr<Widget> createPlugin(HTMLPlugInElement&, const URL&, const Vector<AtomString>&, const Vector<AtomString>&, const String&, bool) final;

    ObjectContentType NODELETE objectContentType(const URL&, const String&) final;
    AtomString NODELETE overrideMediaType() const final;

    void NODELETE redirectDataToPlugin(Widget&) final;
    void NODELETE dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&) final;

#if PLATFORM(COCOA)
    RemoteAXObjectRef NODELETE accessibilityRemoteObject() final;
    IntPoint NODELETE accessibilityRemoteFrameOffset() final;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void NODELETE setIsolatedTree(Ref<WebCore::AXIsolatedTree>&&) final;
    RefPtr<WebCore::AXIsolatedTree> isolatedTree() const final;
#endif
    void willCacheResponse(DocumentLoader*, ResourceLoaderIdentifier, NSCachedURLResponse *, CompletionHandler<void(NSCachedURLResponse *)>&&) const final;
#endif

    Ref<FrameNetworkingContext> createNetworkingContext() final;

    bool NODELETE isEmptyFrameLoaderClient() const override;
    void NODELETE prefetchDNS(const String&) final;
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&&) final;

#if USE(QUICK_LOOK)
    RefPtr<LegacyPreviewLoaderClient> createPreviewLoaderClient(const String&, const String&) final;
#endif

    bool NODELETE hasFrameSpecificStorageAccess() final;
    void NODELETE revokeFrameSpecificStorageAccess() final;

    void NODELETE dispatchLoadEventToOwnerElementInAnotherProcess() final;

    RefPtr<HistoryItem> createHistoryItemTree(bool, BackForwardItemIdentifier) const final;
};

} // namespace WebCore
