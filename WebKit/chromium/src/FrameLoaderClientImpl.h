/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FrameLoaderClientImpl_h
#define FrameLoaderClientImpl_h

// FIXME: remove this relative path once consumers from glue are removed.
#include "../public/WebNavigationPolicy.h"
#include "FrameLoaderClient.h"
#include "KURL.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>


namespace WebKit {

class WebFrameImpl;
class WebPluginContainerImpl;
class WebPluginLoadObserver;

class FrameLoaderClientImpl : public WebCore::FrameLoaderClient {
public:
    FrameLoaderClientImpl(WebFrameImpl* webFrame);
    ~FrameLoaderClientImpl();

    WebFrameImpl* webFrame() const { return m_webFrame; }

    // WebCore::FrameLoaderClient ----------------------------------------------

    virtual void frameLoaderDestroyed();

    // Notifies the WebView delegate that the JS window object has been cleared,
    // giving it a chance to bind native objects to the window before script
    // parsing begins.
    virtual void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld*);
    virtual void documentElementAvailable();

    // A frame's V8 context was created or destroyed.
    virtual void didCreateScriptContextForFrame();
    virtual void didDestroyScriptContextForFrame();

    // A context untied to a frame was created (through evaluateInIsolatedWorld).
    // This context is not tied to the lifetime of its frame, and is destroyed
    // in garbage collection.
    virtual void didCreateIsolatedScriptContext();

    virtual bool hasWebView() const;
    virtual bool hasFrameView() const;
    virtual void makeRepresentation(WebCore::DocumentLoader*);
    virtual void forceLayout();
    virtual void forceLayoutForNonHTML();
    virtual void setCopiesOnScroll();
    virtual void detachedFromParent2();
    virtual void detachedFromParent3();
    virtual void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&);
    virtual void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse);
    virtual bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long identifier);
    virtual void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&);
    virtual void dispatchDidCancelAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&);
    virtual void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&);
    virtual void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int lengthReceived);
    virtual void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier);
    virtual void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceError&);
    virtual bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length);
    virtual void dispatchDidHandleOnloadEvents();
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad();
    virtual void dispatchDidCancelClientRedirect();
    virtual void dispatchWillPerformClientRedirect(const WebCore::KURL&, double interval, double fireDate);
    virtual void dispatchDidNavigateWithinPage();
    virtual void dispatchDidChangeLocationWithinPage();
    virtual void dispatchDidPushStateWithinPage();
    virtual void dispatchDidReplaceStateWithinPage();
    virtual void dispatchDidPopStateWithinPage();
    virtual void dispatchWillClose();
    virtual void dispatchDidReceiveIcon();
    virtual void dispatchDidStartProvisionalLoad();
    virtual void dispatchDidReceiveTitle(const WebCore::String& title);
    virtual void dispatchDidChangeIcons();
    virtual void dispatchDidCommitLoad();
    virtual void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&);
    virtual void dispatchDidFailLoad(const WebCore::ResourceError&);
    virtual void dispatchDidFinishDocumentLoad();
    virtual void dispatchDidFinishLoad();
    virtual void dispatchDidFirstLayout();
    virtual void dispatchDidFirstVisuallyNonEmptyLayout();
    virtual WebCore::Frame* dispatchCreatePage();
    virtual void dispatchShow();
    virtual void dispatchDecidePolicyForMIMEType(WebCore::FramePolicyFunction function, const WebCore::String& mime_type, const WebCore::ResourceRequest&);
    virtual void dispatchDecidePolicyForNewWindowAction(WebCore::FramePolicyFunction function, const WebCore::NavigationAction& action, const WebCore::ResourceRequest& request, PassRefPtr<WebCore::FormState> form_state, const WebCore::String& frame_name);
    virtual void dispatchDecidePolicyForNavigationAction(WebCore::FramePolicyFunction function, const WebCore::NavigationAction& action, const WebCore::ResourceRequest& request, PassRefPtr<WebCore::FormState> form_state);
    virtual void cancelPolicyCheck();
    virtual void dispatchUnableToImplementPolicy(const WebCore::ResourceError&);
    virtual void dispatchWillSendSubmitEvent(WebCore::HTMLFormElement*);
    virtual void dispatchWillSubmitForm(WebCore::FramePolicyFunction, PassRefPtr<WebCore::FormState>);
    virtual void dispatchDidLoadMainResource(WebCore::DocumentLoader*);
    virtual void revertToProvisionalState(WebCore::DocumentLoader*);
    virtual void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&);
    virtual void willChangeEstimatedProgress() { }
    virtual void didChangeEstimatedProgress() { }
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
    virtual void dispatchDidAddBackForwardItem(WebCore::HistoryItem*) const;
    virtual void dispatchDidRemoveBackForwardItem(WebCore::HistoryItem*) const;
    virtual void dispatchDidChangeBackForwardIndex() const;
    virtual void didDisplayInsecureContent();
    virtual void didRunInsecureContent(WebCore::SecurityOrigin*);
    virtual WebCore::ResourceError blockedError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError interruptForPolicyChangeError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&);
    virtual WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&);
    virtual WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&);
    virtual bool shouldFallBack(const WebCore::ResourceError&);
    virtual bool canHandleRequest(const WebCore::ResourceRequest&) const;
    virtual bool canShowMIMEType(const WebCore::String& MIMEType) const;
    virtual bool representationExistsForURLScheme(const WebCore::String& URLScheme) const;
    virtual WebCore::String generatedMIMETypeForURLScheme(const WebCore::String& URLScheme) const;
    virtual void frameLoadCompleted();
    virtual void saveViewStateToItem(WebCore::HistoryItem*);
    virtual void restoreViewState();
    virtual void provisionalLoadStarted();
    virtual void didFinishLoad();
    virtual void prepareForDataSourceReplacement();
    virtual PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(
        const WebCore::ResourceRequest&, const WebCore::SubstituteData&);
    virtual void setTitle(const WebCore::String& title, const WebCore::KURL&);
    virtual WebCore::String userAgent(const WebCore::KURL&);
    virtual void savePlatformDataToCachedFrame(WebCore::CachedFrame*);
    virtual void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*);
    virtual void transitionToCommittedForNewPage();
    virtual bool canCachePage() const;
    virtual void download(
        WebCore::ResourceHandle*, const WebCore::ResourceRequest&,
        const WebCore::ResourceRequest& initialRequest,
        const WebCore::ResourceResponse&);
    virtual PassRefPtr<WebCore::Frame> createFrame(
        const WebCore::KURL& url, const WebCore::String& name,
        WebCore::HTMLFrameOwnerElement* ownerElement,
        const WebCore::String& referrer, bool allowsScrolling,
        int marginWidth, int marginHeight);
    virtual void didTransferChildFrameToNewDocument();
    virtual PassRefPtr<WebCore::Widget> createPlugin(
        const WebCore::IntSize&, WebCore::HTMLPlugInElement*, const WebCore::KURL&,
        const Vector<WebCore::String>&, const Vector<WebCore::String>&,
        const WebCore::String&, bool loadManually);
    virtual void redirectDataToPlugin(WebCore::Widget* pluginWidget);
    virtual PassRefPtr<WebCore::Widget> createJavaAppletWidget(
        const WebCore::IntSize&,
        WebCore::HTMLAppletElement*,
        const WebCore::KURL& /* base_url */,
        const Vector<WebCore::String>& paramNames,
        const Vector<WebCore::String>& paramValues);
    virtual WebCore::ObjectContentType objectContentType(
        const WebCore::KURL& url, const WebCore::String& mimeType);
    virtual WebCore::String overrideMediaType() const;
    virtual void didPerformFirstNavigation() const;
    virtual void registerForIconNotification(bool listen = true);
    virtual void didChangeScrollOffset();
    virtual bool allowJavaScript(bool enabledPerSettings);
    virtual bool allowPlugins(bool enabledPerSettings);
    virtual bool allowImages(bool enabledPerSettings);
    virtual void didNotAllowScript();
    virtual void didNotAllowPlugins();

private:
    void makeDocumentView();

    // Given a NavigationAction, determine the associated WebNavigationPolicy.
    // For example, a middle click means "open in background tab".
    static bool actionSpecifiesNavigationPolicy(
        const WebCore::NavigationAction& action, WebNavigationPolicy* policy);

    PassOwnPtr<WebPluginLoadObserver> pluginLoadObserver();

    // The WebFrame that owns this object and manages its lifetime. Therefore,
    // the web frame object is guaranteed to exist.
    WebFrameImpl* m_webFrame;

    // True if makeRepresentation was called.  We don't actually have a concept
    // of a "representation", but we need to know when we're expected to have one.
    // See finishedLoading().
    bool m_hasRepresentation;

    // Used to help track client redirects. When a provisional load starts, it
    // has no redirects in its chain. But in the case of client redirects, we want
    // to add that initial load as a redirect. When we get a new provisional load
    // and the dest URL matches that load, we know that it was the result of a
    // previous client redirect and the source should be added as a redirect.
    // Both should be empty if unused.
    WebCore::KURL m_expectedClientRedirectSrc;
    WebCore::KURL m_expectedClientRedirectDest;

    // Contains a pointer to the plugin widget.
    RefPtr<WebPluginContainerImpl> m_pluginWidget;

    // Indicates if we need to send over the initial notification to the plugin
    // which specifies that the plugin should be ready to accept data.
    bool m_sentInitialResponseToPlugin;

    // The navigation policy to use for the next call to dispatchCreatePage.
    WebNavigationPolicy m_nextNavigationPolicy;
};

} // namespace WebKit

#endif
