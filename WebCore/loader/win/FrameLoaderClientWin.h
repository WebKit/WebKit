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

#ifndef FrameLoaderClientWin_H
#define FrameLoaderClientWin_H

#include "FrameLoaderClient.h"

namespace WebCore {

    class FrameLoaderClientWin : public FrameLoaderClient, public Shared<FrameLoaderClientWin> {
    public:
        virtual ~FrameLoaderClientWin() { }
        virtual void frameLoaderDestroyed();

        virtual void ref() { Shared<FrameLoaderClientWin>::ref(); }
        virtual void deref() { Shared<FrameLoaderClientWin>::deref(); }

        virtual bool hasWebView() const;
        virtual bool hasFrameView() const;

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

        virtual void dispatchDidHandleOnloadEvents();
        virtual void dispatchDidReceiveServerRedirectForProvisionalLoad();
        virtual void dispatchDidCancelClientRedirect();
        virtual void dispatchWillPerformClientRedirect(const KURL&, double, double);
        virtual void dispatchDidChangeLocationWithinPage();
        virtual void dispatchWillClose();
        virtual void dispatchDidReceiveIcon();
        virtual void dispatchDidStartProvisionalLoad();
        virtual void dispatchDidReceiveTitle(const String&);
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

        virtual void progressStarted();
        virtual void progressCompleted();

        virtual void setMainFrameDocumentReady(bool);

        virtual void willChangeTitle(DocumentLoader*);
        virtual void didChangeTitle(DocumentLoader*);

        virtual void finishedLoading(DocumentLoader*);
        virtual void finalSetupForReplace(DocumentLoader*);

        virtual void setDefersLoading(bool);

        virtual bool isArchiveLoadPending(ResourceLoader*) const;
        virtual void cancelPendingArchiveLoad(ResourceLoader*);
        virtual void clearArchivedResources();

        virtual bool canHandleRequest(const ResourceRequest&) const;
        virtual bool canShowMIMEType(const String&) const;
        virtual bool representationExistsForURLScheme(const String&) const;
        virtual String generatedMIMETypeForURLScheme(const String&) const;

        virtual void frameLoadCompleted();
        virtual void restoreScrollPositionAndViewState();
        virtual void provisionalLoadStarted();
        virtual bool shouldTreatURLAsSameAsCurrent(const KURL&) const;
        virtual void addHistoryItemForFragmentScroll();
        virtual void didFinishLoad();
        virtual void prepareForDataSourceReplacement();
        virtual void setTitle(const String& title, const KURL&);

        virtual String userAgent();
    };

}

#endif // FrameLoaderClientWin_H
