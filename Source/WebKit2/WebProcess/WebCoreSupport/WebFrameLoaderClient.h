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

namespace WebKit {

class PluginView;
class WebFrame;
    
class WebFrameLoaderClient : public WebCore::FrameLoaderClient {
public:
    WebFrameLoaderClient();
    ~WebFrameLoaderClient();

    void setWebFrame(WebFrame* webFrame) { m_frame = webFrame; }
    WebFrame* webFrame() const { return m_frame; }

private:
    virtual void frameLoaderDestroyed() override;

    virtual bool hasHTMLView() const override { return true; }
    virtual bool hasWebView() const override;
    
    virtual void makeRepresentation(WebCore::DocumentLoader*) override;
    virtual void forceLayout() override;
#if PLATFORM(IOS)
    virtual void forceLayoutWithoutRecalculatingStyles() override;
#endif
    virtual void forceLayoutForNonHTML() override;
    
    virtual void setCopiesOnScroll() override;
    
    virtual void detachedFromParent2() override;
    virtual void detachedFromParent3() override;
    
    virtual void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&) override;
    
    virtual void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse) override;
    virtual bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long identifier) override;
    virtual void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&) override;
    virtual void dispatchDidCancelAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&) override;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual bool canAuthenticateAgainstProtectionSpace(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ProtectionSpace&) override;
#endif
#if PLATFORM(IOS)
    virtual RetainPtr<CFDictionaryRef> connectionProperties(WebCore::DocumentLoader*, unsigned long identifier) override;
#endif
    virtual void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&) override;
    virtual void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int dataLength) override;
    virtual void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier) override;
    virtual void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceError&) override;
    virtual bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length) override;
    
    virtual void dispatchDidHandleOnloadEvents() override;
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() override;
    virtual void dispatchDidCancelClientRedirect() override;
    virtual void dispatchWillPerformClientRedirect(const WebCore::URL&, double interval, double fireDate) override;
    virtual void dispatchDidChangeLocationWithinPage() override;
    virtual void dispatchDidPushStateWithinPage() override;
    virtual void dispatchDidReplaceStateWithinPage() override;
    virtual void dispatchDidPopStateWithinPage() override;
    virtual void dispatchWillClose() override;
    virtual void dispatchDidReceiveIcon() override;
    virtual void dispatchDidStartProvisionalLoad() override;
    virtual void dispatchDidReceiveTitle(const WebCore::StringWithDirection&) override;
    virtual void dispatchDidChangeIcons(WebCore::IconType) override;
    virtual void dispatchDidCommitLoad() override;
    virtual void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&) override;
    virtual void dispatchDidFailLoad(const WebCore::ResourceError&) override;
    virtual void dispatchDidFinishDocumentLoad() override;
    virtual void dispatchDidFinishLoad() override;

    virtual void dispatchDidLayout(WebCore::LayoutMilestones) override;
    virtual void dispatchDidLayout() override;

    virtual WebCore::Frame* dispatchCreatePage(const WebCore::NavigationAction&) override;
    virtual void dispatchShow() override;
    
    virtual void dispatchDecidePolicyForResponse(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, WebCore::FramePolicyFunction) override;
    virtual void dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<WebCore::FormState>, const String& frameName, WebCore::FramePolicyFunction) override;
    virtual void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<WebCore::FormState>, WebCore::FramePolicyFunction) override;
    virtual void cancelPolicyCheck() override;
    
    virtual void dispatchUnableToImplementPolicy(const WebCore::ResourceError&) override;
    
    virtual void dispatchWillSendSubmitEvent(PassRefPtr<WebCore::FormState>) override;
    virtual void dispatchWillSubmitForm(PassRefPtr<WebCore::FormState>, WebCore::FramePolicyFunction) override;
    
    virtual void revertToProvisionalState(WebCore::DocumentLoader*) override;
    virtual void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&) override;
    
    virtual void setMainFrameDocumentReady(bool) override;
    
    virtual void startDownload(const WebCore::ResourceRequest&, const String& suggestedName = String()) override;
    
    virtual void willChangeTitle(WebCore::DocumentLoader*) override;
    virtual void didChangeTitle(WebCore::DocumentLoader*) override;
    
    virtual void committedLoad(WebCore::DocumentLoader*, const char*, int) override;
    virtual void finishedLoading(WebCore::DocumentLoader*) override;
    
    virtual void updateGlobalHistory() override;
    virtual void updateGlobalHistoryRedirectLinks() override;
    
    virtual bool shouldGoToHistoryItem(WebCore::HistoryItem*) const override;

    virtual void didDisplayInsecureContent() override;
    virtual void didRunInsecureContent(WebCore::SecurityOrigin*, const WebCore::URL&) override;
    virtual void didDetectXSS(const WebCore::URL&, bool didBlockEntirePage) override;

    virtual WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&) override;
    virtual WebCore::ResourceError blockedError(const WebCore::ResourceRequest&) override;
    virtual WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&) override;
    virtual WebCore::ResourceError interruptedForPolicyChangeError(const WebCore::ResourceRequest&) override;
    
    virtual WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&) override;
    virtual WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&) override;
    virtual WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&) override;
    
    virtual bool shouldFallBack(const WebCore::ResourceError&) override;
    
    virtual bool canHandleRequest(const WebCore::ResourceRequest&) const override;
    virtual bool canShowMIMEType(const String& MIMEType) const override;
    virtual bool canShowMIMETypeAsHTML(const String& MIMEType) const override;
    virtual bool representationExistsForURLScheme(const String& URLScheme) const override;
    virtual String generatedMIMETypeForURLScheme(const String& URLScheme) const override;
    
    virtual void frameLoadCompleted() override;
    virtual void saveViewStateToItem(WebCore::HistoryItem*) override;
    virtual void restoreViewState() override;
    virtual void provisionalLoadStarted() override;
    virtual void didFinishLoad() override;
    virtual void prepareForDataSourceReplacement() override;
    
    virtual PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&);
    virtual void setTitle(const WebCore::StringWithDirection&, const WebCore::URL&) override;
    
    virtual String userAgent(const WebCore::URL&) override;
    
    virtual void savePlatformDataToCachedFrame(WebCore::CachedFrame*) override;
    virtual void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) override;
#if PLATFORM(IOS)
    virtual void didRestoreFrameHierarchyForCachedFrame() override;
#endif
    virtual void transitionToCommittedForNewPage() override;

    virtual void didSaveToPageCache() override;
    virtual void didRestoreFromPageCache() override;

    virtual void dispatchDidBecomeFrameset(bool) override;

    virtual bool canCachePage() const override { return true; }
    virtual void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&) override;
    
    virtual PassRefPtr<WebCore::Frame> createFrame(const WebCore::URL& url, const String& name, WebCore::HTMLFrameOwnerElement* ownerElement,
                                          const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight) override;
    
    virtual PassRefPtr<WebCore::Widget> createPlugin(const WebCore::IntSize&, WebCore::HTMLPlugInElement*, const WebCore::URL&, const Vector<String>&, const Vector<String>&, const String&, bool loadManually) override;
    virtual void recreatePlugin(WebCore::Widget*) override;
    virtual void redirectDataToPlugin(WebCore::Widget* pluginWidget) override;
    
#if ENABLE(WEBGL)
    virtual WebCore::WebGLLoadPolicy webGLPolicyForURL(const String&) const override;
#endif // ENABLE(WEBGL)

    virtual PassRefPtr<WebCore::Widget> createJavaAppletWidget(const WebCore::IntSize&, WebCore::HTMLAppletElement*, const WebCore::URL& baseURL, const Vector<String>& paramNames, const Vector<String>& paramValues) override;
    
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    virtual PassRefPtr<WebCore::Widget> createMediaPlayerProxyPlugin(const WebCore::IntSize&, WebCore::HTMLMediaElement*, const WebCore::URL&, const Vector<String>&, const Vector<String>&, const String&) override;
    virtual void hideMediaPlayerProxyPlugin(WebCore::Widget*) override;
    virtual void showMediaPlayerProxyPlugin(WebCore::Widget*) override;
#endif

    virtual WebCore::ObjectContentType objectContentType(const WebCore::URL&, const String& mimeType, bool shouldPreferPlugInsForImages) override;
    virtual String overrideMediaType() const override;

    virtual void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld&) override;
    
    virtual void dispatchGlobalObjectAvailable(WebCore::DOMWrapperWorld&) override;
    virtual void dispatchWillDisconnectDOMWindowExtensionFromGlobalObject(WebCore::DOMWindowExtension*) override;
    virtual void dispatchDidReconnectDOMWindowExtensionToGlobalObject(WebCore::DOMWindowExtension*) override;
    virtual void dispatchWillDestroyGlobalObjectForDOMWindowExtension(WebCore::DOMWindowExtension*) override;

    virtual void registerForIconNotification(bool listen = true) override;
    
#if PLATFORM(MAC)
    virtual RemoteAXObjectRef accessibilityRemoteObject() override;
    
    virtual NSCachedURLResponse* willCacheResponse(WebCore::DocumentLoader*, unsigned long identifier, NSCachedURLResponse*) const override;
#endif

    virtual bool shouldAlwaysUsePluginDocument(const String& /*mimeType*/) const override;

    virtual void didChangeScrollOffset() override;

    virtual bool allowScript(bool enabledPerSettings) override;

    virtual bool shouldForceUniversalAccessFromLocalURL(const WebCore::URL&) override;

    virtual PassRefPtr<WebCore::FrameNetworkingContext> createNetworkingContext() override;

    virtual void forcePageTransitionIfNeeded() override;

    WebFrame* m_frame;
    RefPtr<PluginView> m_pluginView;
    bool m_hasSentResponseToPluginView;
    bool m_didCompletePageTransitionAlready;
    bool m_frameCameFromPageCache;
};

// As long as EmptyFrameLoaderClient exists in WebCore, this can return 0.
inline WebFrameLoaderClient* toWebFrameLoaderClient(WebCore::FrameLoaderClient& client)
{
    return client.isEmptyFrameLoaderClient() ? 0 : static_cast<WebFrameLoaderClient*>(&client);
}

} // namespace WebKit

#endif // WebFrameLoaderClient_h
