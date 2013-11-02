/*
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FrameLoaderClientWinCE_h
#define FrameLoaderClientWinCE_h

#include "FrameLoaderClient.h"
#include "PluginView.h"
#include "ResourceResponse.h"

class WebView;

namespace WebKit {

class FrameLoaderClientWinCE : public WebCore::FrameLoaderClient {
public:
    FrameLoaderClientWinCE(WebView*);
    virtual ~FrameLoaderClientWinCE();
    virtual void frameLoaderDestroyed() OVERRIDE;

    WebView* webView() const { return m_webView; }

    virtual bool hasWebView() const OVERRIDE;

    virtual void makeRepresentation(WebCore::DocumentLoader*) OVERRIDE;
    virtual void forceLayout() OVERRIDE;
    virtual void forceLayoutForNonHTML() OVERRIDE;

    virtual void setCopiesOnScroll() OVERRIDE;

    virtual void detachedFromParent2() OVERRIDE;
    virtual void detachedFromParent3() OVERRIDE;

    virtual void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&) OVERRIDE;

    virtual void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long  identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse) OVERRIDE;
    virtual bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long identifier) OVERRIDE;
    virtual void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&) OVERRIDE;
    virtual void dispatchDidCancelAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long  identifier, const WebCore::AuthenticationChallenge&) OVERRIDE;
    virtual void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long  identifier, const WebCore::ResourceResponse&) OVERRIDE;
    virtual void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int dataLength) OVERRIDE;
    virtual void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long  identifier) OVERRIDE;
    virtual void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long  identifier, const WebCore::ResourceError&) OVERRIDE;
    virtual bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length) OVERRIDE;

    virtual void dispatchDidHandleOnloadEvents() OVERRIDE;
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() OVERRIDE;
    virtual void dispatchDidCancelClientRedirect() OVERRIDE;
    virtual void dispatchWillPerformClientRedirect(const WebCore::URL&, double, double) OVERRIDE;
    virtual void dispatchDidChangeLocationWithinPage() OVERRIDE;
    virtual void dispatchDidPushStateWithinPage() OVERRIDE;
    virtual void dispatchDidReplaceStateWithinPage() OVERRIDE;
    virtual void dispatchDidPopStateWithinPage() OVERRIDE;
    virtual void dispatchWillClose() OVERRIDE;
    virtual void dispatchDidReceiveIcon() OVERRIDE;
    virtual void dispatchDidStartProvisionalLoad() OVERRIDE;
    virtual void dispatchDidReceiveTitle(const WebCore::StringWithDirection&) OVERRIDE;
    virtual void dispatchDidChangeIcons(WebCore::IconType) OVERRIDE;
    virtual void dispatchDidCommitLoad() OVERRIDE;
    virtual void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&) OVERRIDE;
    virtual void dispatchDidFailLoad(const WebCore::ResourceError&) OVERRIDE;
    virtual void dispatchDidFinishDocumentLoad() OVERRIDE;
    virtual void dispatchDidFinishLoad() OVERRIDE;
    virtual void dispatchDidLayout(WebCore::LayoutMilestones) OVERRIDE;

    virtual WebCore::Frame* dispatchCreatePage(const WebCore::NavigationAction&) OVERRIDE;
    virtual void dispatchShow() OVERRIDE;

    virtual void dispatchDecidePolicyForResponse(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, WebCore::FramePolicyFunction) OVERRIDE;
    virtual void dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, WTF::PassRefPtr<WebCore::FormState>, const WTF::String& frameName, WebCore::FramePolicyFunction) OVERRIDE;
    virtual void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, WTF::PassRefPtr<WebCore::FormState>, WebCore::FramePolicyFunction) OVERRIDE;
    virtual void cancelPolicyCheck() OVERRIDE;

    virtual void dispatchUnableToImplementPolicy(const WebCore::ResourceError&) OVERRIDE;

    virtual void dispatchWillSendSubmitEvent(WTF::PassRefPtr<WebCore::FormState>) OVERRIDE { }
    virtual void dispatchWillSubmitForm(WTF::PassRefPtr<WebCore::FormState>, WebCore::FramePolicyFunction) OVERRIDE;

    virtual void revertToProvisionalState(WebCore::DocumentLoader*) OVERRIDE;
    virtual void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&) OVERRIDE;

    virtual void postProgressStartedNotification() OVERRIDE;
    virtual void postProgressEstimateChangedNotification() OVERRIDE;
    virtual void postProgressFinishedNotification() OVERRIDE;

    virtual PassRefPtr<WebCore::Frame> createFrame(const WebCore::URL&, const WTF::String& name, WebCore::HTMLFrameOwnerElement*, const WTF::String& referrer, bool allowsScrolling, int marginWidth, int marginHeight) OVERRIDE;
    virtual PassRefPtr<WebCore::Widget> createPlugin(const WebCore::IntSize&, WebCore::HTMLPlugInElement*, const WebCore::URL&, const WTF::Vector<WTF::String>&, const WTF::Vector<WTF::String>&, const WTF::String&, bool) OVERRIDE;
    virtual void recreatePlugin(WebCore::Widget*) OVERRIDE { }
    virtual void redirectDataToPlugin(WebCore::Widget* pluginWidget) OVERRIDE;
    virtual PassRefPtr<WebCore::Widget> createJavaAppletWidget(const WebCore::IntSize&, WebCore::HTMLAppletElement*, const WebCore::URL& baseURL, const WTF::Vector<WTF::String>& paramNames, const WTF::Vector<WTF::String>& paramValues) OVERRIDE;
    virtual WTF::String overrideMediaType() const OVERRIDE;
    virtual void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld&) OVERRIDE;
    virtual void documentElementAvailable() OVERRIDE;
    virtual void didPerformFirstNavigation() const OVERRIDE;

    virtual void registerForIconNotification(bool) OVERRIDE;

    virtual WebCore::ObjectContentType objectContentType(const WebCore::URL&, const WTF::String& mimeType, bool shouldPreferPlugInsForImages) OVERRIDE;

    virtual void setMainFrameDocumentReady(bool) OVERRIDE;

    virtual void startDownload(const WebCore::ResourceRequest&, const String& suggestedName = String()) OVERRIDE;

    virtual void willChangeTitle(WebCore::DocumentLoader*) OVERRIDE;
    virtual void didChangeTitle(WebCore::DocumentLoader*) OVERRIDE;

    virtual void committedLoad(WebCore::DocumentLoader*, const char*, int) OVERRIDE;
    virtual void finishedLoading(WebCore::DocumentLoader*) OVERRIDE;

    virtual void updateGlobalHistory() OVERRIDE;
    virtual void updateGlobalHistoryRedirectLinks() OVERRIDE;
    virtual bool shouldGoToHistoryItem(WebCore::HistoryItem*) const OVERRIDE;
    virtual bool shouldStopLoadingForHistoryItem(WebCore::HistoryItem*) const OVERRIDE;

    virtual void didDisplayInsecureContent() OVERRIDE;
    virtual void didRunInsecureContent(WebCore::SecurityOrigin*, const WebCore::URL&) OVERRIDE;
    virtual void didDetectXSS(const WebCore::URL&, bool didBlockEntirePage) OVERRIDE;

    virtual WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&) OVERRIDE;
    virtual WebCore::ResourceError blockedError(const WebCore::ResourceRequest&) OVERRIDE;
    virtual WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&) OVERRIDE;
    virtual WebCore::ResourceError interruptedForPolicyChangeError(const WebCore::ResourceRequest&) OVERRIDE;

    virtual WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&) OVERRIDE;
    virtual WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&) OVERRIDE;
    virtual WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&) OVERRIDE;

    virtual bool shouldFallBack(const WebCore::ResourceError&) OVERRIDE;

    virtual bool canHandleRequest(const WebCore::ResourceRequest&) const OVERRIDE;
    virtual bool canShowMIMEType(const WTF::String&) const OVERRIDE;
    virtual bool canShowMIMETypeAsHTML(const WTF::String&) const OVERRIDE;
    virtual bool representationExistsForURLScheme(const WTF::String&) const OVERRIDE;
    virtual WTF::String generatedMIMETypeForURLScheme(const WTF::String&) const OVERRIDE;

    virtual void frameLoadCompleted() OVERRIDE;
    virtual void saveViewStateToItem(WebCore::HistoryItem*) OVERRIDE;
    virtual void restoreViewState() OVERRIDE;
    virtual void provisionalLoadStarted() OVERRIDE;
    virtual void didFinishLoad() OVERRIDE;
    virtual void prepareForDataSourceReplacement() OVERRIDE;

    virtual WTF::PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&) OVERRIDE;
    virtual void setTitle(const WebCore::StringWithDirection&, const WebCore::URL&) OVERRIDE;

    virtual WTF::String userAgent(const WebCore::URL&) OVERRIDE;

    virtual void savePlatformDataToCachedFrame(WebCore::CachedFrame*) OVERRIDE;
    virtual void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) OVERRIDE;
    virtual void transitionToCommittedForNewPage() OVERRIDE;

    virtual void didSaveToPageCache() OVERRIDE;
    virtual void didRestoreFromPageCache() OVERRIDE;

    virtual void dispatchDidBecomeFrameset(bool) OVERRIDE;

    virtual bool canCachePage() const OVERRIDE;
    virtual void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&) OVERRIDE;

    virtual PassRefPtr<WebCore::FrameNetworkingContext> createNetworkingContext() OVERRIDE;

    void setFrame(WebCore::Frame *frame) { m_frame = frame; }
    WebCore::Frame *frame() { return m_frame; }

private:
    WebView* m_webView;
    WebCore::Frame* m_frame;
    WebCore::ResourceResponse m_response;

    // Plugin view to redirect data to
    WebCore::PluginView* m_pluginView;
    bool m_hasSentResponseToPlugin;
};

} // namespace WebKit

#endif // FrameLoaderClientWinCE_h
