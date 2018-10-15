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

#import <WebCore/FrameLoaderClient.h>
#import <WebCore/Timer.h>
#import <wtf/Forward.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

@class WebDownload;
@class WebFrame;
@class WebFramePolicyListener;
@class WebHistoryItem;
@class WebResource;

namespace PAL {
class SessionID;
}

namespace WebCore {
class AuthenticationChallenge;
class CachedFrame;
class HistoryItem;
class ProtectionSpace;
class ResourceLoader;
class ResourceRequest;
}

class WebFrameLoaderClient : public WebCore::FrameLoaderClient {
public:
    WebFrameLoaderClient(WebFrame* = 0);

    void setWebFrame(WebFrame* webFrame) { m_webFrame = webFrame; }
    WebFrame* webFrame() const { return m_webFrame.get(); }

private:
    void frameLoaderDestroyed() final;
    bool hasWebView() const final; // mainly for assertions

    std::optional<uint64_t> pageID() const final;
    std::optional<uint64_t> frameID() const final;
    PAL::SessionID sessionID() const final;

    void makeRepresentation(WebCore::DocumentLoader*) final;
    bool hasHTMLView() const final;
#if PLATFORM(IOS)
    bool forceLayoutOnRestoreFromPageCache() final;
#endif
    void forceLayoutForNonHTML() final;

    void setCopiesOnScroll() final;

    void detachedFromParent2() final;
    void detachedFromParent3() final;

    void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, PAL::SessionID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&) final;

    void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&) final;

    void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse) final;
    bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long identifier) final;
    void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&) final;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    bool canAuthenticateAgainstProtectionSpace(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ProtectionSpace&) final;
#endif

#if PLATFORM(IOS)
    RetainPtr<CFDictionaryRef> connectionProperties(WebCore::DocumentLoader*, unsigned long identifier) final;
#endif

    void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&) final;
    void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int dataLength) final;
    void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier) final;
#if ENABLE(DATA_DETECTION)
    void dispatchDidFinishDataDetection(NSArray *detectionResults) final;
#endif
    void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceError&) final;

    void willCacheResponse(WebCore::DocumentLoader*, unsigned long identifier, NSCachedURLResponse*, CompletionHandler<void(NSCachedURLResponse *)>&&) const final;

    void dispatchDidDispatchOnloadEvents() final;
    void dispatchDidReceiveServerRedirectForProvisionalLoad() final;
    void dispatchDidCancelClientRedirect() final;
    void dispatchWillPerformClientRedirect(const WebCore::URL&, double interval, WallTime fireDate) final;
    void dispatchDidChangeLocationWithinPage() final;
    void dispatchDidPushStateWithinPage() final;
    void dispatchDidReplaceStateWithinPage() final;
    void dispatchDidPopStateWithinPage() final;
    
    void dispatchWillClose() final;
    void dispatchDidStartProvisionalLoad() final;
    void dispatchDidReceiveTitle(const WebCore::StringWithDirection&) final;
    void dispatchDidCommitLoad(std::optional<WebCore::HasInsecureContent>) final;
    void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&) final;
    void dispatchDidFailLoad(const WebCore::ResourceError&) final;
    void dispatchDidFinishDocumentLoad() final;
    void dispatchDidFinishLoad() final;
    void dispatchDidReachLayoutMilestone(WebCore::LayoutMilestones) final;

    WebCore::Frame* dispatchCreatePage(const WebCore::NavigationAction&) final;
    void dispatchShow() final;

    void dispatchDecidePolicyForResponse(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, WebCore::FramePolicyFunction&&) final;
    void dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, WebCore::FormState*, const WTF::String& frameName, WebCore::FramePolicyFunction&&) final;
    void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse, WebCore::FormState*, WebCore::PolicyDecisionMode, WebCore::ShouldSkipSafeBrowsingCheck, WebCore::FramePolicyFunction&&) final;
    void cancelPolicyCheck() final;

    void dispatchUnableToImplementPolicy(const WebCore::ResourceError&) final;

    void dispatchWillSendSubmitEvent(Ref<WebCore::FormState>&&) final;
    void dispatchWillSubmitForm(WebCore::FormState&, CompletionHandler<void()>&&) final;

    void revertToProvisionalState(WebCore::DocumentLoader*) final;
    void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&) final;
    bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length) final;

    void setMainFrameDocumentReady(bool) final;

    void startDownload(const WebCore::ResourceRequest&, const String& suggestedName = String()) final;

    void willChangeTitle(WebCore::DocumentLoader*) final;
    void didChangeTitle(WebCore::DocumentLoader*) final;

    void willReplaceMultipartContent() final { }
    void didReplaceMultipartContent() final;

    void committedLoad(WebCore::DocumentLoader*, const char*, int) final;
    void finishedLoading(WebCore::DocumentLoader*) final;
    void updateGlobalHistory() final;
    void updateGlobalHistoryRedirectLinks() final;

    bool shouldGoToHistoryItem(WebCore::HistoryItem&) const final;

    void didDisplayInsecureContent() final;
    void didRunInsecureContent(WebCore::SecurityOrigin&, const WebCore::URL&) final;
    void didDetectXSS(const WebCore::URL&, bool didBlockEntirePage) final;

    WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&) final;
    WebCore::ResourceError blockedError(const WebCore::ResourceRequest&) final;
    WebCore::ResourceError blockedByContentBlockerError(const WebCore::ResourceRequest&) final;
    WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&) final;
    WebCore::ResourceError interruptedForPolicyChangeError(const WebCore::ResourceRequest&) final;
#if ENABLE(CONTENT_FILTERING)
    WebCore::ResourceError blockedByContentFilterError(const WebCore::ResourceRequest&) final;
#endif

    WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&) final;
    WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&) final;
    WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&) final;

    bool shouldFallBack(const WebCore::ResourceError&) final;

    WTF::String userAgent(const WebCore::URL&) final;
    
    void savePlatformDataToCachedFrame(WebCore::CachedFrame*) final;
    void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) final;
#if PLATFORM(IOS)
    void didRestoreFrameHierarchyForCachedFrame() final;
#endif
    void transitionToCommittedForNewPage() final;

    void didSaveToPageCache() final;
    void didRestoreFromPageCache() final;

    void dispatchDidBecomeFrameset(bool) final;

    bool canHandleRequest(const WebCore::ResourceRequest&) const final;
    bool canShowMIMEType(const WTF::String& MIMEType) const final;
    bool canShowMIMETypeAsHTML(const WTF::String& MIMEType) const final;
    bool representationExistsForURLScheme(const WTF::String& URLScheme) const final;
    WTF::String generatedMIMETypeForURLScheme(const WTF::String& URLScheme) const final;

    void frameLoadCompleted() final;
    void saveViewStateToItem(WebCore::HistoryItem&) final;
    void restoreViewState() final;
    void provisionalLoadStarted() final;
    void didFinishLoad() final;
    void prepareForDataSourceReplacement() final;
    Ref<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&) final;
    void updateCachedDocumentLoader(WebCore::DocumentLoader&) final { }

    void setTitle(const WebCore::StringWithDirection&, const WebCore::URL&) final;

    RefPtr<WebCore::Frame> createFrame(const WebCore::URL&, const WTF::String& name, WebCore::HTMLFrameOwnerElement&,
        const WTF::String& referrer) final;
    RefPtr<WebCore::Widget> createPlugin(const WebCore::IntSize&, WebCore::HTMLPlugInElement&, const WebCore::URL&,
        const Vector<WTF::String>&, const Vector<WTF::String>&, const WTF::String&, bool) final;
    void recreatePlugin(WebCore::Widget*) final;
    void redirectDataToPlugin(WebCore::Widget&) final;

#if ENABLE(WEBGL)
    WebCore::WebGLLoadPolicy webGLPolicyForURL(const WebCore::URL&) const final;
    WebCore::WebGLLoadPolicy resolveWebGLPolicyForURL(const WebCore::URL&) const final;
#endif

    RefPtr<WebCore::Widget> createJavaAppletWidget(const WebCore::IntSize&, WebCore::HTMLAppletElement&, const WebCore::URL& baseURL,
        const Vector<WTF::String>& paramNames, const Vector<WTF::String>& paramValues) final;
    
    WebCore::ObjectContentType objectContentType(const WebCore::URL&, const WTF::String& mimeType) final;
    WTF::String overrideMediaType() const final;
    
    void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld&) final;

#if PLATFORM(IOS)
    bool shouldLoadMediaElementURL(const WebCore::URL&) const final;
#endif

    RemoteAXObjectRef accessibilityRemoteObject() final { return 0; }
    
    RetainPtr<WebFramePolicyListener> setUpPolicyListener(WebCore::FramePolicyFunction&&, WebCore::PolicyAction defaultPolicy, NSURL *appLinkURL = nil);

    NSDictionary *actionDictionary(const WebCore::NavigationAction&, WebCore::FormState*) const;
    
    bool canCachePage() const final;

    Ref<WebCore::FrameNetworkingContext> createNetworkingContext() final;

    bool shouldPaintBrokenImage(const WebCore::URL&) const final;

#if USE(QUICK_LOOK)
    RefPtr<WebCore::PreviewLoaderClient> createPreviewLoaderClient(const String& fileName, const String& uti) final;
#endif

#if ENABLE(CONTENT_FILTERING)
    void contentFilterDidBlockLoad(WebCore::ContentFilterUnblockHandler) final;
#endif

    void prefetchDNS(const String&) final;

    void getLoadDecisionForIcons(const Vector<std::pair<WebCore::LinkIcon&, uint64_t>>&) final;
    void finishedLoadingIcon(uint64_t, WebCore::SharedBuffer*) final;

    uint64_t m_activeIconLoadCallbackID { 0 };

    RetainPtr<WebFrame> m_webFrame;

    WeakObjCPtr<WebFramePolicyListener> m_policyListener;
};
