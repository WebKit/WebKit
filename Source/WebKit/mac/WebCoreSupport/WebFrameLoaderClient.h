/*
 * Copyright (C) 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
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

@class WebDownload;
@class WebFrame;
@class WebFramePolicyListener;
@class WebHistoryItem;
@class WebResource;

namespace WebCore {
class AuthenticationChallenge;
class CachedFrame;
class HistoryItem;
class ProtectionSpace;
class ResourceLoader;
class ResourceRequest;
class SessionID;
}

typedef HashMap<RefPtr<WebCore::ResourceLoader>, RetainPtr<WebResource>> ResourceMap;

class WebFrameLoaderClient : public WebCore::FrameLoaderClient {
public:
    WebFrameLoaderClient(WebFrame* = 0);

    void setWebFrame(WebFrame* webFrame) { m_webFrame = webFrame; }
    WebFrame* webFrame() const { return m_webFrame.get(); }

private:
    void frameLoaderDestroyed() override;
    bool hasWebView() const override; // mainly for assertions

    void makeRepresentation(WebCore::DocumentLoader*) override;
    bool hasHTMLView() const override;
#if PLATFORM(IOS)
    bool forceLayoutOnRestoreFromPageCache() override;
#endif
    void forceLayoutForNonHTML() override;

    void setCopiesOnScroll() override;

    void detachedFromParent2() override;
    void detachedFromParent3() override;

    void convertMainResourceLoadToDownload(WebCore::DocumentLoader*, WebCore::SessionID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&) override;

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
#if ENABLE(DATA_DETECTION)
    void dispatchDidFinishDataDetection(NSArray *detectionResults) override;
#endif
    void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceError&) override;

    NSCachedURLResponse* willCacheResponse(WebCore::DocumentLoader*, unsigned long identifier, NSCachedURLResponse*) const override;

    void dispatchDidDispatchOnloadEvents() override;
    void dispatchDidReceiveServerRedirectForProvisionalLoad() override;
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

    WebCore::Frame* dispatchCreatePage(const WebCore::NavigationAction&) override;
    void dispatchShow() override;

    void dispatchDecidePolicyForResponse(const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, WebCore::FramePolicyFunction) override;
    void dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<WebCore::FormState>, const WTF::String& frameName, WebCore::FramePolicyFunction) override;
    void dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<WebCore::FormState>, WebCore::FramePolicyFunction) override;
    void cancelPolicyCheck() override;

    void dispatchUnableToImplementPolicy(const WebCore::ResourceError&) override;

    void dispatchWillSendSubmitEvent(PassRefPtr<WebCore::FormState>) override;
    void dispatchWillSubmitForm(PassRefPtr<WebCore::FormState>, WebCore::FramePolicyFunction) override;

    void revertToProvisionalState(WebCore::DocumentLoader*) override;
    void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&) override;
    bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length) override;

    void setMainFrameDocumentReady(bool) override;

    void startDownload(const WebCore::ResourceRequest&, const String& suggestedName = String()) override;

    void willChangeTitle(WebCore::DocumentLoader*) override;
    void didChangeTitle(WebCore::DocumentLoader*) override;

    void willReplaceMultipartContent() override { }
    void didReplaceMultipartContent() override;

    void committedLoad(WebCore::DocumentLoader*, const char*, int) override;
    void finishedLoading(WebCore::DocumentLoader*) override;
    void updateGlobalHistory() override;
    void updateGlobalHistoryRedirectLinks() override;

    bool shouldGoToHistoryItem(WebCore::HistoryItem*) const override;
    void updateGlobalHistoryItemForPage() override;

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

    WTF::String userAgent(const WebCore::URL&) override;
    
    void savePlatformDataToCachedFrame(WebCore::CachedFrame*) override;
    void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) override;
#if PLATFORM(IOS)
    void didRestoreFrameHierarchyForCachedFrame() override;
#endif
    void transitionToCommittedForNewPage() override;

    void didSaveToPageCache() override;
    void didRestoreFromPageCache() override;

    void dispatchDidBecomeFrameset(bool) override;

    bool canHandleRequest(const WebCore::ResourceRequest&) const override;
    bool canShowMIMEType(const WTF::String& MIMEType) const override;
    bool canShowMIMETypeAsHTML(const WTF::String& MIMEType) const override;
    bool representationExistsForURLScheme(const WTF::String& URLScheme) const override;
    WTF::String generatedMIMETypeForURLScheme(const WTF::String& URLScheme) const override;

    void frameLoadCompleted() override;
    void saveViewStateToItem(WebCore::HistoryItem*) override;
    void restoreViewState() override;
    void provisionalLoadStarted() override;
    void didFinishLoad() override;
    void prepareForDataSourceReplacement() override;
    Ref<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&) override;
    void updateCachedDocumentLoader(WebCore::DocumentLoader&) override { }

    void setTitle(const WebCore::StringWithDirection&, const WebCore::URL&) override;

    RefPtr<WebCore::Frame> createFrame(const WebCore::URL&, const WTF::String& name, WebCore::HTMLFrameOwnerElement*,
        const WTF::String& referrer, bool allowsScrolling, int marginWidth, int marginHeight) override;
    RefPtr<WebCore::Widget> createPlugin(const WebCore::IntSize&, WebCore::HTMLPlugInElement*, const WebCore::URL&, const Vector<WTF::String>&,
        const Vector<WTF::String>&, const WTF::String&, bool) override;
    void recreatePlugin(WebCore::Widget*) override;
    void redirectDataToPlugin(WebCore::Widget* pluginWidget) override;

#if ENABLE(WEBGL)
    WebCore::WebGLLoadPolicy webGLPolicyForURL(const String&) const override;
    WebCore::WebGLLoadPolicy resolveWebGLPolicyForURL(const String&) const override;
#endif // ENABLE(WEBGL)

    PassRefPtr<WebCore::Widget> createJavaAppletWidget(const WebCore::IntSize&, WebCore::HTMLAppletElement*, const WebCore::URL& baseURL,
        const Vector<WTF::String>& paramNames, const Vector<WTF::String>& paramValues) override;
    
    WebCore::ObjectContentType objectContentType(const WebCore::URL&, const WTF::String& mimeType) override;
    WTF::String overrideMediaType() const override;
    
    void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld&) override;

    void registerForIconNotification(bool listen) override;

#if PLATFORM(IOS)
    bool shouldLoadMediaElementURL(const WebCore::URL&) const override;
#endif

    RemoteAXObjectRef accessibilityRemoteObject() override { return 0; }
    
    RetainPtr<WebFramePolicyListener> setUpPolicyListener(WebCore::FramePolicyFunction, NSURL *appLinkURL = nil);

    NSDictionary *actionDictionary(const WebCore::NavigationAction&, PassRefPtr<WebCore::FormState>) const;
    
    bool canCachePage() const override;

    PassRefPtr<WebCore::FrameNetworkingContext> createNetworkingContext() override;

#if ENABLE(REQUEST_AUTOCOMPLETE)
    void didRequestAutocomplete(PassRefPtr<WebCore::FormState>) override { }
#endif

    bool shouldPaintBrokenImage(const WebCore::URL&) const override;

#if USE(QUICK_LOOK)
    void didCreateQuickLookHandle(WebCore::QuickLookHandle&) override;
#endif

#if ENABLE(CONTENT_FILTERING)
    void contentFilterDidBlockLoad(WebCore::ContentFilterUnblockHandler) override;
#endif

    void prefetchDNS(const String&) override;

    RetainPtr<WebFrame> m_webFrame;

    RetainPtr<WebFramePolicyListener> m_policyListener;
};
