/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
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

#pragma once

#include "ApplicationCacheResourceLoader.h"
#include "DOMApplicationCache.h"
#include <wtf/Noncopyable.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ApplicationCache;
class ApplicationCacheResource;
class ApplicationCacheStorage;
class Document;
class DocumentLoader;
class Frame;
class SecurityOrigin;

enum ApplicationCacheUpdateOption {
    ApplicationCacheUpdateWithBrowsingContext,
    ApplicationCacheUpdateWithoutBrowsingContext
};

class ApplicationCacheGroup : public CanMakeWeakPtr<ApplicationCacheGroup> {
    WTF_MAKE_NONCOPYABLE(ApplicationCacheGroup);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ApplicationCacheGroup(Ref<ApplicationCacheStorage>&&, const URL& manifestURL);
    virtual ~ApplicationCacheGroup();
    
    enum UpdateStatus { Idle, Checking, Downloading };

    static ApplicationCache* cacheForMainRequest(const ResourceRequest&, DocumentLoader*);
    static ApplicationCache* fallbackCacheForMainRequest(const ResourceRequest&, DocumentLoader*);
    
    static void selectCache(Frame&, const URL& manifestURL);
    static void selectCacheWithoutManifestURL(Frame&);

    ApplicationCacheStorage& storage() { return m_storage; }
    const URL& manifestURL() const { return m_manifestURL; }
    const SecurityOrigin& origin() const { return m_origin.get(); }
    UpdateStatus updateStatus() const { return m_updateStatus; }
    void setUpdateStatus(UpdateStatus status);

    void setStorageID(unsigned storageID) { m_storageID = storageID; }
    unsigned storageID() const { return m_storageID; }
    void clearStorageID();
    
    void update(Frame&, ApplicationCacheUpdateOption); // FIXME: Frame should not be needed when updating without browsing context.
    void cacheDestroyed(ApplicationCache&);
    
    void abort(Frame&);

    bool cacheIsComplete(ApplicationCache& cache) { return m_caches.contains(&cache); }

    void stopLoadingInFrame(Frame&);

    ApplicationCache* newestCache() const { return m_newestCache.get(); }
    void setNewestCache(Ref<ApplicationCache>&&);

    void makeObsolete();
    bool isObsolete() const { return m_isObsolete; }

    void finishedLoadingMainResource(DocumentLoader&);
    void failedLoadingMainResource(DocumentLoader&);

    void disassociateDocumentLoader(DocumentLoader&);

private:
    static void postListenerTask(const AtomString& eventType, const HashSet<DocumentLoader*>& set) { postListenerTask(eventType, 0, 0, set); }
    static void postListenerTask(const AtomString& eventType, DocumentLoader& loader)  { postListenerTask(eventType, 0, 0, loader); }
    static void postListenerTask(const AtomString& eventType, int progressTotal, int progressDone, const HashSet<DocumentLoader*>&);
    static void postListenerTask(const AtomString& eventType, int progressTotal, int progressDone, DocumentLoader&);

    void scheduleReachedMaxAppCacheSizeCallback();

    void didFinishLoadingManifest();
    void didFailLoadingManifest(ApplicationCacheResourceLoader::Error);

    void didFailLoadingEntry(ApplicationCacheResourceLoader::Error, const URL&, unsigned type);
    void didFinishLoadingEntry(const URL&);

    void didReachMaxAppCacheSize();
    void didReachOriginQuota(int64_t totalSpaceNeeded);
    
    void startLoadingEntry();
    void deliverDelayedMainResources();
    void checkIfLoadIsComplete();
    void cacheUpdateFailed();
    void recalculateAvailableSpaceInQuota();
    void manifestNotFound();
    
    void addEntry(const String&, unsigned type);
    
    void associateDocumentLoaderWithCache(DocumentLoader*, ApplicationCache*);
    
    void stopLoading();

    ResourceRequest createRequest(URL&&, ApplicationCacheResource*);

    Ref<ApplicationCacheStorage> m_storage;

    URL m_manifestURL;
    Ref<SecurityOrigin> m_origin;
    UpdateStatus m_updateStatus { Idle };
    
    // This is the newest complete cache in the group.
    RefPtr<ApplicationCache> m_newestCache;
    
    // All complete caches in this cache group.
    HashSet<ApplicationCache*> m_caches;
    
    // The cache being updated (if any). Note that cache updating does not immediately create a new
    // ApplicationCache object, so this may be null even when update status is not Idle.
    RefPtr<ApplicationCache> m_cacheBeingUpdated;

    // List of pending master entries, used during the update process to ensure that new master entries are cached.
    HashSet<DocumentLoader*> m_pendingMasterResourceLoaders;
    // How many of the above pending master entries have not yet finished downloading.
    int m_downloadingPendingMasterResourceLoadersCount { 0 };
    
    // These are all the document loaders that are associated with a cache in this group.
    HashSet<DocumentLoader*> m_associatedDocumentLoaders;

    // The URLs and types of pending cache entries.
    HashMap<String, unsigned> m_pendingEntries;
    
    // The total number of items to be processed to update the cache group and the number that have been done.
    int m_progressTotal { 0 };
    int m_progressDone { 0 };

    // Frame used for fetching resources when updating.
    // FIXME: An update started by a particular frame should not stop if it is destroyed, but there are other frames associated with the same cache group.
    WeakPtr<Frame> m_frame;
  
    // An obsolete cache group is never stored, but the opposite is not true - storing may fail for multiple reasons, such as exceeding disk quota.
    unsigned m_storageID { 0 };
    bool m_isObsolete { false };

    // During update, this is used to handle asynchronously arriving results.
    enum CompletionType {
        None,
        NoUpdate,
        Failure,
        Completed
    };
    CompletionType m_completionType { None };

    // This flag is set immediately after the ChromeClient::reachedMaxAppCacheSize() callback is invoked as a result of the storage layer failing to save a cache
    // due to reaching the maximum size of the application cache database file. This flag is used by ApplicationCacheGroup::checkIfLoadIsComplete() to decide
    // the course of action in case of this failure (i.e. call the ChromeClient callback or run the failure steps).
    bool m_calledReachedMaxAppCacheSize { false };
    
    RefPtr<ApplicationCacheResource> m_currentResource;
    RefPtr<ApplicationCacheResourceLoader> m_entryLoader;
    unsigned long m_currentResourceIdentifier;

    RefPtr<ApplicationCacheResource> m_manifestResource;
    RefPtr<ApplicationCacheResourceLoader> m_manifestLoader;

    int64_t m_availableSpaceInQuota;
    bool m_originQuotaExceededPreviously { false };

    friend class ChromeClientCallbackTimer;
};

} // namespace WebCore
