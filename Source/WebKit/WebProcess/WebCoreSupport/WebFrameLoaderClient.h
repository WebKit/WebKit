/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "SameDocumentNavigationType.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/FrameLoaderClient.h>
#include <pal/SessionID.h>

namespace WebKit {

class PluginView;
class WebFrame;
struct WebsitePoliciesData;
    
class WebFrameLoaderClient final : public WebCore::FrameLoaderClient {
public:
    explicit WebFrameLoaderClient(Ref<WebFrame>&&);
    ~WebFrameLoaderClient();

    WebFrame& webFrame() const { return m_frame.get(); }

    bool frameHasCustomContentProvider() const { return m_frameHasCustomContentProvider; }

    void setUseIconLoadingClient(bool useIconLoadingClient) { m_useIconLoadingClient = useIconLoadingClient; }

    void applyToDocumentLoader(WebsitePoliciesData&&);

    std::optional<WebPageProxyIdentifier> webPageProxyID() const;
    std::optional<WebCore::PageIdentifier> pageID() const final;

#if ENABLE(TRACKING_PREVENTION)
    bool hasFrameSpecificStorageAccess() final { return !!m_frameSpecificStorageAccessIdentifier; }
    
    struct FrameSpecificStorageAccessIdentifier {
        WebCore::FrameIdentifier frameID;
        WebCore::PageIdentifier pageID;
    };
    void setHasFrameSpecificStorageAccess(FrameSpecificStorageAccessIdentifier&&);
    void didLoadFromRegistrableDomain(WebCore::RegistrableDomain&&) final;
    Vector<WebCore::RegistrableDomain> loadedSubresourceDomains() const final;
#endif

    WebCore::AllowsContentJavaScript allowsContentJavaScriptFromMostRecentNavigation() const final;

private:
    bool hasHTMLView() const final;
    bool hasWebView() const final;
    
    void makeRepresentation(WebCore::DocumentLoader*) final;
#if PLATFORM(IOS_FAMILY)
    bool forceLayoutOnRestoreFromBackForwardCache() final;
#endif
    void forceLayoutForNonHTML() final;
    
    void setCopiesOnScroll() final;
    
    void detachedFromParent2() final;
    void detachedFromParent3() final;
    
    void assignIdentifierToInitialRequest(WebCore::ResourceLoaderIdentifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&) final;
    
    void dispatchWillSendRequest(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse) final;
    bool shouldUseCredentialStorage(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier) final;
    void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, const WebCore::AuthenticationChallenge&) final;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    bool canAuthenticateAgainstProtectionSpace(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, const WebCore::ProtectionSpace&) final;
#endif
#if PLATFORM(IOS_FAMILY)
    RetainPtr<CFDictionaryRef> connectionProperties(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier) final;
#endif
    void dispatchDidReceiveResponse(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, const WebCore::ResourceResponse&) final;
    void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, int dataLength) final;
    void dispatchDidFinishLoading(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier) final;
    void dispatchDidFailLoading(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, const WebCore::ResourceError&) final;
    bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length) final;
#if ENABLE(DATA_DETECTION)
    void dispatchDidFinishDataDetection(NSArray *detectionResults) final;
#endif
    void dispatchDidChangeMainDocument() final;
    void dispatchWillChangeDocument(const URL& currentUrl, const URL& newUrl) final;

    void dispatchDidDispatchOnloadEvents() final;
    void dispatchDidReceiveServerRedirectForProvisionalLoad() final;
    void dispatchDidChangeProvisionalURL() final;
    void dispatchDidCancelClientRedirect() final;
    void dispatchWillPerformClientRedirect(const URL&, double interval, WallTime fireDate, WebCore::LockBackForwardList) final;
    void dispatchDidChangeLocationWithinPage() final;
    void dispatchDidPushStateWithinPage() final;
    void dispatchDidReplaceStateWithinPage() final;
    void dispatchDidPopStateWithinPage() final;
    void didSameDocumentNavigationForFrameViaJSHistoryAPI(SameDocumentNavigationType);
    void dispatchWillClose() final;
    void dispatchDidStartProvisionalLoad() final;
    void dispatchDidReceiveTitle(const WebCore::StringWithDirection&) final;
    void dispatchDidCommitLoad(std::optional<WebCore::HasInsecureContent>, std::optional<WebCore::UsedLegacyTLS>, std::optional<WebCore::WasPrivateRelayed>) final;
    void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&, WebCore::WillContinueLoading) final;
    void dispatchDidFailLoad(const WebCore::ResourceError&) final;
    void dispatchDidFinishDocumentLoad() final;
    void dispatchDidFinishLoad() final;
    void dispatchDidExplicitOpen(const URL&, const String& mimeType) final;

    void dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone>) final;
    void dispatchDidReachVisuallyNonEmptyState() final;
    void dispatchDidLayout() final;

    WebCore::Frame* dispatchCreatePage(const WebCore::NavigationAction&, WebCore::NewFrameOpenerPolicy) final;
    void dispatchShow() final;
    
    void dispatchDecidePolicyForResponse(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, WebCore::PolicyCheckIdentifier, const String&, WebCore::FramePolicyFunction&&) final;
    void dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, WebCore::FormState*, const String& frameName, WebCore::PolicyCheckIdentifier, WebCore::FramePolicyFunction&&) final;
    void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse, WebCore::FormState*, WebCore::PolicyDecisionMode, WebCore::PolicyCheckIdentifier, WebCore::FramePolicyFunction&&) final;
    void cancelPolicyCheck() final;
    
    void dispatchUnableToImplementPolicy(const WebCore::ResourceError&) final;
    
    void dispatchWillSendSubmitEvent(Ref<WebCore::FormState>&&) final;
    void dispatchWillSubmitForm(WebCore::FormState&, CompletionHandler<void()>&&) final;
    
    void revertToProvisionalState(WebCore::DocumentLoader*) final;
    void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&) final;
    
    void setMainFrameDocumentReady(bool) final;
    
    void startDownload(const WebCore::ResourceRequest&, const String& suggestedName = String()) final;
    
    void willChangeTitle(WebCore::DocumentLoader*) final;
    void didChangeTitle(WebCore::DocumentLoader*) final;

    void willReplaceMultipartContent() final;
    void didReplaceMultipartContent() final;

    void committedLoad(WebCore::DocumentLoader*, const WebCore::SharedBuffer&) final;
    void finishedLoading(WebCore::DocumentLoader*) final;
    
    void updateGlobalHistory() final;
    void updateGlobalHistoryRedirectLinks() final;
    
    bool shouldGoToHistoryItem(WebCore::HistoryItem&) const final;

    void didDisplayInsecureContent() final;
    void didRunInsecureContent(WebCore::SecurityOrigin&, const URL&) final;

#if ENABLE(SERVICE_WORKER)
    void didFinishServiceWorkerPageRegistration(bool success) final;
#endif

    WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&) const final;
    WebCore::ResourceError blockedError(const WebCore::ResourceRequest&) const final;
    WebCore::ResourceError blockedByContentBlockerError(const WebCore::ResourceRequest&) const final;
    WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&) const final;
    WebCore::ResourceError interruptedForPolicyChangeError(const WebCore::ResourceRequest&) const final;
#if ENABLE(CONTENT_FILTERING)
    WebCore::ResourceError blockedByContentFilterError(const WebCore::ResourceRequest&) const final;
#endif
    
    WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&) const final;
    WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&) const final;
    WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&) const final;
    
    bool shouldFallBack(const WebCore::ResourceError&) const final;
    
    bool canHandleRequest(const WebCore::ResourceRequest&) const final;
    bool canShowMIMEType(const String& MIMEType) const final;
    bool canShowMIMETypeAsHTML(const String& MIMEType) const final;
    bool representationExistsForURLScheme(StringView URLScheme) const final;
    String generatedMIMETypeForURLScheme(StringView URLScheme) const final;
    
    void frameLoadCompleted() final;
    void saveViewStateToItem(WebCore::HistoryItem&) final;
    void restoreViewState() final;
    void provisionalLoadStarted() final;
    void didFinishLoad() final;
    void prepareForDataSourceReplacement() final;
    
    Ref<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&) final;
    void updateCachedDocumentLoader(WebCore::DocumentLoader&) final;

    void setTitle(const WebCore::StringWithDirection&, const URL&) final;
    
    String userAgent(const URL&) const final;

    String overrideContentSecurityPolicy() const final;

    void savePlatformDataToCachedFrame(WebCore::CachedFrame*) final;
    void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) final;
#if PLATFORM(IOS_FAMILY)
    void didRestoreFrameHierarchyForCachedFrame() final;
#endif
    void transitionToCommittedForNewPage() final;

    void didRestoreFromBackForwardCache() final;

    bool canCachePage() const final;
    void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&) final;

    RefPtr<WebCore::Frame> createFrame(const AtomString& name, WebCore::HTMLFrameOwnerElement&) final;

    RefPtr<WebCore::Widget> createPlugin(const WebCore::IntSize&, WebCore::HTMLPlugInElement&, const URL&, const Vector<AtomString>&, const Vector<AtomString>&, const String&, bool loadManually) final;
    void redirectDataToPlugin(WebCore::Widget&) final;
    
    WebCore::ObjectContentType objectContentType(const URL&, const String& mimeType) final;
    AtomString overrideMediaType() const final;

    void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld&) final;
    
    void dispatchGlobalObjectAvailable(WebCore::DOMWrapperWorld&) final;
    void dispatchServiceWorkerGlobalObjectAvailable(WebCore::DOMWrapperWorld&) final;
    void dispatchWillDisconnectDOMWindowExtensionFromGlobalObject(WebCore::DOMWindowExtension*) final;
    void dispatchDidReconnectDOMWindowExtensionToGlobalObject(WebCore::DOMWindowExtension*) final;
    void dispatchWillDestroyGlobalObjectForDOMWindowExtension(WebCore::DOMWindowExtension*) final;

    void willInjectUserScript(WebCore::DOMWrapperWorld&) final;

#if PLATFORM(COCOA)
    RemoteAXObjectRef accessibilityRemoteObject() final;
    
    void willCacheResponse(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, NSCachedURLResponse*, CompletionHandler<void(NSCachedURLResponse *)>&&) const final;

    NSDictionary *dataDetectionContext() final;
#endif

    void didChangeScrollOffset() final;

    bool allowScript(bool enabledPerSettings) final;

    bool shouldForceUniversalAccessFromLocalURL(const URL&) final;

    Ref<WebCore::FrameNetworkingContext> createNetworkingContext() final;

    void completePageTransitionIfNeeded() final;

#if USE(QUICK_LOOK)
    RefPtr<WebCore::LegacyPreviewLoaderClient> createPreviewLoaderClient(const String& fileName, const String& uti) final;
#endif

#if ENABLE(CONTENT_FILTERING)
    void contentFilterDidBlockLoad(WebCore::ContentFilterUnblockHandler) final;
#endif

    void prefetchDNS(const String&) final;
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&&) final;

    void didRestoreScrollPosition() final;

    void getLoadDecisionForIcons(const Vector<std::pair<WebCore::LinkIcon&, uint64_t>>&) final;

    inline bool hasPlugInView() const;

    Ref<WebFrame> m_frame;

#if ENABLE(PDFKIT_PLUGIN)
    RefPtr<PluginView> m_pluginView;
    bool m_hasSentResponseToPluginView { false };
#endif

    bool m_didCompletePageTransition { false };
    bool m_frameHasCustomContentProvider { false };
    bool m_frameCameFromBackForwardCache { false };
    bool m_useIconLoadingClient { false };
#if ENABLE(TRACKING_PREVENTION)
    std::optional<FrameSpecificStorageAccessIdentifier> m_frameSpecificStorageAccessIdentifier;
#endif

#if ENABLE(APP_BOUND_DOMAINS)
    bool shouldEnableInAppBrowserPrivacyProtections() const final;
    void notifyPageOfAppBoundBehavior() final;
#endif

#if ENABLE(PDFKIT_PLUGIN)
    bool shouldUsePDFPlugin(const String& contentType, StringView path) const final;
#endif

    bool isParentProcessAFullWebBrowser() const final;

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    void modelInlinePreviewUUIDs(CompletionHandler<void(Vector<String>)>&&) const final;
#endif
};

// As long as EmptyFrameLoaderClient exists in WebCore, this can return nullptr.
inline WebFrameLoaderClient* toWebFrameLoaderClient(WebCore::FrameLoaderClient& client)
{
    return client.isEmptyFrameLoaderClient() ? nullptr : static_cast<WebFrameLoaderClient*>(&client);
}

inline const WebFrameLoaderClient* toWebFrameLoaderClient(const WebCore::FrameLoaderClient& client)
{
    return client.isEmptyFrameLoaderClient() ? nullptr : static_cast<const WebFrameLoaderClient*>(&client);
}

} // namespace WebKit
