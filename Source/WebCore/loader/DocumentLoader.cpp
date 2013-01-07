/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "ApplicationCacheHost.h"
#include "ArchiveResourceCollection.h"
#include "CachedPage.h"
#include "CachedResourceLoader.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentParser.h"
#include "DocumentWriter.h"
#include "Event.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "HistoryItem.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "MainResourceLoader.h"
#include "Page.h"
#include "ResourceBuffer.h"
#include "SchemeRegistry.h"
#include "Settings.h"
#include "TextResourceDecoder.h"
#include "WebCoreMemoryInstrumentation.h"
#include <wtf/Assertions.h>
#include <wtf/MemoryInstrumentationHashMap.h>
#include <wtf/MemoryInstrumentationHashSet.h>
#include <wtf/MemoryInstrumentationVector.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/Unicode.h>

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
#include "ArchiveFactory.h"
#endif

namespace WebCore {

static void cancelAll(const ResourceLoaderSet& loaders)
{
    Vector<RefPtr<ResourceLoader> > loadersCopy;
    copyToVector(loaders, loadersCopy);
    size_t size = loadersCopy.size();
    for (size_t i = 0; i < size; ++i)
        loadersCopy[i]->cancel();
}

static void setAllDefersLoading(const ResourceLoaderSet& loaders, bool defers)
{
    Vector<RefPtr<ResourceLoader> > loadersCopy;
    copyToVector(loaders, loadersCopy);
    size_t size = loadersCopy.size();
    for (size_t i = 0; i < size; ++i)
        loadersCopy[i]->setDefersLoading(defers);
}

DocumentLoader::DocumentLoader(const ResourceRequest& req, const SubstituteData& substituteData)
    : m_deferMainResourceDataLoad(true)
    , m_frame(0)
    , m_cachedResourceLoader(CachedResourceLoader::create(this))
    , m_writer(m_frame)
    , m_originalRequest(req)
    , m_substituteData(substituteData)
    , m_originalRequestCopy(req)
    , m_request(req)
    , m_committed(false)
    , m_isStopping(false)
    , m_gotFirstByte(false)
    , m_isClientRedirect(false)
    , m_loadingEmptyDocument(false)
    , m_wasOnloadHandled(false)
    , m_stopRecordingResponses(false)
    , m_substituteResourceDeliveryTimer(this, &DocumentLoader::substituteResourceDeliveryTimerFired)
    , m_didCreateGlobalHistoryEntry(false)
    , m_applicationCacheHost(adoptPtr(new ApplicationCacheHost(this)))
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
    ASSERT(!m_frame || frameLoader()->activeDocumentLoader() != this || !isLoading());
    if (m_iconLoadDecisionCallback)
        m_iconLoadDecisionCallback->invalidate();
    if (m_iconDataCallback)
        m_iconDataCallback->invalidate();
    m_cachedResourceLoader->clearDocumentLoader();
}

PassRefPtr<ResourceBuffer> DocumentLoader::mainResourceData() const
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

const KURL& DocumentLoader::url() const
{
    return request().url();
}

void DocumentLoader::replaceRequestURLForSameDocumentNavigation(const KURL& url)
{
    m_originalRequestCopy.setURL(url);
    m_request.setURL(url);
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

    m_request = req;
}

void DocumentLoader::setMainDocumentError(const ResourceError& error)
{
    m_mainDocumentError = error;    
    frameLoader()->client()->setMainDocumentError(this, error);
}

void DocumentLoader::mainReceivedError(const ResourceError& error)
{
    ASSERT(!error.isNull());

    m_applicationCacheHost->failedLoadingMainResource();

    if (!frameLoader())
        return;
    setMainDocumentError(error);
    clearMainResourceLoader();
    frameLoader()->receivedMainResourceError(error);
}

// Cancels the data source's pending loads.  Conceptually, a data source only loads
// one document at a time, but one document may have many related resources. 
// stopLoading will stop all loads initiated by the data source, 
// but not loads initiated by child frames' data sources -- that's the WebFrame's job.
void DocumentLoader::stopLoading()
{
    RefPtr<Frame> protectFrame(m_frame);
    RefPtr<DocumentLoader> protectLoader(this);

    // In some rare cases, calling FrameLoader::stopLoading could cause isLoading() to return false.
    // (This can happen when there's a single XMLHttpRequest currently loading and stopLoading causes it
    // to stop loading. Because of this, we need to save it so we don't return early.
    bool loading = isLoading();
    
    if (m_committed) {
        // Attempt to stop the frame if the document loader is loading, or if it is done loading but
        // still  parsing. Failure to do so can cause a world leak.
        Document* doc = m_frame->document();
        
        if (loading || doc->parsing())
            m_frame->loader()->stopLoading(UnloadEventPolicyNone);
    }

    // Always cancel multipart loaders
    cancelAll(m_multipartSubresourceLoaders);

    // Appcache uses ResourceHandle directly, DocumentLoader doesn't count these loads.
    m_applicationCacheHost->stopLoadingInFrame(m_frame);
    
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    clearArchiveResources();
#endif

    if (!loading) {
        // If something above restarted loading we might run into mysterious crashes like 
        // https://bugs.webkit.org/show_bug.cgi?id=62764 and <rdar://problem/9328684>
        ASSERT(!isLoading());
        return;
    }

    // We might run in to infinite recursion if we're stopping loading as the result of 
    // detaching from the frame, so break out of that recursion here.
    // See <rdar://problem/9673866> for more details.
    if (m_isStopping)
        return;

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
        mainReceivedError(frameLoader->cancelledError(m_request));
    
    stopLoadingSubresources();
    stopLoadingPlugIns();
    
    m_isStopping = false;
}

void DocumentLoader::commitIfReady()
{
    if (!m_committed) {
        m_committed = true;
        frameLoader()->commitProvisionalLoad();
    }
}

void DocumentLoader::finishedLoading()
{
    commitIfReady();
    if (!frameLoader())
        return;

    if (!maybeCreateArchive()) {
        // If this is an empty document, it will not have actually been created yet. Commit dummy data so that
        // DocumentWriter::begin() gets called and creates the Document.
        if (!m_gotFirstByte)
            commitData(0, 0);
        frameLoader()->client()->finishedLoading(this);
    }

    m_writer.end();
    if (!m_mainDocumentError.isNull())
        return;
    clearMainResourceLoader();
    if (!frameLoader()->stateMachine()->creatingInitialEmptyDocument())
        frameLoader()->checkLoadComplete();
}

void DocumentLoader::commitLoad(const char* data, int length)
{
    // Both unloading the old page and parsing the new page may execute JavaScript which destroys the datasource
    // by starting a new load, so retain temporarily.
    RefPtr<Frame> protectFrame(m_frame);
    RefPtr<DocumentLoader> protectLoader(this);

    commitIfReady();
    FrameLoader* frameLoader = DocumentLoader::frameLoader();
    if (!frameLoader)
        return;
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    if (ArchiveFactory::isArchiveMimeType(response().mimeType()))
        return;
#endif
    frameLoader->client()->committedLoad(this, data, length);
}

void DocumentLoader::commitData(const char* bytes, size_t length)
{
    if (!m_gotFirstByte) {
        m_gotFirstByte = true;
        m_writer.begin(documentURL(), false);
        m_writer.setDocumentWasLoadedAsPartOfNavigation();

        if (frameLoader()->stateMachine()->creatingInitialEmptyDocument())
            return;
        
#if ENABLE(MHTML)
        // The origin is the MHTML file, we need to set the base URL to the document encoded in the MHTML so
        // relative URLs are resolved properly.
        if (m_archive && m_archive->type() == Archive::MHTML)
            m_frame->document()->setBaseURLOverride(m_archive->mainResource()->url());
#endif

        // Call receivedFirstData() exactly once per load. We should only reach this point multiple times
        // for multipart loads, and FrameLoader::isReplacing() will be true after the first time.
        if (!isMultipartReplacingLoad())
            frameLoader()->receivedFirstData();

        bool userChosen = true;
        String encoding = overrideEncoding();
        if (encoding.isNull()) {
            userChosen = false;
            encoding = response().textEncodingName();
#if ENABLE(WEB_ARCHIVE)
            if (m_archive && m_archive->type() == Archive::WebArchive)
                encoding = m_archive->mainResource()->textEncoding();
#endif
        }
        m_writer.setEncoding(encoding, userChosen);
    }
    ASSERT(m_frame->document()->parsing());
    m_writer.addData(bytes, length);
}

void DocumentLoader::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Loader);
    info.addMember(m_frame);
    info.addMember(m_mainResourceLoader);
    info.addMember(m_subresourceLoaders);
    info.addMember(m_multipartSubresourceLoaders);
    info.addMember(m_plugInStreamLoaders);
    info.addMember(m_substituteData);
    info.addMember(m_pageTitle.string());
    info.addMember(m_overrideEncoding);
    info.addMember(m_responses);
    info.addMember(m_originalRequest);
    info.addMember(m_originalRequestCopy);
    info.addMember(m_request);
    info.addMember(m_response);
    info.addMember(m_lastCheckedRequest);
    info.addMember(m_responses);
    info.addMember(m_pendingSubstituteResources);
    info.addMember(m_resourcesClientKnowsAbout);
    info.addMember(m_resourcesLoadedFromMemoryCacheForClientNotification);
    info.addMember(m_clientRedirectSourceForHistory);
    info.addMember(m_mainResourceData);
}

void DocumentLoader::receivedData(const char* data, int length)
{
    if (!isMultipartReplacingLoad())
        commitLoad(data, length);
}

void DocumentLoader::setupForReplace()
{
    if (!mainResourceData())
        return;
    
    maybeFinishLoadingMultipartContent();
    maybeCreateArchive();
    m_writer.end();
    frameLoader()->setReplacing();
    m_gotFirstByte = false;
    
    stopLoadingSubresources();
    stopLoadingPlugIns();
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    clearArchiveResources();
#endif
}

void DocumentLoader::checkLoadComplete()
{
    if (!m_frame || isLoading())
        return;
    ASSERT(this == frameLoader()->activeDocumentLoader());
    m_frame->document()->domWindow()->finishedLoading();
}

void DocumentLoader::setFrame(Frame* frame)
{
    if (m_frame == frame)
        return;
    ASSERT(frame && !m_frame);
    m_frame = frame;
    m_writer.setFrame(frame);
    attachToFrame();
}

void DocumentLoader::attachToFrame()
{
    ASSERT(m_frame);
}

void DocumentLoader::detachFromFrame()
{
    ASSERT(m_frame);
    RefPtr<Frame> protectFrame(m_frame);
    RefPtr<DocumentLoader> protectLoader(this);

    // It never makes sense to have a document loader that is detached from its
    // frame have any loads active, so go ahead and kill all the loads.
    stopLoading();

    m_applicationCacheHost->setDOMApplicationCache(0);
    InspectorInstrumentation::loaderDetachedFromFrame(m_frame, this);
    m_frame = 0;
}

void DocumentLoader::clearMainResourceLoader()
{
    if (m_mainResourceLoader) {
        m_mainResourceData = m_mainResourceLoader->resourceData();
        m_mainResourceLoader = 0;
    }
    m_loadingEmptyDocument = false;

    if (this == frameLoader()->activeDocumentLoader())
        checkLoadComplete();
}

bool DocumentLoader::isLoadingInAPISense() const
{
    // Once a frame has loaded, we no longer need to consider subresources,
    // but we still need to consider subframes.
    if (frameLoader()->state() != FrameStateComplete) {
        if (m_frame->settings()->needsIsLoadingInAPISenseQuirk() && !m_subresourceLoaders.isEmpty())
            return true;
    
        Document* doc = m_frame->document();
        if ((isLoadingMainResource() || !m_frame->document()->loadEventFinished()) && isLoading())
            return true;
        if (m_cachedResourceLoader->requestCount())
            return true;
        if (DocumentParser* parser = doc->parser())
            if (parser->processingData())
                return true;
    }
    return frameLoader()->subframeIsLoading();
}

bool DocumentLoader::maybeCreateArchive()
{
#if !ENABLE(WEB_ARCHIVE) && !ENABLE(MHTML)
    return false;
#else
    
    // Give the archive machinery a crack at this document. If the MIME type is not an archive type, it will return 0.
    RefPtr<ResourceBuffer> mainResourceBuffer = mainResourceData();
    m_archive = ArchiveFactory::create(m_response.url(), mainResourceBuffer ? mainResourceBuffer->sharedBuffer() : 0, m_response.mimeType());
    if (!m_archive)
        return false;
    
    addAllArchiveResources(m_archive.get());
    ArchiveResource* mainResource = m_archive->mainResource();
    m_parsedArchiveData = mainResource->data();
    m_writer.setMIMEType(mainResource->mimeType());
    
    ASSERT(m_frame->document());
    commitData(mainResource->data()->data(), mainResource->data()->size());
    return true;
#endif // !ENABLE(WEB_ARCHIVE) && !ENABLE(MHTML)
}

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
void DocumentLoader::setArchive(PassRefPtr<Archive> archive)
{
    m_archive = archive;
    addAllArchiveResources(m_archive.get());
}

void DocumentLoader::addAllArchiveResources(Archive* archive)
{
    if (!m_archiveResourceCollection)
        m_archiveResourceCollection = adoptPtr(new ArchiveResourceCollection);
        
    ASSERT(archive);
    if (!archive)
        return;
        
    m_archiveResourceCollection->addAllResources(archive);
}

// FIXME: Adding a resource directly to a DocumentLoader/ArchiveResourceCollection seems like bad design, but is API some apps rely on.
// Can we change the design in a manner that will let us deprecate that API without reducing functionality of those apps?
void DocumentLoader::addArchiveResource(PassRefPtr<ArchiveResource> resource)
{
    if (!m_archiveResourceCollection)
        m_archiveResourceCollection = adoptPtr(new ArchiveResourceCollection);
        
    ASSERT(resource);
    if (!resource)
        return;
        
    m_archiveResourceCollection->addResource(resource);
}

PassRefPtr<Archive> DocumentLoader::popArchiveForSubframe(const String& frameName, const KURL& url)
{
    return m_archiveResourceCollection ? m_archiveResourceCollection->popSubframeArchive(frameName, url) : PassRefPtr<Archive>(0);
}

void DocumentLoader::clearArchiveResources()
{
    m_archiveResourceCollection.clear();
    m_substituteResourceDeliveryTimer.stop();
}

SharedBuffer* DocumentLoader::parsedArchiveData() const
{
    return m_parsedArchiveData.get();
}
#endif // ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)

ArchiveResource* DocumentLoader::archiveResourceForURL(const KURL& url) const
{
    if (!m_archiveResourceCollection)
        return 0;
        
    ArchiveResource* resource = m_archiveResourceCollection->archiveResourceForURL(url);

    return resource && !resource->shouldIgnoreWhenUnarchiving() ? resource : 0;
}

PassRefPtr<ArchiveResource> DocumentLoader::mainResource() const
{
    const ResourceResponse& r = response();
    
    RefPtr<ResourceBuffer> mainResourceBuffer = mainResourceData();
    RefPtr<SharedBuffer> data = mainResourceBuffer ? mainResourceBuffer->sharedBuffer() : 0;
    if (!data)
        data = SharedBuffer::create();
        
    return ArchiveResource::create(data, r.url(), r.mimeType(), r.textEncodingName(), frame()->tree()->uniqueName());
}

PassRefPtr<ArchiveResource> DocumentLoader::subresource(const KURL& url) const
{
    if (!isCommitted())
        return 0;
    
    CachedResource* resource = m_cachedResourceLoader->cachedResource(url);
    if (!resource || !resource->isLoaded())
        return archiveResourceForURL(url);

    if (resource->type() == CachedResource::MainResource)
        return 0;

    // FIXME: This has the side effect of making the resource non-purgeable.
    // It would be better if it didn't have this permanent effect.
    if (!resource->makePurgeable(false))
        return 0;

    ResourceBuffer* data = resource->resourceBuffer();
    if (!data)
        return 0;

    return ArchiveResource::create(data->sharedBuffer(), url, resource->response());
}

void DocumentLoader::getSubresources(Vector<PassRefPtr<ArchiveResource> >& subresources) const
{
    if (!isCommitted())
        return;

    const CachedResourceLoader::DocumentResourceMap& allResources = m_cachedResourceLoader->allCachedResources();
    CachedResourceLoader::DocumentResourceMap::const_iterator end = allResources.end();
    for (CachedResourceLoader::DocumentResourceMap::const_iterator it = allResources.begin(); it != end; ++it) {
        RefPtr<ArchiveResource> subresource = this->subresource(KURL(ParsedURLString, it->value->url()));
        if (subresource)
            subresources.append(subresource.release());
    }

    return;
}

void DocumentLoader::deliverSubstituteResourcesAfterDelay()
{
    if (m_pendingSubstituteResources.isEmpty())
        return;
    ASSERT(m_frame && m_frame->page());
    if (m_frame->page()->defersLoading())
        return;
    if (!m_substituteResourceDeliveryTimer.isActive())
        m_substituteResourceDeliveryTimer.startOneShot(0);
}

void DocumentLoader::substituteResourceDeliveryTimerFired(Timer<DocumentLoader>*)
{
    if (m_pendingSubstituteResources.isEmpty())
        return;
    ASSERT(m_frame && m_frame->page());
    if (m_frame->page()->defersLoading())
        return;

    SubstituteResourceMap copy;
    copy.swap(m_pendingSubstituteResources);

    SubstituteResourceMap::const_iterator end = copy.end();
    for (SubstituteResourceMap::const_iterator it = copy.begin(); it != end; ++it) {
        RefPtr<ResourceLoader> loader = it->key;
        SubstituteResource* resource = it->value.get();
        
        if (resource) {
            SharedBuffer* data = resource->data();
        
            loader->didReceiveResponse(resource->response());

            // Calling ResourceLoader::didReceiveResponse can end up cancelling the load,
            // so we need to check if the loader has reached its terminal state.
            if (loader->reachedTerminalState())
                return;

            loader->didReceiveData(data->data(), data->size(), data->size(), true);

            // Calling ResourceLoader::didReceiveData can end up cancelling the load,
            // so we need to check if the loader has reached its terminal state.
            if (loader->reachedTerminalState())
                return;

            loader->didFinishLoading(0);
        } else {
            // A null resource means that we should fail the load.
            // FIXME: Maybe we should use another error here - something like "not in cache".
            loader->didFail(loader->cannotShowURLError());
        }
    }
}

#ifndef NDEBUG
bool DocumentLoader::isSubstituteLoadPending(ResourceLoader* loader) const
{
    return m_pendingSubstituteResources.contains(loader);
}
#endif

void DocumentLoader::cancelPendingSubstituteLoad(ResourceLoader* loader)
{
    if (m_pendingSubstituteResources.isEmpty())
        return;
    m_pendingSubstituteResources.remove(loader);
    if (m_pendingSubstituteResources.isEmpty())
        m_substituteResourceDeliveryTimer.stop();
}

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
bool DocumentLoader::scheduleArchiveLoad(ResourceLoader* loader, const ResourceRequest& request, const KURL& originalURL)
{
    if (request.url() == originalURL) {
        if (ArchiveResource* resource = archiveResourceForURL(originalURL)) {
            m_pendingSubstituteResources.set(loader, resource);
            deliverSubstituteResourcesAfterDelay();
            return true;
        }
    }

    if (!m_archive)
        return false;

    switch (m_archive->type()) {
#if ENABLE(WEB_ARCHIVE)
    case Archive::WebArchive:
        // WebArchiveDebugMode means we fail loads instead of trying to fetch them from the network if they're not in the archive.
        return m_frame->settings() && m_frame->settings()->webArchiveDebugModeEnabled() && ArchiveFactory::isArchiveMimeType(responseMIMEType());
#endif
#if ENABLE(MHTML)
    case Archive::MHTML:
        return true; // Always fail the load for resources not included in the MHTML.
#endif
    default:
        return false;
    }
}
#endif // ENABLE(WEB_ARCHIVE)

void DocumentLoader::addResponse(const ResourceResponse& r)
{
    if (!m_stopRecordingResponses)
        m_responses.append(r);
}

void DocumentLoader::stopRecordingResponses()
{
    m_stopRecordingResponses = true;
    m_responses.shrinkToFit();
}

void DocumentLoader::setTitle(const StringWithDirection& title)
{
    if (m_pageTitle == title)
        return;

    frameLoader()->willChangeTitle(this);
    m_pageTitle = title;
    frameLoader()->didChangeTitle(this);
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

bool DocumentLoader::urlForHistoryReflectsFailure() const
{
    return m_substituteData.isValid() || m_response.httpStatusCode() >= 400;
}

const KURL& DocumentLoader::originalURL() const
{
    return m_originalRequestCopy.url();
}

const KURL& DocumentLoader::requestURL() const
{
    return request().url();
}

const KURL& DocumentLoader::responseURL() const
{
    return m_response.url();
}

KURL DocumentLoader::documentURL() const
{
    KURL url = substituteData().responseURL();
#if ENABLE(WEB_ARCHIVE)
    if (url.isEmpty() && m_archive && m_archive->type() == Archive::WebArchive)
        url = m_archive->mainResource()->url();
#endif
    if (url.isEmpty())
        url = requestURL();
    if (url.isEmpty())
        url = responseURL();
    return url;
}

const String& DocumentLoader::responseMIMEType() const
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
    if (!defers)
        deliverSubstituteResourcesAfterDelay();
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
    // The main resource's underlying ResourceLoader will ask to be added here.
    // It is much simpler to handle special casing of main resource loads if we don't
    // let it be added. In the main resource load case, m_mainResourceLoader->loader()
    // will still be null at this point, but m_gotFirstByte should be false here if and only
    // if we are just starting the main resource load.
    if (!m_gotFirstByte)
        return;
    ASSERT(!m_subresourceLoaders.contains(loader));
    ASSERT(!m_mainResourceLoader || m_mainResourceLoader->loader() != loader);
    m_subresourceLoaders.add(loader);
}

void DocumentLoader::removeSubresourceLoader(ResourceLoader* loader)
{
    if (!m_subresourceLoaders.contains(loader))
        return;
    m_subresourceLoaders.remove(loader);
    checkLoadComplete();
    if (Frame* frame = m_frame)
        frame->loader()->checkLoadComplete();
}

void DocumentLoader::addPlugInStreamLoader(ResourceLoader* loader)
{
    m_plugInStreamLoaders.add(loader);
}

void DocumentLoader::removePlugInStreamLoader(ResourceLoader* loader)
{
    m_plugInStreamLoaders.remove(loader);
    checkLoadComplete();
}

bool DocumentLoader::isLoadingMainResource() const
{
    return !!m_mainResourceLoader || m_loadingEmptyDocument;
}

bool DocumentLoader::isLoadingMultipartContent() const
{
    return m_mainResourceLoader && m_mainResourceLoader->isLoadingMultipartContent();
}

bool DocumentLoader::isMultipartReplacingLoad() const
{
    return isLoadingMultipartContent() && frameLoader()->isReplacing();
}

bool DocumentLoader::maybeLoadEmpty()
{
    bool shouldLoadEmpty = !m_substituteData.isValid() && (m_request.url().isEmpty() || SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(m_request.url().protocol()));
    if (!shouldLoadEmpty && !frameLoader()->client()->representationExistsForURLScheme(m_request.url().protocol()))
        return false;

    m_loadingEmptyDocument = true;
    if (m_request.url().isEmpty() && !frameLoader()->stateMachine()->creatingInitialEmptyDocument())
        m_request.setURL(blankURL());
    String mimeType = shouldLoadEmpty ? "text/html" : frameLoader()->client()->generatedMIMETypeForURLScheme(m_request.url().protocol());
    setResponse(ResourceResponse(m_request.url(), mimeType, 0, String(), String()));
    finishedLoading();
    return true;
}

void DocumentLoader::startLoadingMainResource()
{
    m_mainDocumentError = ResourceError();
    timing()->markNavigationStart();
    ASSERT(!m_mainResourceLoader);

    if (maybeLoadEmpty())
        return;

    m_mainResourceLoader = MainResourceLoader::create(this);

    // FIXME: Is there any way the extra fields could have not been added by now?
    // If not, it would be great to remove this line of code.
    frameLoader()->addExtraFieldsToMainResourceRequest(m_request);
    m_mainResourceLoader->load(m_request, m_substituteData);

    if (m_request.isNull()) {
        m_mainResourceLoader = 0;
        // If the load was aborted by clearing m_request, it's possible the ApplicationCacheHost
        // is now in a state where starting an empty load will be inconsistent. Replace it with
        // a new ApplicationCacheHost.
        m_applicationCacheHost = adoptPtr(new ApplicationCacheHost(this));
        maybeLoadEmpty();
    }
}

void DocumentLoader::cancelMainResourceLoad(const ResourceError& error)
{
    m_mainResourceLoader->cancel(error);
}

void DocumentLoader::subresourceLoaderFinishedLoadingOnePart(ResourceLoader* loader)
{
    m_multipartSubresourceLoaders.add(loader);
    m_subresourceLoaders.remove(loader);
    checkLoadComplete();
    if (Frame* frame = m_frame)
        frame->loader()->checkLoadComplete();    
}

void DocumentLoader::maybeFinishLoadingMultipartContent()
{
    if (!frameLoader()->isReplacing())
        return;

    frameLoader()->setupForReplace();
    m_committed = false;
    RefPtr<ResourceBuffer> resourceData = mainResourceData();
    commitLoad(resourceData->data(), resourceData->size());
}

void DocumentLoader::iconLoadDecisionAvailable()
{
    if (m_frame)
        m_frame->loader()->icon()->loadDecisionReceived(iconDatabase().synchronousLoadDecisionForIconURL(frameLoader()->icon()->url(), this));
}

static void iconLoadDecisionCallback(IconLoadDecision decision, void* context)
{
    static_cast<DocumentLoader*>(context)->continueIconLoadWithDecision(decision);
}

void DocumentLoader::getIconLoadDecisionForIconURL(const String& urlString)
{
    if (m_iconLoadDecisionCallback)
        m_iconLoadDecisionCallback->invalidate();
    m_iconLoadDecisionCallback = IconLoadDecisionCallback::create(this, iconLoadDecisionCallback);
    iconDatabase().loadDecisionForIconURL(urlString, m_iconLoadDecisionCallback);
}

void DocumentLoader::continueIconLoadWithDecision(IconLoadDecision decision)
{
    ASSERT(m_iconLoadDecisionCallback);
    m_iconLoadDecisionCallback = 0;
    if (m_frame)
        m_frame->loader()->icon()->continueLoadWithDecision(decision);
}

static void iconDataCallback(SharedBuffer*, void*)
{
    // FIXME: Implement this once we know what parts of WebCore actually need the icon data returned.
}

void DocumentLoader::getIconDataForIconURL(const String& urlString)
{   
    if (m_iconDataCallback)
        m_iconDataCallback->invalidate();
    m_iconDataCallback = IconDataCallback::create(this, iconDataCallback);
    iconDatabase().iconDataForIconURL(urlString, m_iconDataCallback);
}

void DocumentLoader::handledOnloadEvents()
{
    m_wasOnloadHandled = true;
    applicationCacheHost()->stopDeferringEvents();
}

} // namespace WebCore
