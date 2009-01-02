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

#ifndef ApplicationCacheGroup_h
#define ApplicationCacheGroup_h

#if ENABLE(OFFLINE_WEB_APPLICATIONS)

#include <wtf/Noncopyable.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

#include "KURL.h"
#include "PlatformString.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "SharedBuffer.h"

namespace WebCore {

class ApplicationCache;
class ApplicationCacheResource;
class DOMApplicationCache;
class Document;
class DocumentLoader;
class Frame;

class ApplicationCacheGroup : Noncopyable, ResourceHandleClient {
public:
    ApplicationCacheGroup(const KURL& manifestURL, bool isCopy = false);    
    ~ApplicationCacheGroup();
    
    enum Status { Idle, Checking, Downloading };

    static ApplicationCache* cacheForMainRequest(const ResourceRequest&, DocumentLoader*);
    static ApplicationCache* fallbackCacheForMainRequest(const ResourceRequest&, DocumentLoader*);
    
    static void selectCache(Frame*, const KURL& manifestURL);
    static void selectCacheWithoutManifestURL(Frame*);
    
    const KURL& manifestURL() const { return m_manifestURL; }
    Status status() const { return m_status; }
    
    void setStorageID(unsigned storageID) { m_storageID = storageID; }
    unsigned storageID() const { return m_storageID; }
    void clearStorageID();
    
    void update(Frame*);
    void cacheDestroyed(ApplicationCache*);
        
    ApplicationCache* newestCache() const { return m_newestCache.get(); }
    ApplicationCache* savedNewestCachePointer() const { return m_savedNewestCachePointer; }
    
    void finishedLoadingMainResource(DocumentLoader*);
    void failedLoadingMainResource(DocumentLoader*);
    void documentLoaderDestroyed(DocumentLoader*);

    void setNewestCache(PassRefPtr<ApplicationCache> newestCache);

    bool isCopy() const { return m_isCopy; }
private:
    typedef void (DOMApplicationCache::*ListenerFunction)();
    void callListenersOnAssociatedDocuments(ListenerFunction);
    void callListeners(ListenerFunction, const Vector<RefPtr<DocumentLoader> >& loaders);
    
    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
    virtual void didReceiveData(ResourceHandle*, const char*, int, int lengthReceived);
    virtual void didFinishLoading(ResourceHandle*);
    virtual void didFail(ResourceHandle*, const ResourceError&);

    void didReceiveManifestResponse(const ResourceResponse&);
    void didReceiveManifestData(const char*, int);
    void didFinishLoadingManifest();
    void didFailToLoadManifest();
    
    void startLoadingEntry();
    void checkIfLoadIsComplete();
    void cacheUpdateFailed();
    
    void addEntry(const String&, unsigned type);
    
    void associateDocumentLoaderWithCache(DocumentLoader*, ApplicationCache*);
    
    void stopLoading();
    
    KURL m_manifestURL;
    Status m_status;
    
    // This is the newest cache in the group.
    RefPtr<ApplicationCache> m_newestCache;
    
    // During tear-down we save the pointer to the newest cache to prevent reference cycles.
    ApplicationCache* m_savedNewestCachePointer;
    
    // The caches in this cache group.
    HashSet<ApplicationCache*> m_caches;
    
    // The cache being updated (if any). Note that cache updating does not immediately create a new
    // ApplicationCache object, so this may be null even when status is not Idle.
    RefPtr<ApplicationCache> m_cacheBeingUpdated;

    // When a cache group does not yet have a complete cache, this contains the document loaders
    // that should be associated with the cache once it has been downloaded.
    HashSet<DocumentLoader*> m_cacheCandidates;
    
    // These are all the document loaders that are associated with a cache in this group.
    HashSet<DocumentLoader*> m_associatedDocumentLoaders;
    
    // The URLs and types of pending cache entries.
    typedef HashMap<String, unsigned> EntryMap;
    EntryMap m_pendingEntries;

    // Frame used for fetching resources when updating
    Frame* m_frame;
  
    unsigned m_storageID;
  
    // Whether this cache group is a copy that's only used for transferring the cache to another file.
    bool m_isCopy;
    
    RefPtr<ResourceHandle> m_currentHandle;
    RefPtr<ApplicationCacheResource> m_currentResource;
    
    RefPtr<ApplicationCacheResource> m_manifestResource;
    RefPtr<ResourceHandle> m_manifestHandle;
};

} // namespace WebCore

#endif // ENABLE(OFFLINE_WEB_APPLICATIONS)

#endif // ApplicationCacheGroup_h
