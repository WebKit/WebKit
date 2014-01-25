/*
 * Copyright (C) 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#ifndef WebFrameLoaderClient_h
#define WebFrameLoaderClient_h

#include <WebCore/COMPtr.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/ProgressTrackerClient.h>

namespace WebCore {
    class PluginManualLoader;
    class PluginView;
}

class WebFrame;
class WebFramePolicyListener;
class WebHistory;

class WebFrameLoaderClient : public WebCore::FrameLoaderClient, public WebCore::ProgressTrackerClient {
public:
    WebFrameLoaderClient(WebFrame* = 0);
    ~WebFrameLoaderClient();

    void setWebFrame(WebFrame* webFrame) { m_webFrame = webFrame; }
    WebFrame* webFrame() const { return m_webFrame; }

    virtual bool hasWebView() const;

    virtual void forceLayout() override;

    virtual PassRefPtr<WebCore::FrameNetworkingContext> createNetworkingContext();

    virtual void frameLoaderDestroyed() override;
    virtual void makeRepresentation(WebCore::DocumentLoader*) override;
    virtual void forceLayoutForNonHTML() override;

    virtual void setCopiesOnScroll() override;

    virtual void detachedFromParent2() override;
    virtual void detachedFromParent3() override;

    virtual void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&) override;
    virtual void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&);

    virtual void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse) override;
    virtual bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long identifier) override;
    virtual void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&) override;
    virtual void dispatchDidCancelAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&) override;
    virtual void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&) override;
    virtual void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int dataLength) override;
    virtual void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier) override;
    virtual void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceError&) override;
    virtual bool shouldCacheResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&, const unsigned char* data, unsigned long long length);

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

    virtual void dispatchDecidePolicyForResponse(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, WebCore::FramePolicyFunction) override;
    virtual void dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<WebCore::FormState>, const WTF::String& frameName, WebCore::FramePolicyFunction) override;
    virtual void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<WebCore::FormState>, WebCore::FramePolicyFunction) override;
    virtual void cancelPolicyCheck() override;

    virtual void dispatchUnableToImplementPolicy(const WebCore::ResourceError&) override;

    virtual void dispatchWillSendSubmitEvent(PassRefPtr<WebCore::FormState>) override;
    virtual void dispatchWillSubmitForm(PassRefPtr<WebCore::FormState>, WebCore::FramePolicyFunction) override;

    virtual void revertToProvisionalState(WebCore::DocumentLoader*) override;
    virtual bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length) override;

    virtual WebCore::Frame* dispatchCreatePage(const WebCore::NavigationAction&) override;
    virtual void dispatchShow() override;

    virtual void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&) override;
    virtual void setMainFrameDocumentReady(bool) override;

    virtual void startDownload(const WebCore::ResourceRequest&, const String& suggestedName = String()) override;

    virtual void progressStarted(WebCore::Frame&);
    virtual void progressEstimateChanged(WebCore::Frame&);
    virtual void progressFinished(WebCore::Frame&);

    virtual void committedLoad(WebCore::DocumentLoader*, const char*, int) override;
    virtual void finishedLoading(WebCore::DocumentLoader*) override;

    virtual void willChangeTitle(WebCore::DocumentLoader*) override;
    virtual void didChangeTitle(WebCore::DocumentLoader*) override;

    virtual void updateGlobalHistory() override;
    virtual void updateGlobalHistoryRedirectLinks() override;
    virtual bool shouldGoToHistoryItem(WebCore::HistoryItem*) const override;
    virtual void updateGlobalHistoryItemForPage() override;

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

    virtual WTF::String userAgent(const WebCore::URL&) override;

    virtual PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&);
    virtual void setTitle(const WebCore::StringWithDirection&, const WebCore::URL&);

    virtual void savePlatformDataToCachedFrame(WebCore::CachedFrame*) override;
    virtual void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) override;
    virtual void transitionToCommittedForNewPage() override;

    virtual bool canHandleRequest(const WebCore::ResourceRequest&) const override;
    virtual bool canShowMIMEType(const WTF::String& MIMEType) const override;
    virtual bool canShowMIMETypeAsHTML(const WTF::String& MIMEType) const override;
    virtual bool representationExistsForURLScheme(const WTF::String& URLScheme) const override;
    virtual WTF::String generatedMIMETypeForURLScheme(const WTF::String& URLScheme) const override;

    virtual void frameLoadCompleted() override;
    virtual void saveViewStateToItem(WebCore::HistoryItem *) override;
    virtual void restoreViewState() override;
    virtual void provisionalLoadStarted() override;
    virtual void didFinishLoad() override;
    virtual void prepareForDataSourceReplacement() override;

    virtual void didSaveToPageCache() override;
    virtual void didRestoreFromPageCache() override;

    virtual void dispatchDidBecomeFrameset(bool) override;

    virtual bool canCachePage() const;

    virtual PassRefPtr<WebCore::Frame> createFrame(const WebCore::URL& url, const WTF::String& name, WebCore::HTMLFrameOwnerElement* ownerElement,
        const WTF::String& referrer, bool allowsScrolling, int marginWidth, int marginHeight) override;
    virtual PassRefPtr<WebCore::Widget> createPlugin(const WebCore::IntSize&, WebCore::HTMLPlugInElement*, const WebCore::URL&, const Vector<WTF::String>&, const Vector<WTF::String>&, const WTF::String&, bool loadManually) override;
    virtual void recreatePlugin(WebCore::Widget*) override { }
    virtual void redirectDataToPlugin(WebCore::Widget* pluginWidget) override;

    virtual PassRefPtr<WebCore::Widget> createJavaAppletWidget(const WebCore::IntSize&, WebCore::HTMLAppletElement*, const WebCore::URL& baseURL, const Vector<WTF::String>& paramNames, const Vector<WTF::String>& paramValues) override;

    virtual WebCore::ObjectContentType objectContentType(const WebCore::URL&, const WTF::String& mimeType, bool shouldPreferPlugInsForImages) override;
    virtual WTF::String overrideMediaType() const override;

    virtual void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld&) override;

    COMPtr<WebFramePolicyListener> setUpPolicyListener(WebCore::FramePolicyFunction);
    void receivedPolicyDecision(WebCore::PolicyAction);

    virtual void registerForIconNotification(bool listen) override;

    virtual bool shouldAlwaysUsePluginDocument(const WTF::String& mimeType) const;

    virtual void dispatchDidFailToStartPlugin(const WebCore::PluginView*) const;

protected:
    class WebFramePolicyListenerPrivate;
    OwnPtr<WebFramePolicyListenerPrivate> m_policyListenerPrivate;

private:
    PassRefPtr<WebCore::Frame> createFrame(const WebCore::URL&, const WTF::String& name, WebCore::HTMLFrameOwnerElement*, const WTF::String& referrer);
    WebHistory* webHistory() const;

    WebFrame* m_webFrame;

    // Points to the manual loader that data should be redirected to.
    WebCore::PluginManualLoader* m_manualLoader;

    bool m_hasSentResponseToPlugin;
};

#endif // WebFrameLoaderClient_h
