/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FrameLoaderClientEfl_h
#define FrameLoaderClientEfl_h

#include "EWebKit.h"
#include "FrameLoaderClient.h"
#include "PluginView.h"
#include "ResourceResponse.h"

namespace WebCore {

class FormState;

class FrameLoaderClientEfl : public FrameLoaderClient {
 public:
    explicit FrameLoaderClientEfl(Evas_Object *view);
    virtual ~FrameLoaderClientEfl() { }
    virtual void frameLoaderDestroyed();

    void         setWebFrame(Evas_Object *frame) { m_frame = frame; }
    Evas_Object* webFrame() const { return m_frame; }
    Evas_Object* webView() const { return m_view; }

    void setCustomUserAgent(const String &agent);
    const String& customUserAgent() const;

    void setInitLayoutCompleted(bool completed) { m_initLayoutCompleted = completed; }
    bool getInitLayoutCompleted() { return m_initLayoutCompleted; }

    virtual bool hasWebView() const;
    virtual bool hasFrameView() const;

    void callPolicyFunction(FramePolicyFunction function, PolicyAction action);

    virtual void makeRepresentation(DocumentLoader*);
    virtual void forceLayout();
    virtual void forceLayoutForNonHTML();

    virtual void setCopiesOnScroll();

    virtual void detachedFromParent2();
    virtual void detachedFromParent3();

    virtual void loadedFromCachedPage();

    virtual void assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&);

    virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long  identifier, ResourceRequest&, const ResourceResponse& redirectResponse);
    virtual bool shouldUseCredentialStorage(DocumentLoader*, unsigned long identifier);
    virtual void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&);

    virtual void dispatchDidPushStateWithinPage();
    virtual void dispatchDidPopStateWithinPage();
    virtual void dispatchDidReplaceStateWithinPage();
    virtual void dispatchDidAddBackForwardItem(WebCore::HistoryItem*) const;
    virtual void dispatchDidRemoveBackForwardItem(WebCore::HistoryItem*) const;
    virtual void dispatchDidChangeBackForwardIndex() const;
    virtual void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld*);

    virtual void dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge&);
    virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long  identifier, const ResourceResponse&);
    virtual void dispatchDidReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived);
    virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long  identifier);
    virtual void dispatchDidFailLoading(DocumentLoader*, unsigned long  identifier, const ResourceError&);
    virtual bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length);
    virtual void dispatchDidLoadResourceByXMLHttpRequest(unsigned long identifier, const WebCore::ScriptString& sourceString);

    virtual void dispatchDidHandleOnloadEvents();
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad();
    virtual void dispatchDidCancelClientRedirect();
    virtual void dispatchWillPerformClientRedirect(const KURL&, double, double);
    virtual void dispatchDidChangeLocationWithinPage();
    virtual void dispatchWillClose();
    virtual void dispatchDidReceiveIcon();
    virtual void dispatchDidStartProvisionalLoad();
    virtual void dispatchDidReceiveTitle(const String&);
    virtual void dispatchDidChangeIcons();
    virtual void dispatchDidCommitLoad();
    virtual void dispatchDidFailProvisionalLoad(const ResourceError&);
    virtual void dispatchDidFailLoad(const ResourceError&);
    virtual void dispatchDidFinishDocumentLoad();
    virtual void dispatchDidFinishLoad();
    virtual void dispatchDidFirstLayout();
    virtual void dispatchDidFirstVisuallyNonEmptyLayout();

    virtual Frame* dispatchCreatePage();
    virtual void dispatchShow();

    virtual void dispatchDecidePolicyForMIMEType(FramePolicyFunction, const String& MIMEType, const ResourceRequest&);
    virtual void dispatchDecidePolicyForNewWindowAction(FramePolicyFunction, const NavigationAction&, const ResourceRequest&, WTF::PassRefPtr<FormState>, const String& frameName);
    virtual void dispatchDecidePolicyForNavigationAction(FramePolicyFunction, const NavigationAction&, const ResourceRequest&, WTF::PassRefPtr<FormState>);
    virtual void cancelPolicyCheck();

    virtual void dispatchUnableToImplementPolicy(const ResourceError&);

    virtual void dispatchWillSendSubmitEvent(HTMLFormElement*) { }
    virtual void dispatchWillSubmitForm(FramePolicyFunction, WTF::PassRefPtr<FormState>);

    virtual void dispatchDidLoadMainResource(DocumentLoader*);
    virtual void revertToProvisionalState(DocumentLoader*);
    virtual void setMainDocumentError(DocumentLoader*, const ResourceError&);

    virtual void postProgressStartedNotification();
    virtual void postProgressEstimateChangedNotification();
    virtual void postProgressFinishedNotification();

    virtual PassRefPtr<Frame> createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                               const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight);
    virtual void didTransferChildFrameToNewDocument();

    virtual PassRefPtr<Widget> createPlugin(const IntSize&, HTMLPlugInElement*, const KURL&, const WTF::Vector<String>&, const WTF::Vector<String>&, const String&, bool);
    virtual void redirectDataToPlugin(Widget* pluginWidget);
    virtual PassRefPtr<Widget> createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL& baseURL, const WTF::Vector<String>& paramNames, const WTF::Vector<String>& paramValues);
    virtual String overrideMediaType() const;
    virtual void windowObjectCleared();
    virtual void documentElementAvailable();

    virtual void didPerformFirstNavigation() const;

    virtual void registerForIconNotification(bool);

    virtual ObjectContentType objectContentType(const KURL& url, const String& mimeType);

    virtual void setMainFrameDocumentReady(bool);

    virtual void startDownload(const ResourceRequest&);

    virtual void willChangeTitle(DocumentLoader*);
    virtual void didChangeTitle(DocumentLoader*);

    virtual void committedLoad(DocumentLoader*, const char*, int);
    virtual void finishedLoading(DocumentLoader*);

    virtual void updateGlobalHistory();
    virtual void updateGlobalHistoryRedirectLinks();
    virtual bool shouldGoToHistoryItem(HistoryItem*) const;
    virtual void didDisplayInsecureContent();
    virtual void didRunInsecureContent(SecurityOrigin*);

    virtual ResourceError cancelledError(const ResourceRequest&);
    virtual ResourceError blockedError(const ResourceRequest&);
    virtual ResourceError cannotShowURLError(const ResourceRequest&);
    virtual ResourceError interruptForPolicyChangeError(const ResourceRequest&);

    virtual ResourceError cannotShowMIMETypeError(const ResourceResponse&);
    virtual ResourceError fileDoesNotExistError(const ResourceResponse&);
    virtual ResourceError pluginWillHandleLoadError(const ResourceResponse&);

    virtual bool shouldFallBack(const ResourceError&);

    virtual bool canHandleRequest(const ResourceRequest&) const;
    virtual bool canShowMIMEType(const String&) const;
    virtual bool representationExistsForURLScheme(const String&) const;
    virtual String generatedMIMETypeForURLScheme(const String&) const;

    virtual void frameLoadCompleted();
    virtual void saveViewStateToItem(HistoryItem*);
    virtual void restoreViewState();
    virtual void provisionalLoadStarted();
    virtual void didFinishLoad();
    virtual void prepareForDataSourceReplacement();

    virtual WTF::PassRefPtr<DocumentLoader> createDocumentLoader(const ResourceRequest&, const SubstituteData&);
    virtual void setTitle(const String& title, const KURL&);

    virtual String userAgent(const KURL&);

    virtual void savePlatformDataToCachedFrame(CachedFrame*);
    virtual void transitionToCommittedFromCachedFrame(CachedFrame*);
    virtual void transitionToCommittedForNewPage();

    virtual bool canCachePage() const;
    virtual void download(ResourceHandle*, const ResourceRequest&, const ResourceRequest&, const ResourceResponse&);

    virtual PassRefPtr<WebCore::FrameNetworkingContext> createNetworkingContext();
 private:
    Evas_Object *m_view;
    Evas_Object *m_frame;

    ResourceResponse m_response;
    String m_userAgent;
    String m_customUserAgent;

    ResourceError m_loadError;

    // Plugin view to redirect data to
    PluginView* m_pluginView;
    bool m_hasSentResponseToPlugin;

    bool m_initLayoutCompleted;
};

}

#endif // FrameLoaderClientEfl_h
