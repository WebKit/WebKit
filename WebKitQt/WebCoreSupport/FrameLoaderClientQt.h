/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#ifndef FrameLoaderClientQt_H
#define FrameLoaderClientQt_H

#include <qobject.h>

#include "FrameLoaderClient.h"
#include "KURL.h"
#include "FrameQt.h"
#include "FrameLoader.h"
#include "Shared.h"
#include "ResourceResponse.h"
class QWebFrame;

namespace WebCore {

    class AuthenticationChallenge;
    class DocumentLoader;
    class Element;
    class FormState;
    class NavigationAction;
    class String;
    class ResourceLoader;

    struct LoadErrorResetToken;

    class FrameLoaderClientQt : public QObject, public FrameLoaderClient {
        Q_OBJECT

        void callPolicyFunction(FramePolicyFunction function, PolicyAction action);
    private slots:
        void slotCallPolicyFunction(int);
    signals:
        void sigCallPolicyFunction(int);
        void loadStarted(QWebFrame *frame);
        void loadProgressChanged(double d);
        void loadFinished(QWebFrame *frame);
    public:
        FrameLoaderClientQt();
        ~FrameLoaderClientQt();
        void setFrame(QWebFrame *webFrame, FrameQt *frame);
        virtual void detachFrameLoader();

        virtual bool hasWebView() const; // mainly for assertions
        virtual bool hasFrameView() const; // ditto

        virtual bool hasBackForwardList() const;
        virtual void resetBackForwardList();

        virtual bool provisionalItemIsTarget() const;
        virtual bool loadProvisionalItemFromPageCache();
        virtual void invalidateCurrentItemPageCache();

        virtual bool privateBrowsingEnabled() const;

        virtual void makeDocumentView();
        virtual void makeRepresentation(DocumentLoader*);
        virtual void forceLayout();
        virtual void forceLayoutForNonHTML();

        virtual void updateHistoryForCommit();

        virtual void updateHistoryForBackForwardNavigation();
        virtual void updateHistoryForReload();
        virtual void updateHistoryForStandardLoad();
        virtual void updateHistoryForInternalLoad();

        virtual void updateHistoryAfterClientRedirect();

        virtual void setCopiesOnScroll();

        virtual LoadErrorResetToken* tokenForLoadErrorReset();
        virtual void resetAfterLoadError(LoadErrorResetToken*);
        virtual void doNotResetAfterLoadError(LoadErrorResetToken*);

        virtual void willCloseDocument();

        virtual void detachedFromParent1();
        virtual void detachedFromParent2();
        virtual void detachedFromParent3();
        virtual void detachedFromParent4();

        virtual void loadedFromPageCache();

        virtual void frameLoaderDestroyed();
        virtual bool canHandleRequest(const WebCore::ResourceRequest&) const;

        virtual void dispatchDidHandleOnloadEvents();
        virtual void dispatchDidReceiveServerRedirectForProvisionalLoad();
        virtual void dispatchDidCancelClientRedirect();
        virtual void dispatchWillPerformClientRedirect(const KURL&, double interval, double fireDate);
        virtual void dispatchDidChangeLocationWithinPage();
        virtual void dispatchWillClose();
        virtual void dispatchDidReceiveIcon();
        virtual void dispatchDidStartProvisionalLoad();
        virtual void dispatchDidReceiveTitle(const String& title);
        virtual void dispatchDidCommitLoad();

        virtual void dispatchDidFinishLoad();
        virtual void dispatchDidFirstLayout();

        virtual void dispatchShow();
        virtual void cancelPolicyCheck();

        virtual void dispatchWillSubmitForm(FramePolicyFunction, PassRefPtr<FormState>);

        virtual void dispatchDidLoadMainResource(DocumentLoader*);
        virtual void clearLoadingFromPageCache(DocumentLoader*);
        virtual bool isLoadingFromPageCache(DocumentLoader*);
        virtual void revertToProvisionalState(DocumentLoader*);
        virtual void clearUnarchivingState(DocumentLoader*);

        virtual void setMainFrameDocumentReady(bool);
        virtual void willChangeTitle(DocumentLoader*);
        virtual void didChangeTitle(DocumentLoader*);
        virtual void finishedLoading(DocumentLoader*);
        virtual void finalSetupForReplace(DocumentLoader*);

        virtual void setDefersLoading(bool);
        virtual bool isArchiveLoadPending(ResourceLoader*) const;
        virtual void cancelPendingArchiveLoad(ResourceLoader*);
        virtual void clearArchivedResources();
        virtual bool canShowMIMEType(const String& MIMEType) const;
        virtual bool representationExistsForURLScheme(const String& URLScheme) const;
        virtual String generatedMIMETypeForURLScheme(const String& URLScheme) const;

        virtual void frameLoadCompleted();
        virtual void restoreScrollPositionAndViewState();
        virtual void provisionalLoadStarted();
        virtual bool shouldTreatURLAsSameAsCurrent(const KURL&) const;
        virtual void addHistoryItemForFragmentScroll();
        virtual void didFinishLoad();
        virtual void prepareForDataSourceReplacement();
        virtual void setTitle(const String& title, const KURL&);

        virtual String userAgent();


        virtual void setDocumentViewFromPageCache(WebCore::PageCache*);
        virtual void updateGlobalHistoryForStandardLoad(const WebCore::KURL&);
        virtual void updateGlobalHistoryForReload(const WebCore::KURL&);
        virtual bool shouldGoToHistoryItem(WebCore::HistoryItem*) const;
        virtual void saveScrollPositionAndViewStateToItem(WebCore::HistoryItem*);
        virtual void saveDocumentViewToPageCache(WebCore::PageCache*);
        virtual bool canCachePage() const;
        
        virtual void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&);
        virtual void committedLoad(WebCore::DocumentLoader*, const char*, int);
        virtual WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&);
        virtual WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&);
        virtual WebCore::ResourceError interruptForPolicyChangeError(const WebCore::ResourceRequest&);
        virtual WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&);
        virtual WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&);
        virtual bool shouldFallBack(const WebCore::ResourceError&);
        virtual WTF::PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&);
        virtual void download(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);

        virtual void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&);
        
        virtual void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long, WebCore::ResourceRequest&, const WebCore::ResourceResponse&);
        virtual void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&);
        virtual void dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&);
        virtual void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long, const WebCore::ResourceResponse&);
        virtual void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long, int);
        virtual void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long);
        virtual void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long, const WebCore::ResourceError&);

        virtual bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int);
        virtual void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&);
        virtual void dispatchDidFailLoad(const WebCore::ResourceError&);
        virtual WebCore::Frame* dispatchCreatePage();
        virtual void dispatchDecidePolicyForMIMEType(FramePolicyFunction function, const WebCore::String&, const WebCore::ResourceRequest&);
        virtual void dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::String&);
        virtual void dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const WebCore::NavigationAction&, const WebCore::ResourceRequest&);
        virtual void dispatchUnableToImplementPolicy(const WebCore::ResourceError&);

        virtual void startDownload(const WebCore::ResourceRequest&);
        virtual bool willUseArchive(WebCore::ResourceLoader*, const WebCore::ResourceRequest&, const WebCore::KURL&) const;

        virtual void postProgressStartedNotification();
        virtual void postProgressEstimateChangedNotification();
        virtual void postProgressFinishedNotification();
        
        // FIXME: This should probably not be here, but it's needed for the tests currently
        virtual void partClearedInBegin();


    private:
        Frame *m_frame;
        QWebFrame *m_webFrame;
        ResourceResponse m_response;
        bool m_firstData;
        FramePolicyFunction m_policyFunction;
    };

}

#endif
