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

#include "FrameLoaderClient.h"
#include "ResourceError.h"

#if USE(QUICK_LOOK)
#include "LegacyPreviewLoaderClient.h"
#endif

namespace WebCore {

class WEBCORE_EXPORT EmptyFrameLoaderClient : public FrameLoaderClient {
    Ref<DocumentLoader> createDocumentLoader(const ResourceRequest&, const SubstituteData&) override;

    Optional<FrameIdentifier> frameID() const override { return WTF::nullopt; }
    Optional<PageIdentifier> pageID() const override { return WTF::nullopt; }

    bool hasWebView() const final { return true; } // mainly for assertions

    void makeRepresentation(DocumentLoader*) final { }
#if PLATFORM(IOS_FAMILY)
    bool forceLayoutOnRestoreFromBackForwardCache() final { return false; }
#endif
    void forceLayoutForNonHTML() final { }

    void setCopiesOnScroll() final { }

    void detachedFromParent2() final { }
    void detachedFromParent3() final { }

    void convertMainResourceLoadToDownload(DocumentLoader*, const ResourceRequest&, const ResourceResponse&) final { }

    void assignIdentifierToInitialRequest(unsigned long, DocumentLoader*, const ResourceRequest&) final { }
    bool shouldUseCredentialStorage(DocumentLoader*, unsigned long) override { return false; }
    void dispatchWillSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, const ResourceResponse&) final { }
    void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&) final { }
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    bool canAuthenticateAgainstProtectionSpace(DocumentLoader*, unsigned long, const ProtectionSpace&) final { return false; }
#endif

#if PLATFORM(IOS_FAMILY)
    RetainPtr<CFDictionaryRef> connectionProperties(DocumentLoader*, unsigned long) final { return nullptr; }
#endif

    void dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse&) final { }
    void dispatchDidReceiveContentLength(DocumentLoader*, unsigned long, int) final { }
    void dispatchDidFinishLoading(DocumentLoader*, unsigned long) final { }
#if ENABLE(DATA_DETECTION)
    void dispatchDidFinishDataDetection(NSArray *) final { }
#endif
    void dispatchDidFailLoading(DocumentLoader*, unsigned long, const ResourceError&) final { }
    bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int) final { return false; }

    void dispatchDidDispatchOnloadEvents() final { }
    void dispatchDidReceiveServerRedirectForProvisionalLoad() final { }
    void dispatchDidCancelClientRedirect() final { }
    void dispatchWillPerformClientRedirect(const URL&, double, WallTime, LockBackForwardList) final { }
    void dispatchDidChangeLocationWithinPage() final { }
    void dispatchDidPushStateWithinPage() final { }
    void dispatchDidReplaceStateWithinPage() final { }
    void dispatchDidPopStateWithinPage() final { }
    void dispatchWillClose() final { }
    void dispatchDidStartProvisionalLoad() final { }
    void dispatchDidReceiveTitle(const StringWithDirection&) final { }
    void dispatchDidCommitLoad(Optional<HasInsecureContent>, Optional<UsedLegacyTLS>) final { }
    void dispatchDidFailProvisionalLoad(const ResourceError&, WillContinueLoading) final { }
    void dispatchDidFailLoad(const ResourceError&) final { }
    void dispatchDidFinishDocumentLoad() final { }
    void dispatchDidFinishLoad() final { }
    void dispatchDidReachLayoutMilestone(OptionSet<LayoutMilestone>) final { }
    void dispatchDidReachVisuallyNonEmptyState() final { }

    Frame* dispatchCreatePage(const NavigationAction&) final { return nullptr; }
    void dispatchShow() final { }

    void dispatchDecidePolicyForResponse(const ResourceResponse&, const ResourceRequest&, PolicyCheckIdentifier, const String&, FramePolicyFunction&&) final { }
    void dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest&, FormState*, const String&, PolicyCheckIdentifier, FramePolicyFunction&&) final;
    void dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, const ResourceResponse& redirectResponse, FormState*, PolicyDecisionMode, PolicyCheckIdentifier, FramePolicyFunction&&) final;
    void cancelPolicyCheck() final { }

    void dispatchUnableToImplementPolicy(const ResourceError&) final { }

    void dispatchWillSendSubmitEvent(Ref<FormState>&&) final;
    void dispatchWillSubmitForm(FormState&, CompletionHandler<void()>&&) final;

    void revertToProvisionalState(DocumentLoader*) final { }
    void setMainDocumentError(DocumentLoader*, const ResourceError&) final { }

    void setMainFrameDocumentReady(bool) final { }

    void startDownload(const ResourceRequest&, const String&) final { }

    void willChangeTitle(DocumentLoader*) final { }
    void didChangeTitle(DocumentLoader*) final { }

    void willReplaceMultipartContent() final { }
    void didReplaceMultipartContent() final { }

    void committedLoad(DocumentLoader*, const char*, int) final { }
    void finishedLoading(DocumentLoader*) final { }

    ResourceError cancelledError(const ResourceRequest&) const final { return { ResourceError::Type::Cancellation }; }
    ResourceError blockedError(const ResourceRequest&) const final { return { }; }
    ResourceError blockedByContentBlockerError(const ResourceRequest&) const final { return { }; }
    ResourceError cannotShowURLError(const ResourceRequest&) const final { return { }; }
    ResourceError interruptedForPolicyChangeError(const ResourceRequest&) const final { return { }; }
#if ENABLE(CONTENT_FILTERING)
    ResourceError blockedByContentFilterError(const ResourceRequest&) const final { return { }; }
#endif

    ResourceError cannotShowMIMETypeError(const ResourceResponse&) const final { return { }; }
    ResourceError fileDoesNotExistError(const ResourceResponse&) const final { return { }; }
    ResourceError pluginWillHandleLoadError(const ResourceResponse&) const final { return { }; }

    bool shouldFallBack(const ResourceError&) const final { return false; }

    bool canHandleRequest(const ResourceRequest&) const final { return false; }
    bool canShowMIMEType(const String&) const final { return false; }
    bool canShowMIMETypeAsHTML(const String&) const final { return false; }
    bool representationExistsForURLScheme(const String&) const final { return false; }
    String generatedMIMETypeForURLScheme(const String&) const final { return emptyString(); }

    void frameLoadCompleted() final { }
    void restoreViewState() final { }
    void provisionalLoadStarted() final { }
    void didFinishLoad() final { }
    void prepareForDataSourceReplacement() final { }

    void updateCachedDocumentLoader(DocumentLoader&) final { }
    void setTitle(const StringWithDirection&, const URL&) final { }

    String userAgent(const URL&) const override { return emptyString(); }

    void savePlatformDataToCachedFrame(CachedFrame*) final { }
    void transitionToCommittedFromCachedFrame(CachedFrame*) final { }
#if PLATFORM(IOS_FAMILY)
    void didRestoreFrameHierarchyForCachedFrame() final { }
#endif
    void transitionToCommittedForNewPage() final { }

    void didRestoreFromBackForwardCache() final { }

    void updateGlobalHistory() final { }
    void updateGlobalHistoryRedirectLinks() final { }
    bool shouldGoToHistoryItem(HistoryItem&) const final { return false; }
    void saveViewStateToItem(HistoryItem&) final { }
    bool canCachePage() const final { return false; }
    void didDisplayInsecureContent() final { }
    void didRunInsecureContent(SecurityOrigin&, const URL&) final { }
    void didDetectXSS(const URL&, bool) final { }
    RefPtr<Frame> createFrame(const String&, HTMLFrameOwnerElement&) final;
    RefPtr<Widget> createPlugin(const IntSize&, HTMLPlugInElement&, const URL&, const Vector<String>&, const Vector<String>&, const String&, bool) final;
    RefPtr<Widget> createJavaAppletWidget(const IntSize&, HTMLAppletElement&, const URL&, const Vector<String>&, const Vector<String>&) final;

    ObjectContentType objectContentType(const URL&, const String&) final { return ObjectContentType::None; }
    String overrideMediaType() const final { return { }; }

    void redirectDataToPlugin(Widget&) final { }
    void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&) final { }

#if PLATFORM(COCOA)
    RemoteAXObjectRef accessibilityRemoteObject() final { return nullptr; }
    void willCacheResponse(DocumentLoader*, unsigned long, NSCachedURLResponse *response, CompletionHandler<void(NSCachedURLResponse *)>&& completionHandler) const final { completionHandler(response); }
#endif

#if USE(CFURLCONNECTION)
    bool shouldCacheResponse(DocumentLoader*, unsigned long, const ResourceResponse&, const unsigned char*, unsigned long long) final { return true; }
#endif

    Ref<FrameNetworkingContext> createNetworkingContext() final;

    bool isEmptyFrameLoaderClient() const final { return true; }
    void prefetchDNS(const String&) final { }
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&&) final;

#if USE(QUICK_LOOK)
    RefPtr<LegacyPreviewLoaderClient> createPreviewLoaderClient(const String&, const String&) final { return nullptr; }
#endif
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    bool hasFrameSpecificStorageAccess() final { return false; }
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::EmptyFrameLoaderClient)
    static bool isType(const WebCore::FrameLoaderClient& frameLoaderClient) { return frameLoaderClient.isEmptyFrameLoaderClient(); }
SPECIALIZE_TYPE_TRAITS_END()
