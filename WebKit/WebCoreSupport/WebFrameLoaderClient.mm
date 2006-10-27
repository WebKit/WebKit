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

#import "WebFrameLoaderClient.h"

#import "WebFrameInternal.h"
#import <WebCore/FrameMac.h>
#import <WebCore/PlatformString.h>
#import <WebCore/WebCoreFrameBridge.h>
#import <WebCore/WebDocumentLoader.h>
#import <WebKit/DOMElement.h>
#import <wtf/PassRefPtr.h>

using namespace WebCore;

WebFrameLoaderClient::WebFrameLoaderClient(WebFrame *webFrame)
    : m_webFrame(webFrame)
{
}

void WebFrameLoaderClient::detachFrameLoader()
{
    delete this;
}

bool WebFrameLoaderClient::hasWebView() const
{
    return [m_webFrame.get() _hasWebView];
}

bool WebFrameLoaderClient::hasFrameView() const
{
    return [m_webFrame.get() _hasFrameView];
}

bool WebFrameLoaderClient::hasBackForwardList() const
{
    return [m_webFrame.get() _hasBackForwardList];
}

void WebFrameLoaderClient::resetBackForwardList()
{
    [m_webFrame.get() _resetBackForwardList];
}

bool WebFrameLoaderClient::provisionalItemIsTarget() const
{
    return [m_webFrame.get() _provisionalItemIsTarget];
}

bool WebFrameLoaderClient::loadProvisionalItemFromPageCache()
{
    return [m_webFrame.get() _loadProvisionalItemFromPageCache];
}

void WebFrameLoaderClient::invalidateCurrentItemPageCache()
{
    [m_webFrame.get() _invalidateCurrentItemPageCache];
}

bool WebFrameLoaderClient::privateBrowsingEnabled() const
{
    return [m_webFrame.get() _privateBrowsingEnabled];
}

void WebFrameLoaderClient::makeDocumentView()
{
    [m_webFrame.get() _makeDocumentView];
}

void WebFrameLoaderClient::makeRepresentation(DocumentLoader* loader)
{
    [m_webFrame.get() _makeRepresentationForDocumentLoader:loader];
}

void WebFrameLoaderClient::setDocumentViewFromPageCache(NSDictionary *dictionary)
{
    [m_webFrame.get() _setDocumentViewFromPageCache:dictionary];
}

void WebFrameLoaderClient::forceLayout()
{
    [m_webFrame.get() _forceLayout];
}

void WebFrameLoaderClient::forceLayoutForNonHTML()
{
    [m_webFrame.get() _forceLayoutForNonHTML];
}

void WebFrameLoaderClient::updateHistoryForCommit()
{
    [m_webFrame.get() _updateHistoryForCommit];
}

void WebFrameLoaderClient::updateHistoryForBackForwardNavigation()
{
    [m_webFrame.get() _updateHistoryForBackForwardNavigation];
}

void WebFrameLoaderClient::updateHistoryForReload()
{
    [m_webFrame.get() _updateHistoryForReload];
}

void WebFrameLoaderClient::updateHistoryForStandardLoad()
{
    [m_webFrame.get() _updateHistoryForStandardLoad];
}

void WebFrameLoaderClient::updateHistoryForInternalLoad()
{
    [m_webFrame.get() _updateHistoryForInternalLoad];
}


void WebFrameLoaderClient::updateHistoryAfterClientRedirect()
{
    [m_webFrame.get() _updateHistoryAfterClientRedirect];
}

void WebFrameLoaderClient::setCopiesOnScroll()
{
    [m_webFrame.get() _setCopiesOnScroll];
}

LoadErrorResetToken* WebFrameLoaderClient::tokenForLoadErrorReset()
{
    return [m_webFrame.get() _tokenForLoadErrorReset];
}

void WebFrameLoaderClient::resetAfterLoadError(LoadErrorResetToken* token)
{
    [m_webFrame.get() _resetAfterLoadError:token];
}

void WebFrameLoaderClient::doNotResetAfterLoadError(LoadErrorResetToken* token)
{
    [m_webFrame.get() _doNotResetAfterLoadError:token];
}

void WebFrameLoaderClient::detachedFromParent1()
{
    [m_webFrame.get() _detachedFromParent1];
}

void WebFrameLoaderClient::detachedFromParent2()
{
    [m_webFrame.get() _detachedFromParent2];
}

void WebFrameLoaderClient::detachedFromParent3()
{
    [m_webFrame.get() _detachedFromParent3];
}

void WebFrameLoaderClient::detachedFromParent4()
{
    [m_webFrame.get() _detachedFromParent4];
}

void WebFrameLoaderClient::loadedFromPageCache()
{
    [m_webFrame.get() _loadedFromPageCache];
}

void WebFrameLoaderClient::download(NSURLConnection *connection, NSURLRequest *request,
    NSURLResponse *response, id proxy)
{
    [m_webFrame.get() _downloadWithLoadingConnection:connection request:request response:response proxy:proxy];
}

id WebFrameLoaderClient::dispatchIdentifierForInitialRequest(DocumentLoader* loader, NSURLRequest *request)
{
    return [m_webFrame.get() _dispatchIdentifierForInitialRequest:request fromDocumentLoader:loader];
}

NSURLRequest *WebFrameLoaderClient::dispatchWillSendRequest(DocumentLoader* loader, id identifier,
    NSURLRequest *request, NSURLResponse *redirectResponse)
{
    return [m_webFrame.get() _dispatchResource:identifier willSendRequest:request
        redirectResponse:redirectResponse fromDocumentLoader:loader];
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader* loader, id identifier,
    NSURLAuthenticationChallenge *challenge)
{
    [m_webFrame.get() _dispatchDidReceiveAuthenticationChallenge:challenge forResource:identifier
        fromDocumentLoader:loader];
}

void WebFrameLoaderClient::dispatchDidCancelAuthenticationChallenge(DocumentLoader* loader, id identifier,
    NSURLAuthenticationChallenge *challenge)
{
    [m_webFrame.get() _dispatchDidCancelAuthenticationChallenge:challenge forResource:identifier
        fromDocumentLoader:loader];
}

void WebFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader* loader, id identifier, NSURLResponse *response)
{
    [m_webFrame.get() _dispatchResource:identifier didReceiveResponse:response fromDocumentLoader:loader];
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader* loader, id identifier, int lengthReceived)
{
    [m_webFrame.get() _dispatchResource:identifier didReceiveContentLength:lengthReceived fromDocumentLoader:loader];
}

void WebFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader* loader, id identifier)
{
    [m_webFrame.get() _dispatchResource:identifier didFinishLoadingFromDocumentLoader:loader];
}

void WebFrameLoaderClient::dispatchDidFailLoading(DocumentLoader* loader, id identifier, NSError *error)
{
    [m_webFrame.get() _dispatchResource:identifier didFailLoadingWithError:error fromDocumentLoader:loader];
}

void WebFrameLoaderClient::dispatchDidHandleOnloadEvents()
{
    [m_webFrame.get() _dispatchDidHandleOnloadEventsForFrame];
}

void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    [m_webFrame.get() _dispatchDidReceiveServerRedirectForProvisionalLoadForFrame];
}

void WebFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    [m_webFrame.get() _dispatchDidCancelClientRedirectForFrame];
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(NSURL *URL, NSTimeInterval delay, NSDate *fireDate)
{
    [m_webFrame.get() _dispatchWillPerformClientRedirectToURL:URL delay:delay fireDate:fireDate];
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    [m_webFrame.get() _dispatchDidChangeLocationWithinPageForFrame];
}

void WebFrameLoaderClient::dispatchWillClose()
{
    [m_webFrame.get() _dispatchWillCloseFrame];
}

void WebFrameLoaderClient::dispatchDidReceiveIcon(NSImage *icon)
{
    [m_webFrame.get() _dispatchDidReceiveIcon:icon];
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    [m_webFrame.get() _dispatchDidStartProvisionalLoadForFrame];
}

void WebFrameLoaderClient::dispatchDidReceiveTitle(const String& title)
{
    [m_webFrame.get() _dispatchDidReceiveTitle:title];
}

void WebFrameLoaderClient::dispatchDidCommitLoad()
{
    [m_webFrame.get() _dispatchDidCommitLoadForFrame];
}

void WebFrameLoaderClient::dispatchDidFailProvisionalLoad(NSError *error)
{
    [m_webFrame.get() _dispatchDidFailProvisionalLoadWithError:error];
}

void WebFrameLoaderClient::dispatchDidFailLoad(NSError *error)
{
    [m_webFrame.get() _dispatchDidFailLoadWithError:error];
}

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    [m_webFrame.get() _dispatchDidFinishLoadForFrame];
}

void WebFrameLoaderClient::dispatchDidFirstLayout()
{
    [m_webFrame.get() _dispatchDidFirstLayoutInFrame];
}

Frame* WebFrameLoaderClient::dispatchCreatePage(NSURLRequest *request)
{
    return [m_webFrame.get() _dispatchCreateWebViewWithRequest:request];
}

void WebFrameLoaderClient::dispatchShow()
{
    [m_webFrame.get() _dispatchShow];
}

void WebFrameLoaderClient::dispatchDecidePolicyForMIMEType(WebPolicyDecider *decider, const String& MIMEType, NSURLRequest *request)
{
    [m_webFrame.get() _dispatchDecidePolicyForMIMEType:MIMEType request:request decider:decider];
}

void WebFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(WebPolicyDecider *decider, NSDictionary *action, NSURLRequest *request, const String& frameName)
{
    [m_webFrame.get() _dispatchDecidePolicyForNewWindowAction:action request:request newFrameName:frameName
        decider:decider];
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(WebPolicyDecider *decider, NSDictionary *action,
    NSURLRequest *request)
{
    [m_webFrame.get() _dispatchDecidePolicyForNavigationAction:action request:request decider:decider];
}

void WebFrameLoaderClient::dispatchUnableToImplementPolicy(NSError *error)
{
    [m_webFrame.get() _dispatchUnableToImplementPolicyWithError:error];
}

void WebFrameLoaderClient::dispatchWillSubmitForm(WebPolicyDecider *decider, Frame* sourceFrame,
    Element* form, NSDictionary *values)
{
    [m_webFrame.get() _dispatchSourceFrame:sourceFrame
        willSubmitForm:form withValues:values submissionDecider:decider];
}

void WebFrameLoaderClient::dispatchDidLoadMainResource(DocumentLoader* loader)
{
    [m_webFrame.get() _dispatchDidLoadMainResourceForDocumentLoader:loader];
}

void WebFrameLoaderClient::clearLoadingFromPageCache(DocumentLoader* loader)
{
    [m_webFrame.get() _clearLoadingFromPageCacheForDocumentLoader:loader];
}

bool WebFrameLoaderClient::isLoadingFromPageCache(DocumentLoader* loader)
{
    return [m_webFrame.get() _isDocumentLoaderLoadingFromPageCache:loader];
}

void WebFrameLoaderClient::revertToProvisionalState(DocumentLoader* loader)
{
    [m_webFrame.get() _revertToProvisionalStateForDocumentLoader:loader];
}

void WebFrameLoaderClient::setMainDocumentError(DocumentLoader* loader, NSError *error)
{
    [m_webFrame.get() _setMainDocumentError:error forDocumentLoader:loader];
}

void WebFrameLoaderClient::clearUnarchivingState(DocumentLoader* loader)
{
    [m_webFrame.get() _clearUnarchivingStateForLoader:loader];
}

void WebFrameLoaderClient::progressStarted()
{
    [m_webFrame.get() _progressStarted];
}

void WebFrameLoaderClient::progressCompleted()
{
    [m_webFrame.get() _progressCompleted];
}

void WebFrameLoaderClient::incrementProgress(id identifier, NSURLResponse *response)
{
    [m_webFrame.get() _incrementProgressForIdentifier:identifier response:response];
}

void WebFrameLoaderClient::incrementProgress(id identifier, NSData *data)
{
    [m_webFrame.get() _incrementProgressForIdentifier:identifier data:data];
}

void WebFrameLoaderClient::completeProgress(id identifier)
{
    [m_webFrame.get() _completeProgressForIdentifier:identifier];
}

void WebFrameLoaderClient::setMainFrameDocumentReady(bool ready)
{
    [m_webFrame.get() _setMainFrameDocumentReady:ready];
}

void WebFrameLoaderClient::startDownload(NSURLRequest *request)
{
    [m_webFrame.get() _startDownloadWithRequest:request];
}

void WebFrameLoaderClient::willChangeTitle(DocumentLoader* loader)
{
    [m_webFrame.get() _willChangeTitleForDocument:loader];
}

void WebFrameLoaderClient::didChangeTitle(DocumentLoader* loader)
{
    [m_webFrame.get() _didChangeTitleForDocument:loader];
}

void WebFrameLoaderClient::committedLoad(DocumentLoader* loader, NSData *data)
{
    [m_webFrame.get() _committedLoadWithDocumentLoader:loader data:data];
}

void WebFrameLoaderClient::finishedLoading(DocumentLoader* loader)
{
    [m_webFrame.get() _finishedLoadingDocument:loader];
}

void WebFrameLoaderClient::finalSetupForReplace(DocumentLoader* loader)
{
    [m_webFrame.get() _finalSetupForReplaceWithDocumentLoader:loader];
}

NSError *WebFrameLoaderClient::cancelledError(NSURLRequest *request)
{
    return [m_webFrame.get() _cancelledErrorWithRequest:request];
}

NSError *WebFrameLoaderClient::cannotShowURLError(NSURLRequest *request)
{
    return [m_webFrame.get() _cannotShowURLErrorWithRequest:request];
}

NSError *WebFrameLoaderClient::interruptForPolicyChangeError(NSURLRequest *request)
{
    return [m_webFrame.get() _interruptForPolicyChangeErrorWithRequest:request];
}

NSError *WebFrameLoaderClient::cannotShowMIMETypeError(NSURLResponse *response)
{
    return [m_webFrame.get() _cannotShowMIMETypeErrorWithResponse:response];
}

NSError *WebFrameLoaderClient::fileDoesNotExistError(NSURLResponse *response)
{
    return [m_webFrame.get() _fileDoesNotExistErrorWithResponse:response];
}

bool WebFrameLoaderClient::shouldFallBack(NSError *error)
{
    return [m_webFrame.get() _shouldFallBackForError:error];
}

NSURL *WebFrameLoaderClient::mainFrameURL()
{
    return [m_webFrame.get() _mainFrameURL];
}

void WebFrameLoaderClient::setDefersCallbacks(bool defers)
{
    [m_webFrame.get() _setDefersCallbacks:defers];
}

bool WebFrameLoaderClient::willUseArchive(WebResourceLoader* loader, NSURLRequest *request, NSURL *originalURL) const
{
    return [m_webFrame.get() _willUseArchiveForRequest:request originalURL:originalURL loader:loader];
}

bool WebFrameLoaderClient::isArchiveLoadPending(WebResourceLoader* loader) const
{
    return [m_webFrame.get() _archiveLoadPendingForLoader:loader];
}

void WebFrameLoaderClient::cancelPendingArchiveLoad(WebResourceLoader* loader)
{
    [m_webFrame.get() _cancelPendingArchiveLoadForLoader:loader];
}

void WebFrameLoaderClient::clearArchivedResources()
{
    [m_webFrame.get() _clearArchivedResources];
}

bool WebFrameLoaderClient::canHandleRequest(NSURLRequest *request) const
{
    return [m_webFrame.get() _canHandleRequest:request];
}

bool WebFrameLoaderClient::canShowMIMEType(const String& MIMEType) const
{
    return [m_webFrame.get() _canShowMIMEType:MIMEType];
}

bool WebFrameLoaderClient::representationExistsForURLScheme(const String& URLScheme) const
{
    return [m_webFrame.get() _representationExistsForURLScheme:URLScheme];
}

String WebFrameLoaderClient::generatedMIMETypeForURLScheme(const String& URLScheme) const
{
    return [m_webFrame.get() _generatedMIMETypeForURLScheme:URLScheme];
}

NSDictionary *WebFrameLoaderClient::elementForEvent(NSEvent *event) const
{
    return [m_webFrame.get() _elementForEvent:event];
}

WebPolicyDecider *WebFrameLoaderClient::createPolicyDecider(id object, SEL selector)
{
    return [m_webFrame.get() _createPolicyDeciderWithTarget:object action:selector];
}

void WebFrameLoaderClient::frameLoadCompleted()
{
    [m_webFrame.get() _frameLoadCompleted];
}

void WebFrameLoaderClient::restoreScrollPositionAndViewState()
{
    [m_webFrame.get() _restoreScrollPositionAndViewState];
}

void WebFrameLoaderClient::provisionalLoadStarted()
{
    [m_webFrame.get() _provisionalLoadStarted];
}

bool WebFrameLoaderClient::shouldTreatURLAsSameAsCurrent(NSURL *URL) const
{
    return [m_webFrame.get() _shouldTreatURLAsSameAsCurrent:URL];
}

void WebFrameLoaderClient::addHistoryItemForFragmentScroll()
{
    [m_webFrame.get() _addHistoryItemForFragmentScroll];
}

void WebFrameLoaderClient::didFinishLoad()
{
    [m_webFrame.get() _didFinishLoad];
}

void WebFrameLoaderClient::prepareForDataSourceReplacement()
{
    [m_webFrame.get() _prepareForDataSourceReplacement];
}

PassRefPtr<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(NSURLRequest *request)
{
    return [m_webFrame.get() _createDocumentLoaderWithRequest:request];
}

void WebFrameLoaderClient::setTitle(NSString *title, NSURL *URL)
{
    [m_webFrame.get() _setTitle:title forURL:URL];
}
