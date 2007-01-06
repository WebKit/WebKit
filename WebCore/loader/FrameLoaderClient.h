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

#include "FrameLoaderTypes.h"
#include <wtf/Forward.h>
#include <wtf/Platform.h>

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSURLConnection;
#else
class NSImage;
class NSURLConnection;
#endif
#else
// FIXME: Get rid of this once we don't use id in the loader
typedef void* id;
#endif

namespace WebCore {

    class DocumentLoader;
    class Element;
    class FormState;
    class Frame;
    class FrameLoader;
    class HistoryItem;
    class KURL;
    class NavigationAction;
    class PageCache;
    class String;
    class ResourceError;
    class ResourceHandle;
    class ResourceLoader;
    class ResourceRequest;
    class ResourceResponse;

    typedef void (FrameLoader::*FramePolicyFunction)(PolicyAction);

    class FrameLoaderClient {
    public:
        virtual ~FrameLoaderClient() {  }
        virtual void frameLoaderDestroyed() = 0;
        
        virtual bool hasWebView() const = 0; // mainly for assertions
        virtual bool hasFrameView() const = 0; // ditto

        virtual bool privateBrowsingEnabled() const = 0;

        virtual void makeDocumentView() = 0;
        virtual void makeRepresentation(DocumentLoader*) = 0;
        virtual void setDocumentViewFromPageCache(PageCache*) = 0;
        virtual void forceLayout() = 0;
        virtual void forceLayoutForNonHTML() = 0;

        virtual void setCopiesOnScroll() = 0;

        virtual void detachedFromParent1() = 0;
        virtual void detachedFromParent2() = 0;
        virtual void detachedFromParent3() = 0;
        virtual void detachedFromParent4() = 0;

        virtual void loadedFromPageCache() = 0;

        virtual void download(ResourceHandle*, const ResourceRequest&, const ResourceResponse&) = 0;

#if PLATFORM(MAC)
        virtual id dispatchIdentifierForInitialRequest(DocumentLoader*, const ResourceRequest&) = 0;
#endif
        virtual void dispatchWillSendRequest(DocumentLoader*, id identifier, ResourceRequest&, const ResourceResponse& redirectResponse) = 0;
#if PLATFORM(MAC)
        virtual void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, id identifier, NSURLAuthenticationChallenge *) = 0;
        virtual void dispatchDidCancelAuthenticationChallenge(DocumentLoader*, id identifier, NSURLAuthenticationChallenge *) = 0;
#endif
        virtual void dispatchDidReceiveResponse(DocumentLoader*, id identifier, const ResourceResponse&) = 0;
        virtual void dispatchDidReceiveContentLength(DocumentLoader*, id identifier, int lengthReceived) = 0;
        virtual void dispatchDidFinishLoading(DocumentLoader*, id identifier) = 0;
        virtual void dispatchDidFailLoading(DocumentLoader*, id identifier, const ResourceError&) = 0;
        virtual bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length) = 0;

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
        virtual void dispatchDidFailProvisionalLoad(const ResourceError&) = 0;
        virtual void dispatchDidFailLoad(const ResourceError&) = 0;
        virtual void dispatchDidFinishLoad() = 0;
        virtual void dispatchDidFirstLayout() = 0;

        virtual Frame* dispatchCreatePage() = 0;
        virtual void dispatchShow() = 0;

        virtual void dispatchDecidePolicyForMIMEType(FramePolicyFunction, const String& MIMEType, const ResourceRequest&) = 0;
        virtual void dispatchDecidePolicyForNewWindowAction(FramePolicyFunction, const NavigationAction&, const ResourceRequest&, const String& frameName) = 0;
        virtual void dispatchDecidePolicyForNavigationAction(FramePolicyFunction, const NavigationAction&, const ResourceRequest&) = 0;
        virtual void cancelPolicyCheck() = 0;

        virtual void dispatchUnableToImplementPolicy(const ResourceError&) = 0;

        virtual void dispatchWillSubmitForm(FramePolicyFunction, PassRefPtr<FormState>) = 0;

        virtual void dispatchDidLoadMainResource(DocumentLoader*) = 0;
        virtual void revertToProvisionalState(DocumentLoader*) = 0;
        virtual void setMainDocumentError(DocumentLoader*, const ResourceError&) = 0;
        virtual void clearUnarchivingState(DocumentLoader*) = 0;

        virtual void progressStarted() = 0;
        virtual void progressCompleted() = 0;

        virtual void incrementProgress(id identifier, const ResourceResponse&) = 0;
        virtual void incrementProgress(id identifier, const char*, int) = 0;
        virtual void completeProgress(id identifier) = 0;

        virtual void setMainFrameDocumentReady(bool) = 0;

        virtual void startDownload(const ResourceRequest&) = 0;

        virtual void willChangeTitle(DocumentLoader*) = 0;
        virtual void didChangeTitle(DocumentLoader*) = 0;

        virtual void committedLoad(DocumentLoader*, const char*, int) = 0;
        virtual void finishedLoading(DocumentLoader*) = 0;
        virtual void finalSetupForReplace(DocumentLoader*) = 0;
        
        virtual void updateGlobalHistoryForStandardLoad(const KURL&) = 0;
        virtual void updateGlobalHistoryForReload(const KURL&) = 0;
        virtual bool shouldGoToHistoryItem(HistoryItem*) const = 0;

        virtual ResourceError cancelledError(const ResourceRequest&) = 0;
        virtual ResourceError cannotShowURLError(const ResourceRequest&) = 0;
        virtual ResourceError interruptForPolicyChangeError(const ResourceRequest&) = 0;

        virtual ResourceError cannotShowMIMETypeError(const ResourceResponse&) = 0;
        virtual ResourceError fileDoesNotExistError(const ResourceResponse&) = 0;

        virtual bool shouldFallBack(const ResourceError&) = 0;

        virtual void setDefersLoading(bool) = 0;

        virtual bool willUseArchive(ResourceLoader*, const ResourceRequest&, const KURL& originalURL) const = 0;
        virtual bool isArchiveLoadPending(ResourceLoader*) const = 0;
        virtual void cancelPendingArchiveLoad(ResourceLoader*) = 0;
        virtual void clearArchivedResources() = 0;

        virtual bool canHandleRequest(const ResourceRequest&) const = 0;
        virtual bool canShowMIMEType(const String& MIMEType) const = 0;
        virtual bool representationExistsForURLScheme(const String& URLScheme) const = 0;
        virtual String generatedMIMETypeForURLScheme(const String& URLScheme) const = 0;

        virtual void frameLoadCompleted() = 0;
        virtual void saveScrollPositionAndViewStateToItem(HistoryItem*) = 0;
        virtual void restoreScrollPositionAndViewState() = 0;
        virtual void provisionalLoadStarted() = 0;
        virtual void didFinishLoad() = 0;
        virtual void prepareForDataSourceReplacement() = 0;

        virtual PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&) = 0;
        virtual void setTitle(const String& title, const KURL&) = 0;

        virtual String userAgent() = 0;
        
        virtual void saveDocumentViewToPageCache(PageCache*) = 0;
        virtual bool canCachePage() const = 0;
    };

} // namespace WebCore

#endif // FrameLoaderClient_h
