/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#pragma once

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

    void dispatchDidFailToStartPlugin(const WebCore::PluginView*) const;

    std::optional<uint64_t> pageID() const final;
    std::optional<uint64_t> frameID() const final;
    PAL::SessionID sessionID() const final;

    bool hasWebView() const override;

    Ref<WebCore::FrameNetworkingContext> createNetworkingContext() override;

    void frameLoaderDestroyed() override;
    void makeRepresentation(WebCore::DocumentLoader*) override;
    void forceLayoutForNonHTML() override;

    void setCopiesOnScroll() override;

    void detachedFromParent2() override;
    void detachedFromParent3() override;

    void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, PAL::SessionID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&) override;
    void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&) override;

    void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse) override;
    bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long identifier) override;
    void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&) override;
    void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&) override;
    void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int dataLength) override;
    void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier) override;
    void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceError&) override;
#if USE(CFURLCONNECTION)
    bool shouldCacheResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&, const unsigned char* data, unsigned long long length) override;
#endif

    void dispatchDidDispatchOnloadEvents() override;
    void dispatchDidReceiveServerRedirectForProvisionalLoad() override;
    void dispatchDidCancelClientRedirect() override;
    void dispatchWillPerformClientRedirect(const WebCore::URL&, double interval, WallTime fireDate) override;
    void dispatchDidChangeLocationWithinPage() override;
    void dispatchDidPushStateWithinPage() override;
    void dispatchDidReplaceStateWithinPage() override;
    void dispatchDidPopStateWithinPage() override;
    void dispatchWillClose() override;
    void dispatchDidStartProvisionalLoad() override;
    void dispatchDidReceiveTitle(const WebCore::StringWithDirection&) override;
    void dispatchDidCommitLoad(std::optional<WebCore::HasInsecureContent>) override;
    void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&) override;
    void dispatchDidFailLoad(const WebCore::ResourceError&) override;
    void dispatchDidFinishDocumentLoad() override;
    void dispatchDidFinishLoad() override;
    void dispatchDidReachLayoutMilestone(WebCore::LayoutMilestones) override;

    void dispatchDecidePolicyForResponse(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, WebCore::FramePolicyFunction&&) override;
    void dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, WebCore::FormState*, const WTF::String& frameName, WebCore::FramePolicyFunction&&) override;
    void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse, WebCore::FormState*, WebCore::PolicyDecisionMode, WebCore::ShouldSkipSafeBrowsingCheck, WebCore::FramePolicyFunction&&) override;
    void cancelPolicyCheck() override;

    void dispatchUnableToImplementPolicy(const WebCore::ResourceError&) override;

    void dispatchWillSendSubmitEvent(Ref<WebCore::FormState>&&) override;
    void dispatchWillSubmitForm(WebCore::FormState&, CompletionHandler<void()>&&) override;

    void revertToProvisionalState(WebCore::DocumentLoader*) override;
    bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length) override;

    WebCore::Frame* dispatchCreatePage(const WebCore::NavigationAction&) override;
    void dispatchShow() override;

    void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&) override;
    void setMainFrameDocumentReady(bool) override;

    void startDownload(const WebCore::ResourceRequest&, const String& suggestedName = String()) override;

    void progressStarted(WebCore::Frame&) override;
    void progressEstimateChanged(WebCore::Frame&) override;
    void progressFinished(WebCore::Frame&) override;

    void committedLoad(WebCore::DocumentLoader*, const char*, int) override;
    void finishedLoading(WebCore::DocumentLoader*) override;

    void willChangeTitle(WebCore::DocumentLoader*) override;
    void didChangeTitle(WebCore::DocumentLoader*) override;

    void willReplaceMultipartContent() override { }
    void didReplaceMultipartContent() override { }

    void updateGlobalHistory() override;
    void updateGlobalHistoryRedirectLinks() override;
    bool shouldGoToHistoryItem(WebCore::HistoryItem&) const override;

    void didDisplayInsecureContent() override;
    void didRunInsecureContent(WebCore::SecurityOrigin&, const WebCore::URL&) override;
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

    WTF::String userAgent(const WebCore::URL&) override;

    Ref<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&) override;
    void updateCachedDocumentLoader(WebCore::DocumentLoader&) override { }

    void setTitle(const WebCore::StringWithDirection&, const WebCore::URL&) override;

    void savePlatformDataToCachedFrame(WebCore::CachedFrame*) override;
    void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) override;
    void transitionToCommittedForNewPage() override;

    bool canHandleRequest(const WebCore::ResourceRequest&) const override;
    bool canShowMIMEType(const WTF::String& MIMEType) const override;
    bool canShowMIMETypeAsHTML(const WTF::String& MIMEType) const override;
    bool representationExistsForURLScheme(const WTF::String& URLScheme) const override;
    WTF::String generatedMIMETypeForURLScheme(const WTF::String& URLScheme) const override;

    void frameLoadCompleted() override;
    void saveViewStateToItem(WebCore::HistoryItem&) override;
    void restoreViewState() override;
    void provisionalLoadStarted() override;
    void didFinishLoad() override;
    void prepareForDataSourceReplacement() override;

    void didSaveToPageCache() override;
    void didRestoreFromPageCache() override;

    void dispatchDidBecomeFrameset(bool) override;

    bool canCachePage() const override;

    RefPtr<WebCore::Frame> createFrame(const WebCore::URL&, const WTF::String& name, WebCore::HTMLFrameOwnerElement&,
        const WTF::String& referrer) override;
    RefPtr<WebCore::Widget> createPlugin(const WebCore::IntSize&, WebCore::HTMLPlugInElement&, const WebCore::URL&, const Vector<WTF::String>&, const Vector<WTF::String>&, const WTF::String&, bool loadManually) override;
    void recreatePlugin(WebCore::Widget*) override { }
    void redirectDataToPlugin(WebCore::Widget&) override;

    RefPtr<WebCore::Widget> createJavaAppletWidget(const WebCore::IntSize&, WebCore::HTMLAppletElement&, const WebCore::URL& baseURL, const Vector<WTF::String>& paramNames, const Vector<WTF::String>& paramValues) override;

    WebCore::ObjectContentType objectContentType(const WebCore::URL&, const WTF::String& mimeType) override;
    WTF::String overrideMediaType() const override;

    void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld&) override;

    COMPtr<WebFramePolicyListener> setUpPolicyListener(WebCore::FramePolicyFunction&&);
    void receivedPolicyDecision(WebCore::PolicyAction);

    bool shouldAlwaysUsePluginDocument(const WTF::String& mimeType) const override;

    void prefetchDNS(const String&) override;

private:
    WebHistory* webHistory() const;

    class WebFramePolicyListenerPrivate;
    std::unique_ptr<WebFramePolicyListenerPrivate> m_policyListenerPrivate;

    WebFrame* m_webFrame;

    // Points to the manual loader that data should be redirected to.
    WebCore::PluginManualLoader* m_manualLoader;

    bool m_hasSentResponseToPlugin;
};
