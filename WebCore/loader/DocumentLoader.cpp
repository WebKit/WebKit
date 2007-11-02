/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DocumentLoader.h"

#include "CachedPage.h"
#include "Document.h"
#include "Event.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HistoryItem.h"
#include "Logging.h"
#include "MainResourceLoader.h"
#include "PlatformString.h"
#include "SharedBuffer.h"
#include "XMLTokenizer.h"
#include <wtf/Assertions.h>

namespace WebCore {

/*
 * Performs four operations:
 *  1. Convert backslashes to currency symbols
 *  2. Convert control characters to spaces
 *  3. Trim leading and trailing spaces
 *  4. Collapse internal whitespace.
 */
static inline String canonicalizedTitle(const String& title, Frame* frame)
{
    ASSERT(!title.isEmpty());

    const UChar* characters = title.characters();
    unsigned length = title.length();
    unsigned i;

    Vector<UChar> stringBuilder(length);
    unsigned builderIndex = 0;

    // Skip leading spaces and leading characters that would convert to spaces
    for (i = 0; i < length; ++i) {
        UChar c = characters[i];
        if (!(c <= 0x20 || c == 0x7F))
            break;
    }

    if (i == length)
        return "";

    // Replace control characters with spaces, and backslashes with currency symbols, and collapse whitespace.
    bool previousCharWasWS = false;
    for (; i < length; ++i) {
        UChar c = characters[i];
        if (c <= 0x20 || c == 0x7F) {
            if (previousCharWasWS)
                continue;
            stringBuilder[builderIndex++] = ' ';
            previousCharWasWS = true;
        } else if (c == '\\') {
            stringBuilder[builderIndex++] = frame->backslashAsCurrencySymbol();
            previousCharWasWS = false;
        } else {
            stringBuilder[builderIndex++] = c;
            previousCharWasWS = false;
        }
    }

    // Strip trailing spaces
    while (builderIndex > 0) {
        --builderIndex;
        if (stringBuilder[builderIndex] != ' ')
            break;
    }

    if (!builderIndex && stringBuilder[builderIndex] == ' ')
        return "";

    stringBuilder.resize(builderIndex + 1);
    return String::adopt(stringBuilder);
}

static void cancelAll(const ResourceLoaderSet& loaders)
{
    const ResourceLoaderSet copy = loaders;
    ResourceLoaderSet::const_iterator end = copy.end();
    for (ResourceLoaderSet::const_iterator it = copy.begin(); it != end; ++it)
        (*it)->cancel();
}

static void setAllDefersLoading(const ResourceLoaderSet& loaders, bool defers)
{
    const ResourceLoaderSet copy = loaders;
    ResourceLoaderSet::const_iterator end = copy.end();
    for (ResourceLoaderSet::const_iterator it = copy.begin(); it != end; ++it)
        (*it)->setDefersLoading(defers);
}

DocumentLoader::DocumentLoader(const ResourceRequest& req, const SubstituteData& substituteData)
    : m_deferMainResourceDataLoad(true)
    , m_frame(0)
    , m_originalRequest(req)
    , m_substituteData(substituteData)
    , m_originalRequestCopy(req)
    , m_request(req)
    , m_committed(false)
    , m_isStopping(false)
    , m_loading(false)
    , m_gotFirstByte(false)
    , m_primaryLoadComplete(false)
    , m_isClientRedirect(false)
    , m_loadingFromCachedPage(false)
    , m_stopRecordingResponses(false)
{
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

PassRefPtr<SharedBuffer> DocumentLoader::mainResourceData() const
{
    if (m_mainResourceData)
        return m_mainResourceData;
    if (m_mainResourceLoader)
        return m_mainResourceLoader->resourceData();
    return 0;
}

const ResourceRequest& DocumentLoader::originalRequest() const
{
    return m_originalRequest;
}

const ResourceRequest& DocumentLoader::originalRequestCopy() const
{
    return m_originalRequestCopy;
}

const ResourceRequest& DocumentLoader::request() const
{
    return m_request;
}

ResourceRequest& DocumentLoader::request()
{
    return m_request;
}

const ResourceRequest& DocumentLoader::initialRequest() const
{
    return m_originalRequest;
}

ResourceRequest& DocumentLoader::actualRequest()
{
    return m_request;
}

const KURL& DocumentLoader::URL() const
{
    return request().url();
}

void DocumentLoader::replaceRequestURLForAnchorScroll(const KURL& URL)
{
    m_originalRequestCopy.setURL(URL);
    m_request.setURL(URL);
}

void DocumentLoader::setRequest(const ResourceRequest& req)
{
    // Replacing an unreachable URL with alternate content looks like a server-side
    // redirect at this point, but we can replace a committed dataSource.
    bool handlingUnreachableURL = false;

    handlingUnreachableURL = m_substituteData.isValid() && !m_substituteData.failingURL().isEmpty();

    if (handlingUnreachableURL)
        m_committed = false;

    // We should never be getting a redirect callback after the data
    // source is committed, except in the unreachable URL case. It 
    // would be a WebFoundation bug if it sent a redirect callback after commit.
    ASSERT(!m_committed);

    KURL oldURL = m_request.url();
    m_request = req;

    // Only send webView:didReceiveServerRedirectForProvisionalLoadForFrame: if URL changed.
    // Also, don't send it when replacing unreachable URLs with alternate content.
    if (!handlingUnreachableURL && oldURL.url() != req.url().url())
        frameLoader()->didReceiveServerRedirectForProvisionalLoadForFrame();
}

void DocumentLoader::setMainDocumentError(const ResourceError& error)
{
    m_mainDocumentError = error;    
    frameLoader()->setMainDocumentError(this, error);
 }

void DocumentLoader::clearErrors()
{
    m_mainDocumentError = ResourceError();
}

void DocumentLoader::mainReceivedError(const ResourceError& error, bool isComplete)
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
    // In some rare cases, calling FrameLoader::stopLoading could set m_loading to false.
    // (This can happen when there's a single XMLHttpRequest currently loading and stopLoading causes it
    // to stop loading. Because of this, we need to save it so we don't return early.
    bool loading = m_loading;
    
    if (m_committed) {
        // Attempt to stop the frame if the document loader is loading, or if it is done loading but
        // still  parsing. Failure to do so can cause a world leak.
        Document* doc = m_frame->document();
        
        if (loading || (doc && doc->parsing()))
            m_frame->loader()->stopLoading(false);
    }

    // Always cancel multipart loaders
    cancelAll(m_multipartSubresourceLoaders);

    if (!loading)
        return;
    
    RefPtr<Frame> protectFrame(m_frame);
    RefPtr<DocumentLoader> protectLoader(this);

    m_isStopping = true;

    FrameLoader* frameLoader = DocumentLoader::frameLoader();
    
    if (m_mainResourceLoader)
        // Stop the main resource loader and let it send the cancelled message.
        m_mainResourceLoader->cancel();
    else if (!m_subresourceLoaders.isEmpty())
        // The main resource loader already finished loading. Set the cancelled error on the 
        // document and let the subresourceLoaders send individual cancelled messages below.
        setMainDocumentError(frameLoader->cancelledError(m_request));
    else
        // If there are no resource loaders, we need to manufacture a cancelled message.
        // (A back/forward navigation has no resource loaders because its resources are cached.)
        mainReceivedError(frameLoader->cancelledError(m_request), true);
    
    stopLoadingSubresources();
    stopLoadingPlugIns();
    
    m_isStopping = false;
}

void DocumentLoader::setupForReplace()
{
    frameLoader()->setupForReplace();
    m_committed = false;
}

void DocumentLoader::commitIfReady()
{
    if (m_gotFirstByte && !m_committed) {
        m_committed = true;
        frameLoader()->commitProvisionalLoad(0);
    }
}

void DocumentLoader::finishedLoading()
{
    m_gotFirstByte = true;   
    commitIfReady();
    if (FrameLoader* loader = frameLoader()) {
        loader->finishedLoadingDocument(this);
        loader->end();
    }
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

void DocumentLoader::commitLoad(const char* data, int length)
{
    // Both unloading the old page and parsing the new page may execute JavaScript which destroys the datasource
    // by starting a new load, so retain temporarily.
    RefPtr<DocumentLoader> protect(this);

    commitIfReady();
    if (FrameLoader* frameLoader = DocumentLoader::frameLoader())
        frameLoader->committedLoad(this, data, length);
}

bool DocumentLoader::doesProgressiveLoad(const String& MIMEType) const
{
    return !frameLoader()->isReplacing() || MIMEType == "text/html";
}

void DocumentLoader::receivedData(const char* data, int length)
{    
    m_gotFirstByte = true;
    if (doesProgressiveLoad(m_response.mimeType()))
        commitLoad(data, length);
}

void DocumentLoader::setupForReplaceByMIMEType(const String& newMIMEType)
{
    if (!m_gotFirstByte)
        return;
    
    String oldMIMEType = m_response.mimeType();
    
    if (!doesProgressiveLoad(oldMIMEType)) {
        frameLoader()->revertToProvisional(this);
        setupForReplace();
        RefPtr<SharedBuffer> resourceData = mainResourceData();
        commitLoad(resourceData->data(), resourceData->size());
    }
    
    frameLoader()->finishedLoadingDocument(this);
    m_frame->loader()->end();
    
    frameLoader()->setReplacing();
    m_gotFirstByte = false;
    
    if (doesProgressiveLoad(newMIMEType)) {
        frameLoader()->revertToProvisional(this);
        setupForReplace();
    }
    
    stopLoadingSubresources();
    stopLoadingPlugIns();

    frameLoader()->finalSetupForReplace(this);
}

void DocumentLoader::updateLoading()
{
    ASSERT(this == frameLoader()->activeDocumentLoader());
    setLoading(frameLoader()->isLoading());
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
    ASSERT(!m_isStopping);
    setPrimaryLoadComplete(false);
    ASSERT(frameLoader());
    clearErrors();
    
    setLoading(true);
    
    frameLoader()->prepareForLoadStart();
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
        if (m_mainResourceLoader) {
            m_mainResourceData = m_mainResourceLoader->resourceData();
            m_mainResourceLoader = 0;
        }
        updateLoading();
    }
}

bool DocumentLoader::isLoadingInAPISense() const
{
    // Once a frame has loaded, we no longer need to consider subresources,
    // but we still need to consider subframes.
    if (frameLoader()->state() != FrameStateComplete) {
        if (!m_primaryLoadComplete && isLoading())
            return true;
        if (!m_subresourceLoaders.isEmpty())
            return true;
        if (Document* doc = m_frame->document())
            if (Tokenizer* tok = doc->tokenizer())
                if (tok->processingData())
                    return true;
    }
    return frameLoader()->subframeIsLoading();
}

void DocumentLoader::addResponse(const ResourceResponse& r)
{
    if (!m_stopRecordingResponses)
        m_responses.append(r);
}

void DocumentLoader::stopRecordingResponses()
{
    m_stopRecordingResponses = true;
}

String DocumentLoader::title() const
{
    return m_pageTitle;
}

void DocumentLoader::setLastCheckedRequest(const ResourceRequest& req)
{
    m_lastCheckedRequest = req;
}

const ResourceRequest& DocumentLoader::lastCheckedRequest() const
{
    return m_lastCheckedRequest;
}

const NavigationAction& DocumentLoader::triggeringAction() const
{
    return m_triggeringAction;
}

void DocumentLoader::setTriggeringAction(const NavigationAction& action)
{
    m_triggeringAction = action;
}

const ResponseVector& DocumentLoader::responses() const
{
    return m_responses;
}

void DocumentLoader::setOverrideEncoding(const String& enc)
{
    m_overrideEncoding = enc;
}

String DocumentLoader::overrideEncoding() const
{
    return m_overrideEncoding;
}

void DocumentLoader::setTitle(const String& title)
{
    if (title.isEmpty())
        return;

    String trimmed = canonicalizedTitle(title, m_frame);
    if (!trimmed.isEmpty() && m_pageTitle != trimmed) {
        frameLoader()->willChangeTitle(this);
        m_pageTitle = trimmed;
        frameLoader()->didChangeTitle(this);
    }
}

KURL DocumentLoader::urlForHistory() const
{
    // Return the URL to be used for history and B/F list.
    // Returns nil for WebDataProtocol URLs that aren't alternates 
    // for unreachable URLs, because these can't be stored in history.
    if (m_substituteData.isValid())
        return unreachableURL();

    return m_originalRequestCopy.url();
}

void DocumentLoader::loadFromCachedPage(PassRefPtr<CachedPage> cachedPage)
{
    LOG(PageCache, "WebCorePageCache: DocumentLoader %p loading from cached page %p", this, cachedPage.get());
    
    prepareForLoadStart();
    setLoadingFromCachedPage(true);
    setCommitted(true);
    frameLoader()->commitProvisionalLoad(cachedPage);
}

const ResourceResponse& DocumentLoader::response() const
{
    return m_response;
}

void DocumentLoader::setLoadingFromCachedPage(bool loading)
{
    m_loadingFromCachedPage = loading;
}

bool DocumentLoader::isLoadingFromCachedPage() const
{
    return m_loadingFromCachedPage;
}

void DocumentLoader::setResponse(const ResourceResponse& response) 
{ 
    m_response = response; 
}

bool DocumentLoader::isStopping() const 
{ 
    return m_isStopping;
}

const ResourceError& DocumentLoader::mainDocumentError() const 
{ 
    return m_mainDocumentError; 
}

KURL DocumentLoader::originalURL() const
{
    return m_originalRequestCopy.url();
}

KURL DocumentLoader::requestURL() const
{
    return request().url();
}

KURL DocumentLoader::responseURL() const
{
    return m_response.url();
}

String DocumentLoader::responseMIMEType() const
{
    return m_response.mimeType();
}

const KURL& DocumentLoader::unreachableURL() const
{
    return m_substituteData.failingURL();
}

void DocumentLoader::setDefersLoading(bool defers)
{
    if (m_mainResourceLoader)
        m_mainResourceLoader->setDefersLoading(defers);
    setAllDefersLoading(m_subresourceLoaders, defers);
    setAllDefersLoading(m_plugInStreamLoaders, defers);
}

void DocumentLoader::stopLoadingPlugIns()
{
    cancelAll(m_plugInStreamLoaders);
}

void DocumentLoader::stopLoadingSubresources()
{
    cancelAll(m_subresourceLoaders);
}

void DocumentLoader::addSubresourceLoader(ResourceLoader* loader)
{
    m_subresourceLoaders.add(loader);
    setLoading(true);
}

void DocumentLoader::removeSubresourceLoader(ResourceLoader* loader)
{
    m_subresourceLoaders.remove(loader);
    updateLoading();
    if (Frame* frame = m_frame)
        frame->loader()->checkLoadComplete();
}

void DocumentLoader::addPlugInStreamLoader(ResourceLoader* loader)
{
    m_plugInStreamLoaders.add(loader);
    setLoading(true);
}

void DocumentLoader::removePlugInStreamLoader(ResourceLoader* loader)
{
    m_plugInStreamLoaders.remove(loader);
    updateLoading();
}

bool DocumentLoader::isLoadingMainResource() const
{
    return !!m_mainResourceLoader;
}

bool DocumentLoader::isLoadingSubresources() const
{
    return !m_subresourceLoaders.isEmpty();
}

bool DocumentLoader::isLoadingPlugIns() const
{
    return !m_plugInStreamLoaders.isEmpty();
}

bool DocumentLoader::isLoadingMultipartContent() const
{
    return m_mainResourceLoader && m_mainResourceLoader->isLoadingMultipartContent();
}

bool DocumentLoader::startLoadingMainResource(unsigned long identifier)
{
    ASSERT(!m_mainResourceLoader);
    m_mainResourceLoader = MainResourceLoader::create(m_frame);
    m_mainResourceLoader->setIdentifier(identifier);

    // FIXME: Is there any way the extra fields could have not been added by now?
    // If not, it would be great to remove this line of code.
    frameLoader()->addExtraFieldsToRequest(m_request, true, false);

    if (!m_mainResourceLoader->load(m_request, m_substituteData)) {
        // FIXME: If this should really be caught, we should just ASSERT this doesn't happen;
        // should it be caught by other parts of WebKit or other parts of the app?
        LOG_ERROR("could not create WebResourceHandle for URL %s -- should be caught by policy handler level", m_request.url().url().ascii());
        m_mainResourceLoader = 0;
        return false;
    }

    return true;
}

void DocumentLoader::cancelMainResourceLoad(const ResourceError& error)
{
    m_mainResourceLoader->cancel(error);
}

void DocumentLoader::subresourceLoaderFinishedLoadingOnePart(ResourceLoader* loader)
{
    m_multipartSubresourceLoaders.add(loader);
    m_subresourceLoaders.remove(loader);
    updateLoading();
    if (Frame* frame = m_frame)
        frame->loader()->checkLoadComplete();    
}

void DocumentLoader::iconLoadDecisionAvailable()
{
    if (m_frame)
        m_frame->loader()->iconLoadDecisionAvailable();
}

}
