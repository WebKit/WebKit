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

#import <WebCore/RetainPtr.h>
#import <WebCore/Timer.h>
#import <WebCore/WebFrameLoaderClient.h>
#import <wtf/HashMap.h>

@class WebFrame;
@class WebHistoryItem;
@class WebResource;

namespace WebCore {
    class String;
    class WebResourceLoader;
}

typedef HashMap<RefPtr<WebCore::WebResourceLoader>, WebCore::RetainPtr<WebResource> > ResourceMap;

class WebFrameLoaderClient : public WebCore::FrameLoaderClient {
public:
    WebFrameLoaderClient(WebFrame*);
    WebFrame* webFrame() const { return m_webFrame.get(); }

private:
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
    virtual void makeRepresentation(WebCore::DocumentLoader*);
    virtual void setDocumentViewFromPageCache(NSDictionary *);
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

    virtual void detachedFromParent1();
    virtual void detachedFromParent2();
    virtual void detachedFromParent3();
    virtual void detachedFromParent4();

    virtual void loadedFromPageCache();

    virtual void download(NSURLConnection *, NSURLRequest *, NSURLResponse *, id proxy);

    virtual id dispatchIdentifierForInitialRequest(WebCore::DocumentLoader*, NSURLRequest *);
    virtual NSURLRequest *dispatchWillSendRequest(WebCore::DocumentLoader*, id identifier, NSURLRequest *, NSURLResponse *redirectResponse);
    virtual void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, id identifier, NSURLAuthenticationChallenge *);
    virtual void dispatchDidCancelAuthenticationChallenge(WebCore::DocumentLoader*, id identifier, NSURLAuthenticationChallenge *);
    virtual void dispatchDidReceiveResponse(WebCore::DocumentLoader*, id identifier, NSURLResponse *);
    virtual void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, id identifier, int lengthReceived);
    virtual void dispatchDidFinishLoading(WebCore::DocumentLoader*, id identifier);
    virtual void dispatchDidFailLoading(WebCore::DocumentLoader*, id identifier, NSError *);

    virtual void dispatchDidHandleOnloadEvents();
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad();
    virtual void dispatchDidCancelClientRedirect();
    virtual void dispatchWillPerformClientRedirect(NSURL *URL, NSTimeInterval, NSDate *);
    virtual void dispatchDidChangeLocationWithinPage();
    virtual void dispatchWillClose();
    virtual void dispatchDidReceiveIcon(NSImage *);
    virtual void dispatchDidStartProvisionalLoad();
    virtual void dispatchDidReceiveTitle(const WebCore::String& title);
    virtual void dispatchDidCommitLoad();
    virtual void dispatchDidFailProvisionalLoad(NSError *);
    virtual void dispatchDidFailLoad(NSError *);
    virtual void dispatchDidFinishLoad();
    virtual void dispatchDidFirstLayout();

    virtual WebCore::Frame* dispatchCreatePage(NSURLRequest *);
    virtual void dispatchShow();

    virtual void dispatchDecidePolicyForMIMEType(WebPolicyDecider *, const WebCore::String& MIMEType, NSURLRequest *);
    virtual void dispatchDecidePolicyForNewWindowAction(WebPolicyDecider *, NSDictionary *action, NSURLRequest *, const WebCore::String& frameName);
    virtual void dispatchDecidePolicyForNavigationAction(WebPolicyDecider *, NSDictionary *action, NSURLRequest *);
    virtual void dispatchUnableToImplementPolicy(NSError *);

    virtual void dispatchWillSubmitForm(WebPolicyDecider *, WebCore::Frame* sourceFrame, WebCore::Element* form, NSDictionary *values);

    virtual void dispatchDidLoadMainResource(WebCore::DocumentLoader*);
    virtual void clearLoadingFromPageCache(WebCore::DocumentLoader*);
    virtual bool isLoadingFromPageCache(WebCore::DocumentLoader*);
    virtual void revertToProvisionalState(WebCore::DocumentLoader*);
    virtual void setMainDocumentError(WebCore::DocumentLoader*, NSError *);
    virtual void clearUnarchivingState(WebCore::DocumentLoader*);

    virtual void progressStarted();
    virtual void progressCompleted();

    virtual void incrementProgress(id identifier, NSURLResponse *);
    virtual void incrementProgress(id identifier, NSData *);
    virtual void completeProgress(id identifier);

    virtual void setMainFrameDocumentReady(bool);

    virtual void startDownload(NSURLRequest *);

    virtual void willChangeTitle(WebCore::DocumentLoader*);
    virtual void didChangeTitle(WebCore::DocumentLoader*);

    virtual void committedLoad(WebCore::DocumentLoader*, NSData *);
    virtual void finishedLoading(WebCore::DocumentLoader*);
    virtual void finalSetupForReplace(WebCore::DocumentLoader*);

    virtual NSError *cancelledError(NSURLRequest *);
    virtual NSError *cannotShowURLError(NSURLRequest *);
    virtual NSError *interruptForPolicyChangeError(NSURLRequest *);

    virtual NSError *cannotShowMIMETypeError(NSURLResponse *);
    virtual NSError *fileDoesNotExistError(NSURLResponse *);

    virtual bool shouldFallBack(NSError *);

    virtual NSURL *mainFrameURL();

    virtual void setDefersCallbacks(bool);

    virtual bool willUseArchive(WebCore::WebResourceLoader*, NSURLRequest *, NSURL *originalURL) const;
    virtual bool isArchiveLoadPending(WebCore::WebResourceLoader*) const;
    virtual void cancelPendingArchiveLoad(WebCore::WebResourceLoader*);
    virtual void clearArchivedResources();

    virtual bool canHandleRequest(NSURLRequest *) const;
    virtual bool canShowMIMEType(const WebCore::String& MIMEType) const;
    virtual bool representationExistsForURLScheme(const WebCore::String& URLScheme) const;
    virtual WebCore::String generatedMIMETypeForURLScheme(const WebCore::String& URLScheme) const;

    virtual NSDictionary *elementForEvent(NSEvent *) const;

    virtual WebPolicyDecider *createPolicyDecider(id object, SEL selector);

    virtual void frameLoadCompleted();
    virtual void restoreScrollPositionAndViewState();
    virtual void provisionalLoadStarted();
    virtual bool shouldTreatURLAsSameAsCurrent(NSURL *) const;
    virtual void addHistoryItemForFragmentScroll();
    virtual void didFinishLoad();
    virtual void prepareForDataSourceReplacement();
    virtual PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(NSURLRequest *);
    virtual void setTitle(NSString *title, NSURL *);

    void deliverArchivedResourcesAfterDelay() const;
    bool canUseArchivedResource(NSURLRequest *) const;
    bool canUseArchivedResource(NSURLResponse *) const;
    void deliverArchivedResources(WebCore::Timer<WebFrameLoaderClient>*);

    bool createPageCache(WebHistoryItem *);

    WebCore::RetainPtr<WebFrame> m_webFrame;
    mutable ResourceMap m_pendingArchivedResources;
    mutable WebCore::Timer<WebFrameLoaderClient> m_archivedResourcesDeliveryTimer;
};
