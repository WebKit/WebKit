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
#include "StringHash.h"
#include <wtf/Forward.h>
#include <wtf/Platform.h>

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSCachedURLResponse;
#else
class NSCachedURLResponse;
#endif
#endif

namespace WebCore {

    class AuthenticationChallenge;
    class CachedPage;
    class DocumentLoader;
    class Element;
    class FormState;
    class Frame;
    class FrameLoader;
    class HistoryItem;
    class HTMLFrameOwnerElement;
    class IntSize;
    class KURL;
    class NavigationAction;
    class ResourceError;
    class ResourceHandle;
    class ResourceLoader;
    class ResourceResponse;
    class SharedBuffer;
    class SubstituteData;
    class String;
    class Widget;

    struct ResourceRequest;

    typedef void (FrameLoader::*FramePolicyFunction)(PolicyAction);

    class FrameLoaderClient {
    public:
        virtual ~FrameLoaderClient() {  }
        virtual void frameLoaderDestroyed() = 0;
        
        virtual bool hasWebView() const = 0; // mainly for assertions
        virtual bool hasFrameView() const = 0; // ditto

        virtual bool hasHTMLView() const { return true; }

        virtual bool privateBrowsingEnabled() const = 0;

        virtual void makeDocumentView() = 0;
        virtual void makeRepresentation(DocumentLoader*) = 0;
        virtual void setDocumentViewFromCachedPage(CachedPage*) = 0;
        virtual void forceLayout() = 0;
        virtual void forceLayoutForNonHTML() = 0;

        virtual void setCopiesOnScroll() = 0;

        virtual void detachedFromParent2() = 0;
        virtual void detachedFromParent3() = 0;
        virtual void detachedFromParent4() = 0;

        virtual void assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&) = 0;

        virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse) = 0;
        virtual void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&) = 0;
        virtual void dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&) = 0;        
        virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&) = 0;
        virtual void dispatchDidReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived) = 0;
        virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier) = 0;
        virtual void dispatchDidFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError&) = 0;
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
        virtual void dispatchDidFinishDocumentLoad() = 0;
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

        // Maybe these should go into a ProgressTrackerClient some day
        virtual void willChangeEstimatedProgress() { }
        virtual void didChangeEstimatedProgress() { }
        virtual void postProgressStartedNotification() = 0;
        virtual void postProgressEstimateChangedNotification() = 0;
        virtual void postProgressFinishedNotification() = 0;
        
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
        virtual ResourceError blockedError(const ResourceRequest&) = 0;
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
        virtual void saveViewStateToItem(HistoryItem*) = 0;
        virtual void restoreViewState() = 0;
        virtual void provisionalLoadStarted() = 0;
        virtual void didFinishLoad() = 0;
        virtual void prepareForDataSourceReplacement() = 0;

        virtual PassRefPtr<DocumentLoader> createDocumentLoader(const ResourceRequest&, const SubstituteData&) = 0;
        virtual void setTitle(const String& title, const KURL&) = 0;

        virtual String userAgent(const KURL&) = 0;
        
        virtual void saveDocumentViewToCachedPage(CachedPage*) = 0;
        virtual bool canCachePage() const = 0;
        virtual void download(ResourceHandle*, const ResourceRequest&, const ResourceRequest&, const ResourceResponse&) = 0;

        virtual Frame* createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                   const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight) = 0;
        virtual Widget* createPlugin(const IntSize&, Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool loadManually) = 0;
        virtual void redirectDataToPlugin(Widget* pluginWidget) = 0;
        
        virtual Widget* createJavaAppletWidget(const IntSize&, Element*, const KURL& baseURL, const Vector<String>& paramNames, const Vector<String>& paramValues) = 0;

        virtual ObjectContentType objectContentType(const KURL& url, const String& mimeType) = 0;
        virtual String overrideMediaType() const = 0;

        virtual void windowObjectCleared() const = 0;
        virtual void didPerformFirstNavigation() const = 0; // "Navigation" here means a transition from one page to another that ends up in the back/forward list.
        
        virtual void registerForIconNotification(bool listen = true) = 0;
        
#if PLATFORM(MAC)
        virtual NSCachedURLResponse* willCacheResponse(DocumentLoader*, unsigned long identifier, NSCachedURLResponse*) const = 0;
#endif
    };

} // namespace WebCore

#endif // FrameLoaderClient_h
