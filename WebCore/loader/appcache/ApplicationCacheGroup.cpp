/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ApplicationCacheGroup.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)

#include "ApplicationCache.h"
#include "ApplicationCacheResource.h"
#include "ApplicationCacheStorage.h"
#include "DocumentLoader.h"
#include "DOMApplicationCache.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "MainResourceLoader.h"
#include "ManifestParser.h"
#include "Page.h"
#include "Settings.h"
#include <wtf/HashMap.h>

namespace WebCore {

ApplicationCacheGroup::ApplicationCacheGroup(const KURL& manifestURL, bool isCopy)
    : m_manifestURL(manifestURL)
    , m_status(Idle)
    , m_savedNewestCachePointer(0)
    , m_frame(0)
    , m_storageID(0)
    , m_isCopy(isCopy)
{
}

ApplicationCacheGroup::~ApplicationCacheGroup()
{
    if (m_isCopy) {
        ASSERT(m_newestCache);
        ASSERT(m_caches.size() == 1);
        ASSERT(m_caches.contains(m_newestCache.get()));
        ASSERT(!m_cacheBeingUpdated);
        ASSERT(m_associatedDocumentLoaders.isEmpty());
        ASSERT(m_cacheCandidates.isEmpty());
        ASSERT(m_newestCache->group() == this);
        
        return;
    }
               
    ASSERT(!m_newestCache);
    ASSERT(m_caches.isEmpty());
    
    stopLoading();
    
    cacheStorage().cacheGroupDestroyed(this);
}
    
ApplicationCache* ApplicationCacheGroup::cacheForMainRequest(const ResourceRequest& request, DocumentLoader*)
{
    if (!ApplicationCache::requestIsHTTPOrHTTPSGet(request))
        return 0;

    if (ApplicationCacheGroup* group = cacheStorage().cacheGroupForURL(request.url())) {
        ASSERT(group->newestCache());
        
        return group->newestCache();
    }
    
    return 0;
}
    
ApplicationCache* ApplicationCacheGroup::fallbackCacheForMainRequest(const ResourceRequest& request, DocumentLoader*)
{
    if (!ApplicationCache::requestIsHTTPOrHTTPSGet(request))
        return 0;

    if (ApplicationCacheGroup* group = cacheStorage().fallbackCacheGroupForURL(request.url())) {
        ASSERT(group->newestCache());
        
        return group->newestCache();
    }
    
    return 0;
}

void ApplicationCacheGroup::selectCache(Frame* frame, const KURL& manifestURL)
{
    ASSERT(frame && frame->page());
    
    if (!frame->settings()->offlineWebApplicationCacheEnabled())
        return;
    
    DocumentLoader* documentLoader = frame->loader()->documentLoader();
    ASSERT(!documentLoader->applicationCache());

    if (manifestURL.isNull()) {
        selectCacheWithoutManifestURL(frame);        
        return;
    }
    
    ApplicationCache* mainResourceCache = documentLoader->mainResourceApplicationCache();
    
    if (mainResourceCache) {
        if (manifestURL == mainResourceCache->group()->m_manifestURL) {
            mainResourceCache->group()->associateDocumentLoaderWithCache(documentLoader, mainResourceCache);
            mainResourceCache->group()->update(frame);
        } else {
            // The main resource was loaded from cache, so the cache must have an entry for it. Mark it as foreign.
            ApplicationCacheResource* resource = mainResourceCache->resourceForURL(documentLoader->url());
            bool inStorage = resource->storageID();
            resource->addType(ApplicationCacheResource::Foreign);
            if (inStorage)
                cacheStorage().storeUpdatedType(resource, mainResourceCache);

            // Restart the current navigation from the top of the navigation algorithm, undoing any changes that were made
            // as part of the initial load.
            // The navigation will not result in the same resource being loaded, because "foreign" entries are never picked during navigation.
            frame->loader()->scheduleLocationChange(documentLoader->url(), frame->loader()->referrer(), true);
        }
        
        return;
    }
    
    // The resource was loaded from the network, check if it is a HTTP/HTTPS GET.    
    const ResourceRequest& request = frame->loader()->activeDocumentLoader()->request();

    if (!ApplicationCache::requestIsHTTPOrHTTPSGet(request)) {
        selectCacheWithoutManifestURL(frame);
        return;
    }

    // Check that the resource URL has the same scheme/host/port as the manifest URL.
    if (!protocolHostAndPortAreEqual(manifestURL, request.url())) {
        selectCacheWithoutManifestURL(frame);
        return;
    }
            
    ApplicationCacheGroup* group = cacheStorage().findOrCreateCacheGroup(manifestURL);
            
    if (ApplicationCache* cache = group->newestCache()) {
        ASSERT(cache->manifestResource());
                
        group->associateDocumentLoaderWithCache(frame->loader()->documentLoader(), cache);
              
        if (!frame->loader()->documentLoader()->isLoadingMainResource())
            group->finishedLoadingMainResource(frame->loader()->documentLoader());

        group->update(frame);
    } else {
        bool isUpdating = group->m_cacheBeingUpdated;
                
        if (!isUpdating)
            group->m_cacheBeingUpdated = ApplicationCache::create();
        documentLoader->setCandidateApplicationCacheGroup(group);
        group->m_cacheCandidates.add(documentLoader);

        const KURL& url = frame->loader()->documentLoader()->originalURL();
                
        unsigned type = 0;

        // If the resource has already been downloaded, remove it so that it will be replaced with the implicit resource
        if (isUpdating)
            type = group->m_cacheBeingUpdated->removeResource(url);
               
        // Add the main resource URL as an implicit entry.
        group->addEntry(url, type | ApplicationCacheResource::Implicit);

        if (!frame->loader()->documentLoader()->isLoadingMainResource())
            group->finishedLoadingMainResource(frame->loader()->documentLoader());
                
        if (!isUpdating)
            group->update(frame);                
    }               
}

void ApplicationCacheGroup::selectCacheWithoutManifestURL(Frame* frame)
{
    if (!frame->settings()->offlineWebApplicationCacheEnabled())
        return;

    DocumentLoader* documentLoader = frame->loader()->documentLoader();
    ASSERT(!documentLoader->applicationCache());

    ApplicationCache* mainResourceCache = documentLoader->mainResourceApplicationCache();

    if (mainResourceCache) {
        mainResourceCache->group()->associateDocumentLoaderWithCache(documentLoader, mainResourceCache);
        mainResourceCache->group()->update(frame);
    }
}

void ApplicationCacheGroup::finishedLoadingMainResource(DocumentLoader* loader)
{
    const KURL& url = loader->originalURL();
    
    if (ApplicationCache* cache = loader->applicationCache()) {
        RefPtr<ApplicationCacheResource> resource = ApplicationCacheResource::create(url, loader->response(), ApplicationCacheResource::Implicit, loader->mainResourceData());
        cache->addResource(resource.release());
        
        if (!m_cacheBeingUpdated)
            return;
    }
    
    ASSERT(m_pendingEntries.contains(url));
 
    EntryMap::iterator it = m_pendingEntries.find(url);
    ASSERT(it->second & ApplicationCacheResource::Implicit);

    RefPtr<ApplicationCacheResource> resource = ApplicationCacheResource::create(url, loader->response(), it->second, loader->mainResourceData());

    ASSERT(m_cacheBeingUpdated);
    m_cacheBeingUpdated->addResource(resource.release());
    
    m_pendingEntries.remove(it);
    
    checkIfLoadIsComplete();
}

void ApplicationCacheGroup::failedLoadingMainResource(DocumentLoader* unusedLoader)
{
    ASSERT_UNUSED(unusedLoader, m_cacheCandidates.contains(unusedLoader) || m_associatedDocumentLoaders.contains(unusedLoader));

    // Note that cacheUpdateFailed() can cause the cache group to be deleted.
    cacheUpdateFailed();
}

void ApplicationCacheGroup::stopLoading()
{
    if (m_manifestHandle) {
        ASSERT(!m_currentHandle);

        m_manifestHandle->setClient(0);
        m_manifestHandle->cancel();
        m_manifestHandle = 0;
    }
    
    if (m_currentHandle) {
        ASSERT(!m_manifestHandle);
        ASSERT(m_cacheBeingUpdated);

        m_currentHandle->setClient(0);
        m_currentHandle->cancel();
        m_currentHandle = 0;
    }    
    
    m_cacheBeingUpdated = 0;
}    

void ApplicationCacheGroup::documentLoaderDestroyed(DocumentLoader* loader)
{
    HashSet<DocumentLoader*>::iterator it = m_associatedDocumentLoaders.find(loader);
    
    if (it != m_associatedDocumentLoaders.end()) {
        ASSERT(!m_cacheCandidates.contains(loader));
    
        m_associatedDocumentLoaders.remove(it);
    } else {
        ASSERT(m_cacheCandidates.contains(loader));
        m_cacheCandidates.remove(loader);
    }
    
    if (!m_associatedDocumentLoaders.isEmpty() || !m_cacheCandidates.isEmpty())
        return;

    // This was the last document loader referencing the cache group, so there is at most one cache remaining in the group.
    // If there are none, this was an initial cache attempt.

    if (m_caches.size() == 1) {
        ASSERT(m_caches.contains(m_newestCache.get()));

        // Release our reference to the newest cache. This could cause us to be deleted.
        // Any ongoing updates will be stopped from destructor.
        m_savedNewestCachePointer = m_newestCache.release().get();

        return;
    }
    
    // There is an initial cache attempt in progress
    ASSERT(m_cacheBeingUpdated);
    ASSERT(m_caches.size() == 0);
    
    // Delete ourselves, causing the cache attempt to be stopped.
    delete this;
}    

void ApplicationCacheGroup::cacheDestroyed(ApplicationCache* cache)
{
    ASSERT(m_caches.contains(cache));
    
    m_caches.remove(cache);
    
    if (cache != m_savedNewestCachePointer)
        cacheStorage().remove(cache);

    // FIXME: When the newest cache is destroyed, we'd rather clear m_savedNewestCachePointer to avoid having a hanging reference - but currently, ApplicationCacheStorage checks the value as a flag.

    if (m_caches.isEmpty())
        delete this;
}

void ApplicationCacheGroup::setNewestCache(PassRefPtr<ApplicationCache> newestCache)
{ 
    ASSERT(!m_newestCache);
    ASSERT(!m_caches.contains(newestCache.get()));
    ASSERT(!newestCache->group());
           
    m_newestCache = newestCache; 
    m_caches.add(m_newestCache.get());
    m_newestCache->setGroup(this);
}

void ApplicationCacheGroup::update(Frame* frame)
{
    if (m_status == Checking || m_status == Downloading) 
        return;

    ASSERT(!m_frame);
    m_frame = frame;

    m_status = Checking;

    callListenersOnAssociatedDocuments(&DOMApplicationCache::callCheckingListener);
    
    ASSERT(!m_manifestHandle);
    ASSERT(!m_manifestResource);
    
    // FIXME: Handle defer loading
    
    ResourceRequest request(m_manifestURL);
    m_frame->loader()->applyUserAgent(request);
    
    m_manifestHandle = ResourceHandle::create(request, this, m_frame, false, true, false);
}
 
void ApplicationCacheGroup::didReceiveResponse(ResourceHandle* handle, const ResourceResponse& response)
{
    if (handle == m_manifestHandle) {
        didReceiveManifestResponse(response);
        return;
    }
    
    ASSERT(handle == m_currentHandle);
    
    int statusCode = response.httpStatusCode() / 100;
    if (statusCode == 4 || statusCode == 5) {
        // Note that cacheUpdateFailed() can cause the cache group to be deleted.
        cacheUpdateFailed();
        return;
    }
    
    const KURL& url = handle->request().url();
    
    ASSERT(!m_currentResource);
    ASSERT(m_pendingEntries.contains(url));
    
    unsigned type = m_pendingEntries.get(url);
    
    // If this is an initial cache attempt, we should not get implicit resources delivered here.
    if (!m_newestCache)
        ASSERT(!(type & ApplicationCacheResource::Implicit));
    
    m_currentResource = ApplicationCacheResource::create(url, response, type);
}

void ApplicationCacheGroup::didReceiveData(ResourceHandle* handle, const char* data, int length, int)
{
    if (handle == m_manifestHandle) {
        didReceiveManifestData(data, length);
        return;
    }
    
    ASSERT(handle == m_currentHandle);
    
    ASSERT(m_currentResource);
    m_currentResource->data()->append(data, length);
}

void ApplicationCacheGroup::didFinishLoading(ResourceHandle* handle)
{
    if (handle == m_manifestHandle) {
        didFinishLoadingManifest();
        return;
    }
 
    ASSERT(m_currentHandle == handle);
    ASSERT(m_pendingEntries.contains(handle->request().url()));
    
    m_pendingEntries.remove(handle->request().url());
    
    ASSERT(m_cacheBeingUpdated);

    m_cacheBeingUpdated->addResource(m_currentResource.release());
    m_currentHandle = 0;
    
    // Load the next file.
    if (!m_pendingEntries.isEmpty()) {
        startLoadingEntry();
        return;
    }
    
    checkIfLoadIsComplete();
}

void ApplicationCacheGroup::didFail(ResourceHandle* handle, const ResourceError&)
{
    if (handle == m_manifestHandle) {
        didFailToLoadManifest();
        return;
    }
    
    // Note that cacheUpdateFailed() can cause the cache group to be deleted.
    cacheUpdateFailed();
}

void ApplicationCacheGroup::didReceiveManifestResponse(const ResourceResponse& response)
{
    int statusCode = response.httpStatusCode() / 100;

    if (statusCode == 4 || statusCode == 5 || 
        !equalIgnoringCase(response.mimeType(), "text/cache-manifest")) {
        didFailToLoadManifest();
        return;
    }
    
    ASSERT(!m_manifestResource);
    ASSERT(m_manifestHandle);
    m_manifestResource = ApplicationCacheResource::create(m_manifestHandle->request().url(), response, 
                                                          ApplicationCacheResource::Manifest);
}

void ApplicationCacheGroup::didReceiveManifestData(const char* data, int length)
{
    ASSERT(m_manifestResource);
    m_manifestResource->data()->append(data, length);
}

void ApplicationCacheGroup::didFinishLoadingManifest()
{
    if (!m_manifestResource) {
        didFailToLoadManifest();
        return;
    }

    bool isUpgradeAttempt = m_newestCache;
    
    m_manifestHandle = 0;

    // Check if the manifest is byte-for-byte identical.
    if (isUpgradeAttempt) {
        ApplicationCacheResource* newestManifest = m_newestCache->manifestResource();
        ASSERT(newestManifest);
    
        if (newestManifest->data()->size() == m_manifestResource->data()->size() &&
            !memcmp(newestManifest->data()->data(), m_manifestResource->data()->data(), newestManifest->data()->size())) {
            
            callListenersOnAssociatedDocuments(&DOMApplicationCache::callNoUpdateListener);
         
            m_status = Idle;
            m_frame = 0;
            m_manifestResource = 0;
            return;
        }
    }
    
    Manifest manifest;
    if (!parseManifest(m_manifestURL, m_manifestResource->data()->data(), m_manifestResource->data()->size(), manifest)) {
        didFailToLoadManifest();
        return;
    }
        
    // We have the manifest, now download the resources.
    m_status = Downloading;
    
    callListenersOnAssociatedDocuments(&DOMApplicationCache::callDownloadingListener);

#ifndef NDEBUG
    // We should only have implicit entries.
    {
        EntryMap::const_iterator end = m_pendingEntries.end();
        for (EntryMap::const_iterator it = m_pendingEntries.begin(); it != end; ++it)
            ASSERT(it->second & ApplicationCacheResource::Implicit);
    }
#endif
    
    if (isUpgradeAttempt) {
        ASSERT(!m_cacheBeingUpdated);
        
        m_cacheBeingUpdated = ApplicationCache::create();
        
        ApplicationCache::ResourceMap::const_iterator end = m_newestCache->end();
        for (ApplicationCache::ResourceMap::const_iterator it = m_newestCache->begin(); it != end; ++it) {
            unsigned type = it->second->type();
            if (type & (ApplicationCacheResource::Implicit | ApplicationCacheResource::Dynamic))
                addEntry(it->first, type);
        }
    }
    
    HashSet<String>::const_iterator end = manifest.explicitURLs.end();
    for (HashSet<String>::const_iterator it = manifest.explicitURLs.begin(); it != end; ++it)
        addEntry(*it, ApplicationCacheResource::Explicit);

    size_t fallbackCount = manifest.fallbackURLs.size();
    for (size_t i = 0; i  < fallbackCount; ++i)
        addEntry(manifest.fallbackURLs[i].second, ApplicationCacheResource::Fallback);
    
    m_cacheBeingUpdated->setOnlineWhitelist(manifest.onlineWhitelistedURLs);
    m_cacheBeingUpdated->setFallbackURLs(manifest.fallbackURLs);
    
    startLoadingEntry();
}

void ApplicationCacheGroup::cacheUpdateFailed()
{
    stopLoading();
        
    callListenersOnAssociatedDocuments(&DOMApplicationCache::callErrorListener);

    m_pendingEntries.clear();
    m_manifestResource = 0;

    while (!m_cacheCandidates.isEmpty()) {
        HashSet<DocumentLoader*>::iterator it = m_cacheCandidates.begin();
        
        ASSERT((*it)->candidateApplicationCacheGroup() == this);
        (*it)->setCandidateApplicationCacheGroup(0);
        m_cacheCandidates.remove(it);
    }
    
    m_status = Idle;    
    m_frame = 0;
    
    // If there are no associated caches, delete ourselves
    if (m_associatedDocumentLoaders.isEmpty())
        delete this;
}
    
    
void ApplicationCacheGroup::didFailToLoadManifest()
{
    // Note that cacheUpdateFailed() can cause the cache group to be deleted.
    cacheUpdateFailed();
}

void ApplicationCacheGroup::checkIfLoadIsComplete()
{
    ASSERT(m_cacheBeingUpdated);
    
    if (m_manifestHandle)
        return;
    
    if (!m_pendingEntries.isEmpty())
        return;
    
    // We're done    
    bool isUpgradeAttempt = m_newestCache;
    
    m_cacheBeingUpdated->setManifestResource(m_manifestResource.release());
    
    Vector<RefPtr<DocumentLoader> > documentLoaders;

    if (isUpgradeAttempt) {
        ASSERT(m_cacheCandidates.isEmpty());
        
        copyToVector(m_associatedDocumentLoaders, documentLoaders);
    } else {
        while (!m_cacheCandidates.isEmpty()) {
            HashSet<DocumentLoader*>::iterator it = m_cacheCandidates.begin();
            
            DocumentLoader* loader = *it;
            ASSERT(!loader->applicationCache());
            ASSERT(loader->candidateApplicationCacheGroup() == this);
            
            associateDocumentLoaderWithCache(loader, m_cacheBeingUpdated.get());
    
            documentLoaders.append(loader);

            m_cacheCandidates.remove(it);
        }
    }
    
    setNewestCache(m_cacheBeingUpdated.release());
        
    // Store the cache 
    cacheStorage().storeNewestCache(this);
    
    callListeners(isUpgradeAttempt ? &DOMApplicationCache::callUpdateReadyListener : &DOMApplicationCache::callCachedListener, 
                  documentLoaders);

    m_status = Idle;
    m_frame = 0;
}

void ApplicationCacheGroup::startLoadingEntry()
{
    ASSERT(m_cacheBeingUpdated);

    if (m_pendingEntries.isEmpty()) {
        checkIfLoadIsComplete();
        return;
    }
    
    EntryMap::const_iterator it = m_pendingEntries.begin();

    // If this is an initial cache attempt, we do not want to fetch any implicit entries,
    // since those are fed to us by the normal loader machinery.
    if (!m_newestCache) {
        // Get the first URL in the entry table that is not implicit
        EntryMap::const_iterator end = m_pendingEntries.end();
    
        while (it->second & ApplicationCacheResource::Implicit) {
            ++it;

            if (it == end)
                return;
        }
    }
    
    callListenersOnAssociatedDocuments(&DOMApplicationCache::callProgressListener);

    // FIXME: If this is an upgrade attempt, the newest cache should be used as an HTTP cache.
    
    ASSERT(!m_currentHandle);
    
    ResourceRequest request(it->first);
    m_frame->loader()->applyUserAgent(request);

    m_currentHandle = ResourceHandle::create(request, this, m_frame, false, true, false);
}

void ApplicationCacheGroup::addEntry(const String& url, unsigned type)
{
    ASSERT(m_cacheBeingUpdated);
    
    // Don't add the URL if we already have an implicit resource in the cache
    if (ApplicationCacheResource* resource = m_cacheBeingUpdated->resourceForURL(url)) {
        ASSERT(resource->type() & ApplicationCacheResource::Implicit);
    
        resource->addType(type);
        return;
    }

    // Don't add the URL if it's the same as the manifest URL.
    if (m_manifestResource && m_manifestResource->url() == url) {
        m_manifestResource->addType(type);
        return;
    }
    
    pair<EntryMap::iterator, bool> result = m_pendingEntries.add(url, type);
    
    if (!result.second)
        result.first->second |= type;
}

void ApplicationCacheGroup::associateDocumentLoaderWithCache(DocumentLoader* loader, ApplicationCache* cache)
{
    // If teardown started already, revive the group.
    if (m_savedNewestCachePointer) {
        ASSERT(cache == m_savedNewestCachePointer);
        m_newestCache = m_savedNewestCachePointer;
        m_savedNewestCachePointer = 0;
    }

    loader->setApplicationCache(cache);

    ASSERT(!m_associatedDocumentLoaders.contains(loader));
    m_associatedDocumentLoaders.add(loader);
}
 
void ApplicationCacheGroup::callListenersOnAssociatedDocuments(ListenerFunction listenerFunction)
{
    Vector<RefPtr<DocumentLoader> > loaders;
    copyToVector(m_associatedDocumentLoaders, loaders);

    callListeners(listenerFunction, loaders);
}
    
void ApplicationCacheGroup::callListeners(ListenerFunction listenerFunction, const Vector<RefPtr<DocumentLoader> >& loaders)
{
    for (unsigned i = 0; i < loaders.size(); i++) {
        Frame* frame = loaders[i]->frame();
        if (!frame)
            continue;
        
        ASSERT(frame->loader()->documentLoader() == loaders[i]);
        DOMWindow* window = frame->domWindow();
        
        if (DOMApplicationCache* domCache = window->optionalApplicationCache())
            (domCache->*listenerFunction)();
    }    
}

void ApplicationCacheGroup::clearStorageID()
{
    m_storageID = 0;
    
    HashSet<ApplicationCache*>::const_iterator end = m_caches.end();
    for (HashSet<ApplicationCache*>::const_iterator it = m_caches.begin(); it != end; ++it)
        (*it)->clearStorageID();
}
    

}

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)
