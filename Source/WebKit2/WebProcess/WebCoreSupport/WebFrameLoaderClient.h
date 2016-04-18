/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef WebFrameLoaderClient_h
#define WebFrameLoaderClient_h

#include <WebCore/FrameLoaderClient.h>

namespace WebCore {
class SessionID;
}

namespace WebKit {

class PluginView;
class WebFrame;
    
class WebFrameLoaderClient : public WebCore::FrameLoaderClient {
public:
    WebFrameLoaderClient();
    ~WebFrameLoaderClient();

    void setWebFrame(WebFrame* webFrame) { m_frame = webFrame; }
    WebFrame* webFrame() const { return m_frame; }

    bool frameHasCustomContentProvider() const { return m_frameHasCustomContentProvider; }

private:
    void frameLoaderDestroyed() override;

    bool hasHTMLView() const override;
    bool hasWebView() const override;
    
    void makeRepresentation(WebCore::DocumentLoader*) override;
#if PLATFORM(IOS)
    bool forceLayoutOnRestoreFromPageCache() override;
#endif
    void forceLayoutForNonHTML() override;
    
    void setCopiesOnScroll() override;
    
    void detachedFromParent2() override;
    void detachedFromParent3() override;
    
    void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&) override;
    
    void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse) override;
    bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long identifier) override;
    void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&) override;
    void dispatchDidCancelAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&) override;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    bool canAuthenticateAgainstProtectionSpace(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ProtectionSpace&) override;
#endif
#if PLATFORM(IOS)
    RetainPtr<CFDictionaryRef> connectionProperties(WebCore::DocumentLoader*, unsigned long identifier) override;
#endif
    void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&) override;
    void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int dataLength) override;
    void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier) override;
    void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceError&) override;
    bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length) override;
#if ENABLE(DATA_DETECTION)
    void dispatchDidFinishDataDetection(NSArray *detectionResults) override;
#endif
    
    void dispatchDidDispatchOnloadEvents() override;
    void dispatchDidReceiveServerRedirectForProvisionalLoad() override;
    void dispatchDidChangeProvisionalURL() override;
    void dispatchDidCancelClientRedirect() override;
    void dispatchWillPerformClientRedirect(const WebCore::URL&, double interval, double fireDate) override;
    void dispatchDidChangeLocationWithinPage() override;
    void dispatchDidPushStateWithinPage() override;
    void dispatchDidReplaceStateWithinPage() override;
    void dispatchDidPopStateWithinPage() override;
    void dispatchWillClose() override;
    void dispatchDidReceiveIcon() override;
    void dispatchDidStartProvisionalLoad() override;
    void dispatchDidReceiveTitle(const WebCore::StringWithDirection&) override;
    void dispatchDidCommitLoad() override;
    void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&) override;
    void dispatchDidFailLoad(const WebCore::ResourceError&) override;
    void dispatchDidFinishDocumentLoad() override;
    void dispatchDidFinishLoad() override;

    void dispatchDidLayout(WebCore::LayoutMilestones) override;
    void dispatchDidLayout() override;

    WebCore::Frame* dispatchCreatePage(const WebCore::NavigationAction&) override;
    void dispatchShow() override;
    
    void dispatchDecidePolicyForResponse(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, WebCore::FramePolicyFunction) override;
    void dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<WebCore::FormState>, const String& frameName, WebCore::FramePolicyFunction) override;
    void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<WebCore::FormState>, WebCore::FramePolicyFunction) override;
    void cancelPolicyCheck() override;
    
    void dispatchUnableToImplementPolicy(const WebCore::ResourceError&) override;
    
    void dispatchWillSendSubmitEvent(PassRefPtr<WebCore::FormState>) override;
    void dispatchWillSubmitForm(PassRefPtr<WebCore::FormState>, WebCore::FramePolicyFunction) override;
    
    void revertToProvisionalState(WebCore::DocumentLoader*) override;
    void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&) override;
    
    void setMainFrameDocumentReady(bool) override;
    
    void startDownload(const WebCore::ResourceRequest&, const String& suggestedName = String()) override;
    
    void willChangeTitle(WebCore::DocumentLoader*) override;
    void didChangeTitle(WebCore::DocumentLoader*) override;

    void willReplaceMultipartContent() override;
    void didReplaceMultipartContent() override;

    void committedLoad(WebCore::DocumentLoader*, const char*, int) override;
    void finishedLoading(WebCore::DocumentLoader*) override;
    
    void updateGlobalHistory() override;
    void updateGlobalHistoryRedirectLinks() override;
    
    bool shouldGoToHistoryItem(WebCore::HistoryItem*) const override;

    void didDisplayInsecureContent() override;
    void didRunInsecureContent(WebCore::SecurityOrigin*, const WebCore::URL&) override;
    void didDetectXSS(const WebCore::URL&, bool didBlockEntirePage) override;

    WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&) override;
    WebCore::ResourceError blockedError(const WebCore::ResourceRequest&) override;
    WebCore::ResourceError blockedByContentBlockerError(const WebCore::ResourceRequest&) override;
    WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&) override;
    WebCore::ResourceError interruptedForPolicyChangeError(const WebCore::ResourceRequest&) override;
    
    WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&) override;
    WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&) override;
    WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&) override;
    
    bool shouldFallBack(const WebCore::ResourceError&) override;
    
    bool canHandleRequest(const WebCore::ResourceRequest&) const override;
    bool canShowMIMEType(const String& MIMEType) const override;
    bool canShowMIMETypeAsHTML(const String& MIMEType) const override;
    bool representationExistsForURLScheme(const String& URLScheme) const override;
    String generatedMIMETypeForURLScheme(const String& URLScheme) const override;
    
    void frameLoadCompleted() override;
    void saveViewStateToItem(WebCore::HistoryItem*) override;
    void restoreViewState() override;
    void provisionalLoadStarted() override;
    void didFinishLoad() override;
    void prepareForDataSourceReplacement() override;
    
    Ref<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&) override;
    void updateCachedDocumentLoader(WebCore::DocumentLoader&) override;

    void setTitle(const WebCore::StringWithDirection&, const WebCore::URL&) override;
    
    String userAgent(const WebCore::URL&) override;
    
    void savePlatformDataToCachedFrame(WebCore::CachedFrame*) override;
    void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) override;
#if PLATFORM(IOS)
    void didRestoreFrameHierarchyForCachedFrame() override;
#endif
    void transitionToCommittedForNewPage() override;

    void didSaveToPageCache() override;
    void didRestoreFromPageCache() override;

    void dispatchDidBecomeFrameset(bool) override;

    bool canCachePage() const override;
    void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, WebCore::SessionID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&) override;

    RefPtr<WebCore::Frame> createFrame(const WebCore::URL&, const String& name, WebCore::HTMLFrameOwnerElement*,
                                          const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight) override;
    
    RefPtr<WebCore::Widget> createPlugin(const WebCore::IntSize&, WebCore::HTMLPlugInElement*, const WebCore::URL&, const Vector<String>&, const Vector<String>&, const String&, bool loadManually) override;
    void recreatePlugin(WebCore::Widget*) override;
    void redirectDataToPlugin(WebCore::Widget* pluginWidget) override;
    
#if ENABLE(WEBGL)
    WebCore::WebGLLoadPolicy webGLPolicyForURL(const String&) const override;
    WebCore::WebGLLoadPolicy resolveWebGLPolicyForURL(const String&) const override;
#endif // ENABLE(WEBGL)

    PassRefPtr<WebCore::Widget> createJavaAppletWidget(const WebCore::IntSize&, WebCore::HTMLAppletElement*, const WebCore::URL& baseURL, const Vector<String>& paramNames, const Vector<String>& paramValues) override;
    
    WebCore::ObjectContentType objectContentType(const WebCore::URL&, const String& mimeType) override;
    String overrideMediaType() const override;

    void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld&) override;
    
    void dispatchGlobalObjectAvailable(WebCore::DOMWrapperWorld&) override;
    void dispatchWillDisconnectDOMWindowExtensionFromGlobalObject(WebCore::DOMWindowExtension*) override;
    void dispatchDidReconnectDOMWindowExtensionToGlobalObject(WebCore::DOMWindowExtension*) override;
    void dispatchWillDestroyGlobalObjectForDOMWindowExtension(WebCore::DOMWindowExtension*) override;

    void registerForIconNotification(bool listen = true) override;
    
#if PLATFORM(COCOA)
    RemoteAXObjectRef accessibilityRemoteObject() override;
    
    NSCachedURLResponse* willCacheResponse(WebCore::DocumentLoader*, unsigned long identifier, NSCachedURLResponse*) const override;
#endif

    bool shouldAlwaysUsePluginDocument(const String& /*mimeType*/) const override;

    void didChangeScrollOffset() override;

    bool allowScript(bool enabledPerSettings) override;

    bool shouldForceUniversalAccessFromLocalURL(const WebCore::URL&) override;

    PassRefPtr<WebCore::FrameNetworkingContext> createNetworkingContext() override;

#if ENABLE(REQUEST_AUTOCOMPLETE)
    void didRequestAutocomplete(PassRefPtr<WebCore::FormState>) override;
#endif

    void forcePageTransitionIfNeeded() override;

#if USE(QUICK_LOOK)
    void didCreateQuickLookHandle(WebCore::QuickLookHandle&) override;
#endif

#if ENABLE(CONTENT_FILTERING)
    void contentFilterDidBlockLoad(WebCore::ContentFilterUnblockHandler) override;
#endif

    void prefetchDNS(const String&) override;

    void didRestoreScrollPosition() override;

    WebFrame* m_frame;
    RefPtr<PluginView> m_pluginView;
    bool m_hasSentResponseToPluginView;
    bool m_didCompletePageTransition;
    bool m_frameHasCustomContentProvider;
    bool m_frameCameFromPageCache;
};

// As long as EmptyFrameLoaderClient exists in WebCore, this can return 0.
inline WebFrameLoaderClient* toWebFrameLoaderClient(WebCore::FrameLoaderClient& client)
{
    return client.isEmptyFrameLoaderClient() ? 0 : static_cast<WebFrameLoaderClient*>(&client);
}

} // namespace WebKit

#endif // WebFrameLoaderClient_h
