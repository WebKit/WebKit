/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#ifndef FrameLoaderClient_h
#define FrameLoaderClient_h

#include "AbstractShared.h"
#include "FrameLoaderTypes.h"
#include <wtf/Forward.h>

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSURLConnection;
#else
class NSImage;
class NSURLConnection;
#endif
#endif

namespace WebCore {

    class DocumentLoader;
    class Element;
    class FormState;
    class Frame;
    class FrameLoader;
    class KURL;
    class NavigationAction;
    class String;
    class ResourceLoader;
    class ResourceRequest;

    struct LoadErrorResetToken;

    typedef void (FrameLoader::*FramePolicyFunction)(PolicyAction);

    class FrameLoaderClient : public AbstractShared {
    public:
        virtual bool hasWebView() const = 0; // mainly for assertions
        virtual bool hasFrameView() const = 0; // ditto

        virtual bool hasBackForwardList() const = 0;
        virtual void resetBackForwardList() = 0;

        virtual bool provisionalItemIsTarget() const = 0;
        virtual bool loadProvisionalItemFromPageCache() = 0;
        virtual void invalidateCurrentItemPageCache() = 0;

        virtual bool privateBrowsingEnabled() const = 0;

        virtual void makeDocumentView() = 0;
        virtual void makeRepresentation(DocumentLoader*) = 0;
#if PLATFORM(MAC)
        virtual void setDocumentViewFromPageCache(NSDictionary *) = 0;
#endif
        virtual void forceLayout() = 0;
        virtual void forceLayoutForNonHTML() = 0;

        virtual void updateHistoryForCommit() = 0;

        virtual void updateHistoryForBackForwardNavigation() = 0;
        virtual void updateHistoryForReload() = 0;
        virtual void updateHistoryForStandardLoad() = 0;
        virtual void updateHistoryForInternalLoad() = 0;

        virtual void updateHistoryAfterClientRedirect() = 0;

        virtual void setCopiesOnScroll() = 0;

        virtual LoadErrorResetToken* tokenForLoadErrorReset() = 0;
        virtual void resetAfterLoadError(LoadErrorResetToken*) = 0;
        virtual void doNotResetAfterLoadError(LoadErrorResetToken*) = 0;

        virtual void willCloseDocument() = 0;

        virtual void detachedFromParent1() = 0;
        virtual void detachedFromParent2() = 0;
        virtual void detachedFromParent3() = 0;
        virtual void detachedFromParent4() = 0;

        virtual void loadedFromPageCache() = 0;

#if PLATFORM(MAC)
        virtual void download(NSURLConnection *, NSURLRequest *, NSURLResponse *, id proxy) = 0;

        virtual id dispatchIdentifierForInitialRequest(DocumentLoader*, NSURLRequest *) = 0;
        virtual NSURLRequest *dispatchWillSendRequest(DocumentLoader*, id identifier, NSURLRequest *, NSURLResponse *redirectResponse) = 0;
        virtual void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, id identifier, NSURLAuthenticationChallenge *) = 0;
        virtual void dispatchDidCancelAuthenticationChallenge(DocumentLoader*, id identifier, NSURLAuthenticationChallenge *) = 0;
        virtual void dispatchDidReceiveResponse(DocumentLoader*, id identifier, NSURLResponse *) = 0;
        virtual void dispatchDidReceiveContentLength(DocumentLoader*, id identifier, int lengthReceived) = 0;
        virtual void dispatchDidFinishLoading(DocumentLoader*, id identifier) = 0;
        virtual void dispatchDidFailLoading(DocumentLoader*, id identifier, NSError *) = 0;
        virtual bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, NSURLRequest *, NSURLResponse *, int length) = 0;
#endif

        virtual void dispatchDidHandleOnloadEvents() = 0;
        virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() = 0;
        virtual void dispatchDidCancelClientRedirect() = 0;
        virtual void dispatchWillPerformClientRedirect(const KURL&, double interval, double fireDate) = 0;
        virtual void dispatchDidChangeLocationWithinPage() = 0;
        virtual void dispatchWillClose() = 0;
        virtual void dispatchDidReceiveIcon() = 0;
        virtual void dispatchDidStartProvisionalLoad() = 0;
        virtual void dispatchDidReceiveTitle(const String& title) = 0;
        virtual void dispatchDidCommitLoad() = 0;
#if PLATFORM(MAC)
        virtual void dispatchDidFailProvisionalLoad(NSError *) = 0;
        virtual void dispatchDidFailLoad(NSError *) = 0;
#endif
        virtual void dispatchDidFinishLoad() = 0;
        virtual void dispatchDidFirstLayout() = 0;

#if PLATFORM(MAC)
        virtual Frame* dispatchCreatePage(NSURLRequest *) = 0;
#endif
        virtual void dispatchShow() = 0;

#if PLATFORM(MAC)
        virtual void dispatchDecidePolicyForMIMEType(FramePolicyFunction, const String& MIMEType, NSURLRequest *) = 0;
        virtual void dispatchDecidePolicyForNewWindowAction(FramePolicyFunction, const NavigationAction&, NSURLRequest *, const String& frameName) = 0;
        virtual void dispatchDecidePolicyForNavigationAction(FramePolicyFunction, const NavigationAction&, NSURLRequest *) = 0;
#endif
        virtual void cancelPolicyCheck() = 0;

#if PLATFORM(MAC)
        virtual void dispatchUnableToImplementPolicy(NSError *) = 0;
#endif

        virtual void dispatchWillSubmitForm(FramePolicyFunction, PassRefPtr<FormState>) = 0;

        virtual void dispatchDidLoadMainResource(DocumentLoader*) = 0;
        virtual void clearLoadingFromPageCache(DocumentLoader*) = 0;
        virtual bool isLoadingFromPageCache(DocumentLoader*) = 0;
        virtual void revertToProvisionalState(DocumentLoader*) = 0;
#if PLATFORM(MAC)
        virtual void setMainDocumentError(DocumentLoader*, NSError *) = 0;
#endif
        virtual void clearUnarchivingState(DocumentLoader*) = 0;

        virtual void progressStarted() = 0;
        virtual void progressCompleted() = 0;

#if PLATFORM(MAC)
        virtual void incrementProgress(id identifier, NSURLResponse *) = 0;
        virtual void incrementProgress(id identifier, NSData *) = 0;
        virtual void completeProgress(id identifier) = 0;
#endif

        virtual void setMainFrameDocumentReady(bool) = 0;

#if PLATFORM(MAC)
        virtual void startDownload(NSURLRequest *) = 0;
#endif

        virtual void willChangeTitle(DocumentLoader*) = 0;
        virtual void didChangeTitle(DocumentLoader*) = 0;

#if PLATFORM(MAC)
        virtual void committedLoad(DocumentLoader*, NSData *) = 0;
#endif
        virtual void finishedLoading(DocumentLoader*) = 0;
        virtual void finalSetupForReplace(DocumentLoader*) = 0;

#if PLATFORM(MAC)
        virtual NSError *cancelledError(NSURLRequest *) = 0;
        virtual NSError *cannotShowURLError(NSURLRequest *) = 0;
        virtual NSError *interruptForPolicyChangeError(NSURLRequest *) = 0;

        virtual NSError *cannotShowMIMETypeError(NSURLResponse *) = 0;
        virtual NSError *fileDoesNotExistError(NSURLResponse *) = 0;

        virtual bool shouldFallBack(NSError *) = 0;
#endif

        virtual void setDefersLoading(bool) = 0;

#if PLATFORM(MAC)
        virtual bool willUseArchive(ResourceLoader*, NSURLRequest *, NSURL *originalURL) const = 0;
#endif
        virtual bool isArchiveLoadPending(ResourceLoader*) const = 0;
        virtual void cancelPendingArchiveLoad(ResourceLoader*) = 0;
        virtual void clearArchivedResources() = 0;

        virtual bool canHandleRequest(const ResourceRequest&) const = 0;
        virtual bool canShowMIMEType(const String& MIMEType) const = 0;
        virtual bool representationExistsForURLScheme(const String& URLScheme) const = 0;
        virtual String generatedMIMETypeForURLScheme(const String& URLScheme) const = 0;

        virtual void frameLoadCompleted() = 0;
        virtual void restoreScrollPositionAndViewState() = 0;
        virtual void provisionalLoadStarted() = 0;
        virtual bool shouldTreatURLAsSameAsCurrent(const KURL&) const = 0;
        virtual void addHistoryItemForFragmentScroll() = 0;
        virtual void didFinishLoad() = 0;
        virtual void prepareForDataSourceReplacement() = 0;
#if PLATFORM(MAC)
        virtual PassRefPtr<DocumentLoader> createDocumentLoader(NSURLRequest *) = 0;
#endif
        virtual void setTitle(const String& title, const KURL&) = 0;

        virtual String userAgent() = 0;
    };

} // namespace WebCore

#endif // FrameLoaderClient_h
