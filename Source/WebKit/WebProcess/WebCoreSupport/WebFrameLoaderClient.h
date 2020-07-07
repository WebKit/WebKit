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

#include "WebPageProxyIdentifier.h"
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

    Optional<WebPageProxyIdentifier> webPageProxyID() const;
    Optional<WebCore::PageIdentifier> pageID() const final;
    Optional<WebCore::FrameIdentifier> frameID() const final;

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    bool hasFrameSpecificStorageAccess() final { return !!m_frameSpecificStorageAccessIdentifier; }
    
    struct FrameSpecificStorageAccessIdentifier {
        WebCore::FrameIdentifier frameID;
        WebCore::PageIdentifier pageID;
    };
    void setHasFrameSpecificStorageAccess(FrameSpecificStorageAccessIdentifier&&);
    void didLoadFromRegistrableDomain(WebCore::RegistrableDomain&&) final;
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
    
    void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&) final;
    
    void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse) final;
    bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long identifier) final;
    void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&) final;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    bool canAuthenticateAgainstProtectionSpace(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ProtectionSpace&) final;
#endif
#if PLATFORM(IOS_FAMILY)
    RetainPtr<CFDictionaryRef> connectionProperties(WebCore::DocumentLoader*, unsigned long identifier) final;
#endif
    void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&) final;
    void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int dataLength) final;
    void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier) final;
    void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceError&) final;
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
    void dispatchWillClose() final;
    void dispatchDidStartProvisionalLoad() final;
    void dispatchDidReceiveTitle(const WebCore::StringWithDirection&) final;
    void dispatchDidCommitLoad(Optional<WebCore::HasInsecureContent>, Optional<WebCore::UsedLegacyTLS>) final;
    void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&, WebCore::WillContinueLoading) final;
    void dispatchDidFailLoad(const WebCore::ResourceError&) final;
    void dispatchDidFinishDocumentLoad() final;
    void dispatchDidFinishLoad() final;
    void dispatchDidExplicitOpen(const URL&, const String& mimeType) final;

    void dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone>) final;
    void dispatchDidReachVisuallyNonEmptyState() final;
    void dispatchDidLayout() final;

    WebCore::Frame* dispatchCreatePage(const WebCore::NavigationAction&) final;
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

    void committedLoad(WebCore::DocumentLoader*, const char*, int) final;
    void finishedLoading(WebCore::DocumentLoader*) final;
    
    void updateGlobalHistory() final;
    void updateGlobalHistoryRedirectLinks() final;
    
    bool shouldGoToHistoryItem(WebCore::HistoryItem&) const final;

    void didDisplayInsecureContent() final;
    void didRunInsecureContent(WebCore::SecurityOrigin&, const URL&) final;
    void didDetectXSS(const URL&, bool didBlockEntirePage) final;

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
    bool representationExistsForURLScheme(const String& URLScheme) const final;
    String generatedMIMETypeForURLScheme(const String& URLScheme) const final;
    
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

    RefPtr<WebCore::Frame> createFrame(const String& name, WebCore::HTMLFrameOwnerElement&) final;

    RefPtr<WebCore::Widget> createPlugin(const WebCore::IntSize&, WebCore::HTMLPlugInElement&, const URL&, const Vector<String>&, const Vector<String>&, const String&, bool loadManually) final;
    void redirectDataToPlugin(WebCore::Widget&) final;
    
#if ENABLE(WEBGL)
    WebCore::WebGLLoadPolicy webGLPolicyForURL(const URL&) const final;
    WebCore::WebGLLoadPolicy resolveWebGLPolicyForURL(const URL&) const final;
#endif // ENABLE(WEBGL)

    RefPtr<WebCore::Widget> createJavaAppletWidget(const WebCore::IntSize&, WebCore::HTMLAppletElement&, const URL& baseURL, const Vector<String>& paramNames, const Vector<String>& paramValues) final;
    
    WebCore::ObjectContentType objectContentType(const URL&, const String& mimeType) final;
    String overrideMediaType() const final;

    void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld&) final;
    
    void dispatchGlobalObjectAvailable(WebCore::DOMWrapperWorld&) final;
    void dispatchWillDisconnectDOMWindowExtensionFromGlobalObject(WebCore::DOMWindowExtension*) final;
    void dispatchDidReconnectDOMWindowExtensionToGlobalObject(WebCore::DOMWindowExtension*) final;
    void dispatchWillDestroyGlobalObjectForDOMWindowExtension(WebCore::DOMWindowExtension*) final;

    void willInjectUserScript(WebCore::DOMWrapperWorld&) final;

#if PLATFORM(COCOA)
    RemoteAXObjectRef accessibilityRemoteObject() final;
    
    void willCacheResponse(WebCore::DocumentLoader*, unsigned long identifier, NSCachedURLResponse*, CompletionHandler<void(NSCachedURLResponse *)>&&) const final;

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
    void finishedLoadingIcon(uint64_t callbackIdentifier, WebCore::SharedBuffer*) final;

    void didCreateWindow(WebCore::DOMWindow&) final;

#if ENABLE(APPLICATION_MANIFEST)
    void finishedLoadingApplicationManifest(uint64_t, const Optional<WebCore::ApplicationManifest>&) final;
#endif

    Ref<WebFrame> m_frame;
    RefPtr<PluginView> m_pluginView;
    bool m_hasSentResponseToPluginView { false };
    bool m_didCompletePageTransition { false };
    bool m_frameHasCustomContentProvider { false };
    bool m_frameCameFromBackForwardCache { false };
    bool m_useIconLoadingClient { false };
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    Optional<FrameSpecificStorageAccessIdentifier> m_frameSpecificStorageAccessIdentifier;
#endif

    bool shouldEnableInAppBrowserPrivacyProtections() const final;
    void notifyPageOfAppBoundBehavior() final;
};

// As long as EmptyFrameLoaderClient exists in WebCore, this can return 0.
inline WebFrameLoaderClient* toWebFrameLoaderClient(WebCore::FrameLoaderClient& client)
{
    return client.isEmptyFrameLoaderClient() ? 0 : static_cast<WebFrameLoaderClient*>(&client);
}

} // namespace WebKit
