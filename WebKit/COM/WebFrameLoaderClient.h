/*
 * Copyright (C) 2006 Don Gibson <dgibson77@gmail.com>
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

#ifndef WebFrameLoaderClient_H
#define WebFrameLoaderClient_H

#include "FrameLoaderClient.h"

class WebFrame;

class WebFrameLoaderClient : public WebCore::FrameLoaderClient {
public:
    WebFrameLoaderClient(WebFrame* webFrame);
    virtual ~WebFrameLoaderClient();

    virtual void frameLoaderDestroyed();

    virtual bool hasWebView() const;
    virtual bool hasFrameView() const;

    virtual bool hasBackForwardList() const;
    virtual void resetBackForwardList();

    virtual bool provisionalItemIsTarget() const;
    virtual bool loadProvisionalItemFromPageCache();
    virtual void invalidateCurrentItemPageCache();

    virtual bool privateBrowsingEnabled() const;

    virtual void makeDocumentView();
    virtual void makeRepresentation(WebCore::DocumentLoader*);
    virtual void forceLayout();
    virtual void forceLayoutForNonHTML();

    virtual void updateHistoryForCommit();

    virtual void updateHistoryForBackForwardNavigation();
    virtual void updateHistoryForReload();
    virtual void updateHistoryForStandardLoad();
    virtual void updateHistoryForInternalLoad();

    virtual void updateHistoryAfterClientRedirect();

    virtual void setCopiesOnScroll();

    virtual WebCore::LoadErrorResetToken* tokenForLoadErrorReset();
    virtual void resetAfterLoadError(WebCore::LoadErrorResetToken*);
    virtual void doNotResetAfterLoadError(WebCore::LoadErrorResetToken*);

    virtual void willCloseDocument();

    virtual void detachedFromParent1();
    virtual void detachedFromParent2();
    virtual void detachedFromParent3();
    virtual void detachedFromParent4();

    virtual void loadedFromPageCache();

    virtual void dispatchDidHandleOnloadEvents();
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad();
    virtual void dispatchDidCancelClientRedirect();
    virtual void dispatchWillPerformClientRedirect(const WebCore::KURL&,
                                                   double, double);
    virtual void dispatchDidChangeLocationWithinPage();
    virtual void dispatchWillClose();
    virtual void dispatchDidReceiveIcon();
    virtual void dispatchDidStartProvisionalLoad();
    virtual void dispatchDidReceiveTitle(const WebCore::String&);
    virtual void dispatchDidCommitLoad();
    virtual void dispatchDidFinishLoad();
    virtual void dispatchDidFirstLayout();

    virtual void dispatchShow();

    virtual void cancelPolicyCheck();

    virtual void dispatchWillSubmitForm(WebCore::FramePolicyFunction,
                                        PassRefPtr<WebCore::FormState>);

    virtual void dispatchDidLoadMainResource(WebCore::DocumentLoader*);
    virtual void clearLoadingFromPageCache(WebCore::DocumentLoader*);
    virtual bool isLoadingFromPageCache(WebCore::DocumentLoader*);
    virtual void revertToProvisionalState(WebCore::DocumentLoader*);
    virtual void clearUnarchivingState(WebCore::DocumentLoader*);

    virtual void progressStarted();
    virtual void progressCompleted();

    virtual void setMainFrameDocumentReady(bool);

    virtual void willChangeTitle(WebCore::DocumentLoader*);
    virtual void didChangeTitle(WebCore::DocumentLoader*);

    virtual void finishedLoading(WebCore::DocumentLoader*);
    virtual void finalSetupForReplace(WebCore::DocumentLoader*);

    virtual void setDefersLoading(bool);

    virtual bool isArchiveLoadPending(WebCore::ResourceLoader*) const;
    virtual void cancelPendingArchiveLoad(WebCore::ResourceLoader*);
    virtual void clearArchivedResources();

    virtual bool canHandleRequest(const WebCore::ResourceRequest&) const;
    virtual bool canShowMIMEType(const WebCore::String&) const;
    virtual bool representationExistsForURLScheme(const WebCore::String&) const;
    virtual WebCore::String generatedMIMETypeForURLScheme(const WebCore::String&) const;

    virtual void frameLoadCompleted();
    virtual void restoreScrollPositionAndViewState();
    virtual void provisionalLoadStarted();
    virtual bool shouldTreatURLAsSameAsCurrent(const WebCore::KURL&) const;
    virtual void addHistoryItemForFragmentScroll();
    virtual void didFinishLoad();
    virtual void prepareForDataSourceReplacement();
    virtual void setTitle(const WebCore::String& title, const WebCore::KURL&);

    virtual WebCore::String userAgent();

protected:
    // The WebFrame that owns this object and manages its lifetime. Therefore,
    // the web frame object is guaranteed to exist.
    WebFrame* m_webFrame;
};

#endif // FrameLoaderClientWin_H
