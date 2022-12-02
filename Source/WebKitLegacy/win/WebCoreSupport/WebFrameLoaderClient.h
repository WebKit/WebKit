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

class WebFrame;
class WebFramePolicyListener;
class WebHistory;

class WebFrameLoaderClient : public WebCore::FrameLoaderClient {
public:
    WebFrameLoaderClient(WebFrame* = nullptr);
    ~WebFrameLoaderClient();

    void setWebFrame(WebFrame* webFrame) { m_webFrame = webFrame; }
    WebFrame* webFrame() const { return m_webFrame; }

    std::optional<WebCore::PageIdentifier> pageID() const final;

    bool hasWebView() const override;

    Ref<WebCore::FrameNetworkingContext> createNetworkingContext() override;

    void makeRepresentation(WebCore::DocumentLoader*) override;
    void forceLayoutForNonHTML() override;

    void setCopiesOnScroll() override;

    void detachedFromParent2() override;
    void detachedFromParent3() override;

    void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&) override;
    void assignIdentifierToInitialRequest(WebCore::ResourceLoaderIdentifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&) override;

    void dispatchWillSendRequest(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse) override;
    bool shouldUseCredentialStorage(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier) override;
    void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, const WebCore::AuthenticationChallenge&) override;
    void dispatchDidReceiveResponse(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, const WebCore::ResourceResponse&) override;
    void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, int dataLength) override;
    void dispatchDidFinishLoading(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier) override;
    void dispatchDidFailLoading(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, const WebCore::ResourceError&) override;
#if USE(CFURLCONNECTION)
    bool shouldCacheResponse(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, const WebCore::ResourceResponse&, const unsigned char* data, unsigned long long length) override;
#endif

    void dispatchDidDispatchOnloadEvents() override;
    void dispatchDidReceiveServerRedirectForProvisionalLoad() override;
    void dispatchDidCancelClientRedirect() override;
    void dispatchWillPerformClientRedirect(const URL&, double interval, WallTime fireDate, WebCore::LockBackForwardList) override;
    void dispatchDidChangeLocationWithinPage() override;
    void dispatchDidPushStateWithinPage() override;
    void dispatchDidReplaceStateWithinPage() override;
    void dispatchDidPopStateWithinPage() override;
    void dispatchWillClose() override;
    void dispatchDidStartProvisionalLoad() override;
    void dispatchDidReceiveTitle(const WebCore::StringWithDirection&) override;
    void dispatchDidCommitLoad(std::optional<WebCore::HasInsecureContent>, std::optional<WebCore::UsedLegacyTLS>, std::optional<WebCore::WasPrivateRelayed>) override;
    void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&, WebCore::WillContinueLoading) override;
    void dispatchDidFailLoad(const WebCore::ResourceError&) override;
    void dispatchDidFinishDocumentLoad() override;
    void dispatchDidFinishLoad() override;
    void dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone>) override;

    void dispatchDecidePolicyForResponse(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, WebCore::PolicyCheckIdentifier, const String&, WebCore::FramePolicyFunction&&) override;
    void dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, WebCore::FormState*, const WTF::String& frameName, WebCore::PolicyCheckIdentifier, WebCore::FramePolicyFunction&&) override;
    void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse, WebCore::FormState*, WebCore::PolicyDecisionMode, WebCore::PolicyCheckIdentifier, WebCore::FramePolicyFunction&&) override;
    void cancelPolicyCheck() override;

    void dispatchUnableToImplementPolicy(const WebCore::ResourceError&) override;

    void dispatchWillSendSubmitEvent(Ref<WebCore::FormState>&&) override;
    void dispatchWillSubmitForm(WebCore::FormState&, CompletionHandler<void()>&&) override;

    void revertToProvisionalState(WebCore::DocumentLoader*) override;
    bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length) override;

    WebCore::Frame* dispatchCreatePage(const WebCore::NavigationAction&, WebCore::NewFrameOpenerPolicy) override;
    void dispatchShow() override;

    void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&) override;
    void setMainFrameDocumentReady(bool) override;

    void startDownload(const WebCore::ResourceRequest&, const String& suggestedName = String()) override;

    void committedLoad(WebCore::DocumentLoader*, const WebCore::SharedBuffer&) override;
    void finishedLoading(WebCore::DocumentLoader*) override;

    void willChangeTitle(WebCore::DocumentLoader*) override;
    void didChangeTitle(WebCore::DocumentLoader*) override;

    void willReplaceMultipartContent() override { }
    void didReplaceMultipartContent() override { }

    void updateGlobalHistory() override;
    void updateGlobalHistoryRedirectLinks() override;
    bool shouldGoToHistoryItem(WebCore::HistoryItem&) const override;

    void didDisplayInsecureContent() override;
    void didRunInsecureContent(WebCore::SecurityOrigin&, const URL&) override;

    WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&) const override;
    WebCore::ResourceError blockedError(const WebCore::ResourceRequest&) const override;
    WebCore::ResourceError blockedByContentBlockerError(const WebCore::ResourceRequest&) const override;
    WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&) const override;
    WebCore::ResourceError interruptedForPolicyChangeError(const WebCore::ResourceRequest&) const override;
    WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&) const override;
    WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&) const override;
    WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&) const override;

    bool shouldFallBack(const WebCore::ResourceError&) const override;

    WTF::String userAgent(const URL&) const override;

    Ref<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&) override;
    void updateCachedDocumentLoader(WebCore::DocumentLoader&) override { }

    void setTitle(const WebCore::StringWithDirection&, const URL&) override;

    void savePlatformDataToCachedFrame(WebCore::CachedFrame*) override;
    void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) override;
    void transitionToCommittedForNewPage() override;

    bool canHandleRequest(const WebCore::ResourceRequest&) const override;
    bool canShowMIMEType(const WTF::String& MIMEType) const override;
    bool canShowMIMETypeAsHTML(const WTF::String& MIMEType) const override;
    bool representationExistsForURLScheme(WTF::StringView URLScheme) const override;
    WTF::String generatedMIMETypeForURLScheme(WTF::StringView URLScheme) const override;

    void frameLoadCompleted() override;
    void saveViewStateToItem(WebCore::HistoryItem&) override;
    void restoreViewState() override;
    void provisionalLoadStarted() override;
    void didFinishLoad() override;
    void prepareForDataSourceReplacement() override;

    void didRestoreFromBackForwardCache() override;

    bool canCachePage() const override;

    RefPtr<WebCore::Frame> createFrame(const WTF::AtomString& name, WebCore::HTMLFrameOwnerElement&) override;
    RefPtr<WebCore::Widget> createPlugin(const WebCore::IntSize&, WebCore::HTMLPlugInElement&, const URL&, const Vector<WTF::AtomString>&, const Vector<WTF::AtomString>&, const WTF::String&, bool loadManually) override;
    void redirectDataToPlugin(WebCore::Widget&) override;

    WebCore::ObjectContentType objectContentType(const URL&, const WTF::String& mimeType) override;
    WTF::AtomString overrideMediaType() const override;

    void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld&) override;

    COMPtr<WebFramePolicyListener> setUpPolicyListener(WebCore::PolicyCheckIdentifier, WebCore::FramePolicyFunction&&);
    void receivedPolicyDecision(WebCore::PolicyAction);

    bool shouldAlwaysUsePluginDocument(const WTF::String& mimeType) const override;

    void prefetchDNS(const String&) override;
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>&&)>&&) final;

private:
    WebHistory* webHistory() const;

    class WebFramePolicyListenerPrivate;
    std::unique_ptr<WebFramePolicyListenerPrivate> m_policyListenerPrivate;

    WebFrame* m_webFrame;
};
