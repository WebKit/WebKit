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

#import <WebCore/LocalFrameLoaderClient.h>
#import <WebCore/Timer.h>
#import <wtf/Forward.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WeakPtr.h>

@class WebDataSource;
@class WebDownload;
@class WebFrame;
@class WebFramePolicyListener;
@class WebHistoryItem;
@class WebResource;

class WebFrameLoaderClient;

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebFrameLoaderClient> : std::true_type { };
}

namespace PAL {
class SessionID;
}

namespace WebCore {
class AuthenticationChallenge;
class CachedFrame;
class FragmentedSharedBuffer;
class HistoryItem;
class LocalFrame;
class ProtectionSpace;
class ResourceLoader;
class ResourceRequest;
class SharedBuffer;
}

class WebFrameLoaderClient : public WebCore::LocalFrameLoaderClient, public CanMakeWeakPtr<WebFrameLoaderClient> {
public:
    explicit WebFrameLoaderClient(WebCore::FrameLoader&, WebFrame* = nullptr);
    ~WebFrameLoaderClient();

    void setWebFrame(WebFrame& webFrame) { m_webFrame = &webFrame; }
    WebFrame* webFrame() const { return m_webFrame.get(); }

private:
    bool hasWebView() const final; // mainly for assertions

    void makeRepresentation(WebCore::DocumentLoader*) final;
    bool hasHTMLView() const final;
#if PLATFORM(IOS_FAMILY)
    bool forceLayoutOnRestoreFromBackForwardCache() final;
#endif
    void forceLayoutForNonHTML() final;

    void setCopiesOnScroll() final;

    void detachedFromParent2() final;
    void detachedFromParent3() final;

    void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&) final;

    void assignIdentifierToInitialRequest(WebCore::ResourceLoaderIdentifier, WebCore::IsMainResourceLoad, WebCore::DocumentLoader*, const WebCore::ResourceRequest&) final;

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
    void dispatchDidFinishLoading(WebCore::DocumentLoader*, WebCore::IsMainResourceLoad, WebCore::ResourceLoaderIdentifier) final;
#if ENABLE(DATA_DETECTION)
    void dispatchDidFinishDataDetection(NSArray *detectionResults) final;
#endif
    void dispatchDidFailLoading(WebCore::DocumentLoader*, WebCore::IsMainResourceLoad, WebCore::ResourceLoaderIdentifier, const WebCore::ResourceError&) final;

    void willCacheResponse(WebCore::DocumentLoader*, WebCore::ResourceLoaderIdentifier, NSCachedURLResponse*, CompletionHandler<void(NSCachedURLResponse *)>&&) const final;

    void dispatchDidDispatchOnloadEvents() final;
    void dispatchDidReceiveServerRedirectForProvisionalLoad() final;
    void dispatchDidCancelClientRedirect() final;
    void dispatchWillPerformClientRedirect(const URL&, double interval, WallTime fireDate, WebCore::LockBackForwardList) final;
    void dispatchDidChangeLocationWithinPage() final;
    void dispatchDidPushStateWithinPage() final;
    void dispatchDidReplaceStateWithinPage() final;
    void dispatchDidPopStateWithinPage() final;
    
    void dispatchWillClose() final;
    void dispatchDidStartProvisionalLoad() final;
    void dispatchDidReceiveTitle(const WebCore::StringWithDirection&) final;
    void dispatchDidCommitLoad(std::optional<WebCore::HasInsecureContent>, std::optional<WebCore::UsedLegacyTLS>, std::optional<WebCore::WasPrivateRelayed>) final;
    void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&, WebCore::WillContinueLoading, WebCore::WillInternallyHandleFailure) final;
    void dispatchDidFailLoad(const WebCore::ResourceError&) final;
    void dispatchDidFinishDocumentLoad() final;
    void dispatchDidFinishLoad() final;
    void dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone>) final;

    WebCore::LocalFrame* dispatchCreatePage(const WebCore::NavigationAction&, WebCore::NewFrameOpenerPolicy) final;
    void dispatchShow() final;

    void dispatchDecidePolicyForResponse(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, const String&, WebCore::FramePolicyFunction&&) final;
    void dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, WebCore::FormState*, const WTF::String& frameName, std::optional<WebCore::HitTestResult>&&, WebCore::FramePolicyFunction&&) final;
    void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse, WebCore::FormState*, const String&, std::optional<WebCore::NavigationIdentifier>, std::optional<WebCore::HitTestResult>&&, bool, WebCore::IsPerformingHTTPFallback, WebCore::SandboxFlags, WebCore::PolicyDecisionMode, WebCore::FramePolicyFunction&&) final;
    void updateSandboxFlags(WebCore::SandboxFlags) { }
    void updateOpener(const WebCore::Frame&) { }
    void cancelPolicyCheck() final;

    void dispatchUnableToImplementPolicy(const WebCore::ResourceError&) final;

    void dispatchWillSendSubmitEvent(Ref<WebCore::FormState>&&) final;
    void dispatchWillSubmitForm(WebCore::FormState&, CompletionHandler<void()>&&) final;

    void revertToProvisionalState(WebCore::DocumentLoader*) final;
    void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&) final;
    bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length) final;

    void setMainFrameDocumentReady(bool) final;

    void startDownload(const WebCore::ResourceRequest&, const String& suggestedName = String(), WebCore::FromDownloadAttribute = WebCore::FromDownloadAttribute::No) final;

    void willChangeTitle(WebCore::DocumentLoader*) final;
    void didChangeTitle(WebCore::DocumentLoader*) final;

    void willReplaceMultipartContent() final { }
    void didReplaceMultipartContent() final;

    void committedLoad(WebCore::DocumentLoader*, const WebCore::SharedBuffer&) final;
    void finishedLoading(WebCore::DocumentLoader*) final;
    void updateGlobalHistory() final;
    void updateGlobalHistoryRedirectLinks() final;

    bool shouldGoToHistoryItem(WebCore::HistoryItem&) const final;

    void didDisplayInsecureContent() final;
    void didRunInsecureContent(WebCore::SecurityOrigin&) final;

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
    WebCore::ResourceError httpsUpgradeRedirectLoopError(const WebCore::ResourceRequest&) const final;
    WebCore::ResourceError httpNavigationWithHTTPSOnlyError(const WebCore::ResourceRequest&) const final;
    WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&) const final;

    void loadStorageAccessQuirksIfNeeded() final { }

    bool shouldFallBack(const WebCore::ResourceError&) const final;

    WTF::String userAgent(const URL&) const final;
    
    void savePlatformDataToCachedFrame(WebCore::CachedFrame*) final;
    void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) final;
#if PLATFORM(IOS_FAMILY)
    void didRestoreFrameHierarchyForCachedFrame() final;
#endif
    void transitionToCommittedForNewPage(InitializingIframe) final;

    void didRestoreFromBackForwardCache() final;

    bool canHandleRequest(const WebCore::ResourceRequest&) const final;
    bool canShowMIMEType(const WTF::String& MIMEType) const final;
    bool canShowMIMETypeAsHTML(const WTF::String& MIMEType) const final;
    bool representationExistsForURLScheme(WTF::StringView URLScheme) const final;
    WTF::String generatedMIMETypeForURLScheme(WTF::StringView URLScheme) const final;

    void frameLoadCompleted() final;
    void saveViewStateToItem(WebCore::HistoryItem&) final;
    void restoreViewState() final;
    void provisionalLoadStarted() final;
    void didFinishLoad() final;
    void prepareForDataSourceReplacement() final;
    Ref<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&) final;
    void updateCachedDocumentLoader(WebCore::DocumentLoader&) final { }

    void setTitle(const WebCore::StringWithDirection&, const URL&) final;

    RefPtr<WebCore::LocalFrame> createFrame(const WTF::AtomString& name, WebCore::HTMLFrameOwnerElement&) final;
    RefPtr<WebCore::Widget> createPlugin(WebCore::HTMLPlugInElement&, const URL&,
    const Vector<WTF::AtomString>&, const Vector<WTF::AtomString>&, const WTF::String&, bool) final;
    void redirectDataToPlugin(WebCore::Widget&) final;

    WebCore::ObjectContentType objectContentType(const URL&, const WTF::String& mimeType) final;
    WTF::AtomString overrideMediaType() const final;
    
    void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld&) final;

#if PLATFORM(IOS_FAMILY)
    bool shouldLoadMediaElementURL(const URL&) const final;
#endif

    RemoteAXObjectRef accessibilityRemoteObject() final { return 0; }
    WebCore::IntPoint accessibilityRemoteFrameOffset() final { return { }; }
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void setAXIsolatedTreeRoot(WebCore::AXCoreObject*) final { }
#endif

    RetainPtr<WebFramePolicyListener> setUpPolicyListener(WebCore::FramePolicyFunction&&, WebCore::PolicyAction defaultPolicy, NSURL *appLinkURL, NSURL* referrerURL);

    NSDictionary *actionDictionary(const WebCore::NavigationAction&, WebCore::FormState*) const;
    
    bool canCachePage() const final;

    Ref<WebCore::FrameNetworkingContext> createNetworkingContext() final;

    bool shouldPaintBrokenImage(const URL&) const final;

#if USE(QUICK_LOOK)
    RefPtr<WebCore::LegacyPreviewLoaderClient> createPreviewLoaderClient(const String& fileName, const String& uti) final;
#endif

#if ENABLE(CONTENT_FILTERING)
    void contentFilterDidBlockLoad(WebCore::ContentFilterUnblockHandler) final;
#endif

    void prefetchDNS(const String&) final;
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>&&)>&&) final;

    void getLoadDecisionForIcons(const Vector<std::pair<WebCore::LinkIcon&, uint64_t>>&) final;
    void finishedLoadingIcon(WebCore::FragmentedSharedBuffer*);

    void dispatchLoadEventToOwnerElementInAnotherProcess() final { };

#if !PLATFORM(IOS_FAMILY)
    bool m_loadingIcon { false };
#endif

    RetainPtr<WebFrame> m_webFrame;

    WeakObjCPtr<WebFramePolicyListener> m_policyListener;
};

WebDataSource *dataSource(WebCore::DocumentLoader*);
void addTypesFromClass(NSMutableDictionary *, Class, NSArray *);
