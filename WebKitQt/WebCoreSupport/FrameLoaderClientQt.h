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

#include "FrameLoaderClient.h"
#include "KURL.h"
#include "FrameQt.h"
#include "FrameLoader.h"
#include "Shared.h"

namespace WebCore {

    class DocumentLoader;
    class Element;
    class FormState;
    class NavigationAction;
    class String;
    class ResourceLoader;

    struct LoadErrorResetToken;

    class FrameLoaderClientQt : public FrameLoaderClient, public Shared<FrameLoaderClientQt> {
    public:
        FrameLoaderClientQt();
        ~FrameLoaderClientQt();
        virtual void detachFrameLoader();

        virtual void ref();
        virtual void deref();

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

        // FIXME: This should probably not be here, but it's needed for the tests currently
        virtual void partClearedInBegin();
    };

}

#endif
