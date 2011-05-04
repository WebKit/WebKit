/*
 * Copyright (C) 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
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
}

typedef HashMap<RefPtr<WebCore::ResourceLoader>, RetainPtr<WebResource> > ResourceMap;

class WebFrameLoaderClient : public WebCore::FrameLoaderClient {
public:
    WebFrameLoaderClient(WebFrame*);

    WebFrame* webFrame() const { return m_webFrame.get(); }

    virtual void frameLoaderDestroyed();
    void receivedPolicyDecison(WebCore::PolicyAction);

private:
    virtual bool hasWebView() const; // mainly for assertions

    virtual void makeRepresentation(WebCore::DocumentLoader*);
    virtual bool hasHTMLView() const;
    virtual void forceLayout();
    virtual void forceLayoutForNonHTML();

    virtual void setCopiesOnScroll();

    virtual void detachedFromParent2();
    virtual void detachedFromParent3();

    virtual void download(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);

    virtual void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&);

    virtual void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse);
    virtual bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long identifier);
    virtual void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&);
    virtual void dispatchDidCancelAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&);
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual bool canAuthenticateAgainstProtectionSpace(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ProtectionSpace&);
#endif
    virtual void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&);
    virtual void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int dataLength);
    virtual void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier);
    virtual void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceError&);

    virtual NSCachedURLResponse* willCacheResponse(WebCore::DocumentLoader*, unsigned long identifier, NSCachedURLResponse*) const;

    virtual void dispatchDidHandleOnloadEvents();
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad();
    virtual void dispatchDidCancelClientRedirect();
    virtual void dispatchWillPerformClientRedirect(const WebCore::KURL&, double interval, double fireDate);
    virtual void dispatchDidChangeLocationWithinPage();
    virtual void dispatchDidPushStateWithinPage();
    virtual void dispatchDidReplaceStateWithinPage();
    virtual void dispatchDidPopStateWithinPage();
    
    virtual void dispatchWillClose();
    virtual void dispatchDidReceiveIcon();
    virtual void dispatchDidStartProvisionalLoad();
    virtual void dispatchDidReceiveTitle(const WebCore::StringWithDirection&);
    virtual void dispatchDidChangeIcons(WebCore::IconType);
    virtual void dispatchDidCommitLoad();
    virtual void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&);
    virtual void dispatchDidFailLoad(const WebCore::ResourceError&);
    virtual void dispatchDidFinishDocumentLoad();
    virtual void dispatchDidFinishLoad();
    virtual void dispatchDidFirstLayout();
    virtual void dispatchDidFirstVisuallyNonEmptyLayout();

    virtual WebCore::Frame* dispatchCreatePage(const WebCore::NavigationAction&);
    virtual void dispatchShow();

    virtual void dispatchDecidePolicyForResponse(WebCore::FramePolicyFunction, 
        const WebCore::ResourceResponse&, const WebCore::ResourceRequest&);
    virtual void dispatchDecidePolicyForNewWindowAction(WebCore::FramePolicyFunction,
        const WebCore::NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<WebCore::FormState>, const WTF::String& frameName);
    virtual void dispatchDecidePolicyForNavigationAction(WebCore::FramePolicyFunction,
        const WebCore::NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<WebCore::FormState>);
    virtual void cancelPolicyCheck();

    virtual void dispatchUnableToImplementPolicy(const WebCore::ResourceError&);

    virtual void dispatchWillSendSubmitEvent(WebCore::HTMLFormElement*) { }
    virtual void dispatchWillSubmitForm(WebCore::FramePolicyFunction, PassRefPtr<WebCore::FormState>);

    virtual void dispatchDidLoadMainResource(WebCore::DocumentLoader*);
    virtual void revertToProvisionalState(WebCore::DocumentLoader*);
    virtual void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&);
    virtual bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length);

    virtual void willChangeEstimatedProgress();
    virtual void didChangeEstimatedProgress();
    virtual void postProgressStartedNotification();
    virtual void postProgressEstimateChangedNotification();
    virtual void postProgressFinishedNotification();
    
    virtual void setMainFrameDocumentReady(bool);

    virtual void startDownload(const WebCore::ResourceRequest&);

    virtual void willChangeTitle(WebCore::DocumentLoader*);
    virtual void didChangeTitle(WebCore::DocumentLoader*);

    virtual void committedLoad(WebCore::DocumentLoader*, const char*, int);
    virtual void finishedLoading(WebCore::DocumentLoader*);
    virtual void updateGlobalHistory();
    virtual void updateGlobalHistoryRedirectLinks();

    virtual bool shouldGoToHistoryItem(WebCore::HistoryItem*) const;
    virtual bool shouldStopLoadingForHistoryItem(WebCore::HistoryItem*) const;
    virtual void dispatchDidAddBackForwardItem(WebCore::HistoryItem*) const;
    virtual void dispatchDidRemoveBackForwardItem(WebCore::HistoryItem*) const;
    virtual void dispatchDidChangeBackForwardIndex() const;
    virtual void updateGlobalHistoryItemForPage();

    virtual void didDisplayInsecureContent();
    virtual void didRunInsecureContent(WebCore::SecurityOrigin*, const WebCore::KURL&);

    virtual WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError blockedError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError interruptForPolicyChangeError(const WebCore::ResourceRequest&);

    virtual WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&);
    virtual WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&);
    virtual WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&);

    virtual bool shouldFallBack(const WebCore::ResourceError&);

    virtual WTF::String userAgent(const WebCore::KURL&);
    
    virtual void savePlatformDataToCachedFrame(WebCore::CachedFrame*);
    virtual void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*);
    virtual void transitionToCommittedForNewPage();

    virtual void didSaveToPageCache();
    virtual void didRestoreFromPageCache();

    virtual void dispatchDidBecomeFrameset(bool);

    virtual bool canHandleRequest(const WebCore::ResourceRequest&) const;
    virtual bool canShowMIMEType(const WTF::String& MIMEType) const;
    virtual bool canShowMIMETypeAsHTML(const WTF::String& MIMEType) const;
    virtual bool representationExistsForURLScheme(const WTF::String& URLScheme) const;
    virtual WTF::String generatedMIMETypeForURLScheme(const WTF::String& URLScheme) const;

    virtual void frameLoadCompleted();
    virtual void saveViewStateToItem(WebCore::HistoryItem*);
    virtual void restoreViewState();
    virtual void provisionalLoadStarted();
    virtual void didFinishLoad();
    virtual void prepareForDataSourceReplacement();
    virtual PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&);

    virtual void setTitle(const WebCore::StringWithDirection&, const WebCore::KURL&);

    virtual PassRefPtr<WebCore::Frame> createFrame(const WebCore::KURL& url, const WTF::String& name, WebCore::HTMLFrameOwnerElement*,
                                        const WTF::String& referrer, bool allowsScrolling, int marginWidth, int marginHeight);
    virtual void didTransferChildFrameToNewDocument(WebCore::Page* oldPage);
    virtual void transferLoadingResourceFromPage(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&, WebCore::Page* oldPage);
    virtual PassRefPtr<WebCore::Widget> createPlugin(const WebCore::IntSize&, WebCore::HTMLPlugInElement*, const WebCore::KURL&, const Vector<WTF::String>&,
                                          const Vector<WTF::String>&, const WTF::String&, bool);
    virtual void redirectDataToPlugin(WebCore::Widget* pluginWidget);
    
    virtual PassRefPtr<WebCore::Widget> createJavaAppletWidget(const WebCore::IntSize&, WebCore::HTMLAppletElement*, const WebCore::KURL& baseURL,
                                                    const Vector<WTF::String>& paramNames, const Vector<WTF::String>& paramValues);
    
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    virtual PassRefPtr<WebCore::Widget> createMediaPlayerProxyPlugin(const WebCore::IntSize&, WebCore::HTMLMediaElement*, const WebCore::KURL&, 
                                        const Vector<WTF::String>&, const Vector<WTF::String>&, const WTF::String&);
    virtual void hideMediaPlayerProxyPlugin(WebCore::Widget*);
    virtual void showMediaPlayerProxyPlugin(WebCore::Widget*);
#endif
    
    virtual WebCore::ObjectContentType objectContentType(const WebCore::KURL&, const WTF::String& mimeType, bool shouldPreferPlugInsForImages);
    virtual WTF::String overrideMediaType() const;
    
    virtual void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld*);
    virtual void documentElementAvailable();
    virtual void didPerformFirstNavigation() const;

    virtual void registerForIconNotification(bool listen);

#if ENABLE(JAVA_BRIDGE)
    virtual jobject javaApplet(NSView*);
#endif

#if PLATFORM(MAC)
    virtual RemoteAXObjectRef accessibilityRemoteObject() { return 0; }
#endif
    
    void setOriginalURLForDownload(WebDownload *, const WebCore::ResourceRequest&) const;

    RetainPtr<WebFramePolicyListener> setUpPolicyListener(WebCore::FramePolicyFunction);

    NSDictionary *actionDictionary(const WebCore::NavigationAction&, PassRefPtr<WebCore::FormState>) const;
    
    virtual bool canCachePage() const;

    virtual PassRefPtr<WebCore::FrameNetworkingContext> createNetworkingContext();

    virtual bool shouldPaintBrokenImage(const WebCore::KURL&) const;

    RetainPtr<WebFrame> m_webFrame;

    RetainPtr<WebFramePolicyListener> m_policyListener;
    WebCore::FramePolicyFunction m_policyFunction;
};
