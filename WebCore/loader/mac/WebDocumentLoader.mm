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

#import "config.h"
#import "WebDocumentLoader.h"

#import "FrameMac.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreSystemInterface.h"
#import "WebDataProtocol.h"
#import "WebFrameLoader.h"
#import <wtf/Assertions.h>

namespace WebCore {

DocumentLoader::DocumentLoader(NSURLRequest *req)
    : m_frame(0)
    , m_originalRequest(req)
    , m_originalRequestCopy([req copy])
    , m_request([req mutableCopy])
    , m_loadingStartedTime(0)
    , m_committed(false)
    , m_stopping(false)
    , m_loading(false)
    , m_gotFirstByte(false)
    , m_primaryLoadComplete(false)
    , m_isClientRedirect(false)
    , m_stopRecordingResponses(false)
{
    [m_originalRequestCopy.get() release];
    [m_request.get() release];
    wkSupportsMultipartXMixedReplace(m_request.get());
}

FrameLoader* DocumentLoader::frameLoader() const
{
    if (!m_frame)
        return 0;
    return m_frame->loader();
}

DocumentLoader::~DocumentLoader()
{
    ASSERT(!m_frame || frameLoader()->activeDocumentLoader() != this || !frameLoader()->isLoading());
}    


void DocumentLoader::setMainResourceData(NSData *data)
{
    m_mainResourceData = data;
}

NSData *DocumentLoader::mainResourceData() const
{
    return m_mainResourceData ? m_mainResourceData.get() : frameLoader()->mainResourceData();
}

NSURLRequest *DocumentLoader::originalRequest() const
{
    return m_originalRequest.get();
}

NSURLRequest *DocumentLoader::originalRequestCopy() const
{
    return m_originalRequestCopy.get();
}

NSMutableURLRequest *DocumentLoader::request()
{
    NSMutableURLRequest *clientRequest = [m_request.get() _webDataRequestExternalRequest];
    if (!clientRequest)
        clientRequest = m_request.get();
    return clientRequest;
}

NSURLRequest *DocumentLoader::initialRequest() const
{
    NSURLRequest *clientRequest = [m_originalRequest.get() _webDataRequestExternalRequest];
    if (!clientRequest)
        clientRequest = m_originalRequest.get();
    return clientRequest;
}

NSMutableURLRequest *DocumentLoader::actualRequest()
{
    return m_request.get();
}

NSURL *DocumentLoader::URL() const
{
    return [const_cast<DocumentLoader*>(this)->request() URL];
}

NSURL *DocumentLoader::unreachableURL() const
{
    return [m_originalRequest.get() _webDataRequestUnreachableURL];
}

void DocumentLoader::replaceRequestURLForAnchorScroll(NSURL *URL)
{
    // assert that URLs differ only by fragment ID
    
    NSMutableURLRequest *newOriginalRequest = [m_originalRequestCopy.get() mutableCopy];
    [newOriginalRequest setURL:URL];
    m_originalRequestCopy = newOriginalRequest;
    [newOriginalRequest release];
    
    NSMutableURLRequest *newRequest = [m_request.get() mutableCopy];
    [newRequest setURL:URL];
    m_request = newRequest;
    [newRequest release];
}

void DocumentLoader::setRequest(NSURLRequest *req)
{
    ASSERT_ARG(req, req != m_request);

    // Replacing an unreachable URL with alternate content looks like a server-side
    // redirect at this point, but we can replace a committed dataSource.
    bool handlingUnreachableURL = [req _webDataRequestUnreachableURL] != nil;
    if (handlingUnreachableURL)
        m_committed = false;

    // We should never be getting a redirect callback after the data
    // source is committed, except in the unreachable URL case. It 
    // would be a WebFoundation bug if it sent a redirect callback after commit.
    ASSERT(!m_committed);

    NSURLRequest *oldRequest = [m_request.get() retain];
    NSMutableURLRequest *copy = [req mutableCopy];
    m_request = copy;
    [copy release];

    // Only send webView:didReceiveServerRedirectForProvisionalLoadForFrame: if URL changed.
    // Also, don't send it when replacing unreachable URLs with alternate content.
    if (!handlingUnreachableURL && ![[oldRequest URL] isEqual:[req URL]])
        frameLoader()->didReceiveServerRedirectForProvisionalLoadForFrame();

    [oldRequest release];
}

void DocumentLoader::setResponse(NSURLResponse *resp)
{
    m_response = resp;
}

bool DocumentLoader::isStopping() const
{
    return m_stopping;
}

WebCoreFrameBridge *DocumentLoader::bridge() const
{
    if (!m_frame)
        return nil;
    return Mac(m_frame)->bridge();
}

void DocumentLoader::setMainDocumentError(NSError *error)
{
    m_mainDocumentError = error;    
    frameLoader()->setMainDocumentError(this, error);
 }

NSError *DocumentLoader::mainDocumentError() const
{
    return m_mainDocumentError.get();
}

void DocumentLoader::clearErrors()
{
    m_mainDocumentError = nil;
}

void DocumentLoader::mainReceivedError(NSError *error, bool isComplete)
{
    if (!frameLoader())
        return;
    setMainDocumentError(error);
    if (isComplete)
        frameLoader()->mainReceivedCompleteError(this, error);
}

// Cancels the data source's pending loads.  Conceptually, a data source only loads
// one document at a time, but one document may have many related resources. 
// stopLoading will stop all loads initiated by the data source, 
// but not loads initiated by child frames' data sources -- that's the WebFrame's job.
void DocumentLoader::stopLoading()
{
    // Always attempt to stop the frame because it may still be loading/parsing after the data source
    // is done loading and not stopping it can cause a world leak.
    if (m_committed)
        m_frame->stopLoading();
    
    if (!m_loading)
        return;
    
    RefPtr<Frame> protectFrame(m_frame);
    RefPtr<DocumentLoader> protectLoader(this);

    m_stopping = true;

    FrameLoader* frameLoader = DocumentLoader::frameLoader();
    
    if (frameLoader->isLoadingMainResource())
        // Stop the main resource loader and let it send the cancelled message.
        frameLoader->cancelMainResourceLoad();
    else if (frameLoader->isLoadingSubresources())
        // The main resource loader already finished loading. Set the cancelled error on the 
        // document and let the subresourceLoaders send individual cancelled messages below.
        setMainDocumentError(frameLoader->cancelledError(m_request.get()));
    else
        // If there are no resource loaders, we need to manufacture a cancelled message.
        // (A back/forward navigation has no resource loaders because its resources are cached.)
        mainReceivedError(frameLoader->cancelledError(m_request.get()), true);
    
    frameLoader->stopLoadingSubresources();
    frameLoader->stopLoadingPlugIns();
    
    m_stopping = false;
}

void DocumentLoader::setupForReplace()
{
    frameLoader()->setupForReplace();
    m_committed = NO;
}

void DocumentLoader::commitIfReady()
{
    if (m_gotFirstByte && !m_committed) {
        m_committed = true;
        frameLoader()->commitProvisionalLoad(nil);
    }
}

void DocumentLoader::finishedLoading()
{
    m_gotFirstByte = true;   
    commitIfReady();
    frameLoader()->finishedLoadingDocument(this);
    m_frame->end();
}

void DocumentLoader::setCommitted(bool f)
{
    m_committed = f;
}

bool DocumentLoader::isCommitted() const
{
    return m_committed;
}

void DocumentLoader::setLoading(bool f)
{
    m_loading = f;
}

bool DocumentLoader::isLoading() const
{
    return m_loading;
}

void DocumentLoader::commitLoad(NSData *data)
{
    // Both unloading the old page and parsing the new page may execute JavaScript which destroys the datasource
    // by starting a new load, so retain temporarily.
    RefPtr<DocumentLoader> protect(this);

    commitIfReady();
    if (FrameLoader* frameLoader = DocumentLoader::frameLoader())
        frameLoader->committedLoad(this, data);
}

bool DocumentLoader::doesProgressiveLoad(NSString *MIMEType) const
{
    return !frameLoader()->isReplacing() || [MIMEType isEqualToString:@"text/html"];
}

void DocumentLoader::receivedData(NSData *data)
{    
    m_gotFirstByte = true;
    if (doesProgressiveLoad([m_response.get() MIMEType]))
        commitLoad(data);
}

void DocumentLoader::setupForReplaceByMIMEType(NSString *newMIMEType)
{
    if (!m_gotFirstByte)
        return;
    
    NSString *oldMIMEType = [m_response.get() MIMEType];
    
    if (!doesProgressiveLoad(oldMIMEType)) {
        frameLoader()->revertToProvisional(this);
        setupForReplace();
        commitLoad(mainResourceData());
    }
    
    frameLoader()->finishedLoadingDocument(this);
    m_frame->end();
    
    frameLoader()->setReplacing();
    m_gotFirstByte = false;
    
    if (doesProgressiveLoad(newMIMEType)) {
        frameLoader()->revertToProvisional(this);
        setupForReplace();
    }
    
    frameLoader()->stopLoadingSubresources();
    frameLoader()->stopLoadingPlugIns();

    frameLoader()->finalSetupForReplace(this);
}

void DocumentLoader::updateLoading()
{
    ASSERT(this == frameLoader()->activeDocumentLoader());
    setLoading(frameLoader()->isLoading());
}

NSURLResponse *DocumentLoader::response() const
{
    return m_response.get();
}

void DocumentLoader::setFrame(Frame* frame)
{
    if (m_frame == frame)
        return;
    ASSERT(frame && !m_frame);
    m_frame = frame;
    attachToFrame();
}

void DocumentLoader::attachToFrame()
{
    ASSERT(m_frame);
}

void DocumentLoader::detachFromFrame()
{
    ASSERT(m_frame);
    m_frame = 0;
}

void DocumentLoader::prepareForLoadStart()
{
    ASSERT(!m_stopping);
    setPrimaryLoadComplete(false);
    ASSERT(frameLoader());
    clearErrors();
    
    // Mark the start loading time.
    m_loadingStartedTime = CFAbsoluteTimeGetCurrent();
    
    setLoading(true);
    
    frameLoader()->prepareForLoadStart();
}

double DocumentLoader::loadingStartedTime() const
{
    return m_loadingStartedTime;
}

void DocumentLoader::setIsClientRedirect(bool flag)
{
    m_isClientRedirect = flag;
}

bool DocumentLoader::isClientRedirect() const
{
    return m_isClientRedirect;
}

void DocumentLoader::setPrimaryLoadComplete(bool flag)
{
    m_primaryLoadComplete = flag;
    if (flag) {
        if (frameLoader()->isLoadingMainResource()) {
            setMainResourceData(frameLoader()->mainResourceData());
            frameLoader()->releaseMainResourceLoader();
        }
        updateLoading();
    }
}

bool DocumentLoader::isLoadingInAPISense() const
{
    // Once a frame has loaded, we no longer need to consider subresources,
    // but we still need to consider subframes.
    if (frameLoader()->state() != WebFrameStateComplete) {
        if (!m_primaryLoadComplete && isLoading())
            return true;
        if (frameLoader()->isLoadingSubresources())
            return true;
        if (![bridge() doneProcessingData])
            return true;
    }
    return frameLoader()->subframeIsLoading();
}

void DocumentLoader::addResponse(NSURLResponse *r)
{
    if (!m_stopRecordingResponses)
        m_responses.append(r);
}

void DocumentLoader::stopRecordingResponses()
{
    m_stopRecordingResponses = true;
}

NSString *DocumentLoader::title() const
{
    return [[m_pageTitle.get() retain] autorelease];
}

void DocumentLoader::setLastCheckedRequest(NSURLRequest *req)
{
    NSURLRequest *copy = [req copy];
    m_lastCheckedRequest = req;
    [copy release];
}

NSURLRequest *DocumentLoader::lastCheckedRequest() const
{
    // It's OK not to make a copy here because we know the caller
    // isn't going to modify this request
    return [[m_lastCheckedRequest.get() retain] autorelease];
}

NSDictionary *DocumentLoader::triggeringAction() const
{
    return [[m_triggeringAction.get() retain] autorelease];
}

void DocumentLoader::setTriggeringAction(NSDictionary *action)
{
    m_triggeringAction = action;
}

const ResponseVector& DocumentLoader::responses() const
{
    return m_responses;
}

void DocumentLoader::setOverrideEncoding(NSString *enc)
{
    NSString *copy = [enc copy];
    m_overrideEncoding = copy;
    [copy release];
}

NSString *DocumentLoader::overrideEncoding() const
{
    return [[m_overrideEncoding.get() copy] autorelease];
}

void DocumentLoader::setTitle(NSString *title)
{
    if (!title)
        return;

    NSString *trimmed = [title mutableCopy];
    CFStringTrimWhitespace((CFMutableStringRef)trimmed);

    if ([trimmed length] != 0 && ![m_pageTitle.get() isEqualToString:trimmed]) {
        NSString *copy = [trimmed copy];
        frameLoader()->willChangeTitle(this);
        m_pageTitle = copy;
        frameLoader()->didChangeTitle(this);
        [copy release];
    }

    [trimmed release];
}

NSURL *DocumentLoader::URLForHistory() const
{
    // Return the URL to be used for history and B/F list.
    // Returns nil for WebDataProtocol URLs that aren't alternates 
    // for unreachable URLs, because these can't be stored in history.
    NSURL *URL = [m_originalRequestCopy.get() URL];
    if ([WebDataProtocol _webIsDataProtocolURL:URL])
        URL = [m_originalRequestCopy.get() _webDataRequestUnreachableURL];
    return URL;
}

}
