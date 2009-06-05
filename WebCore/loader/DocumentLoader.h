/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef DocumentLoader_h
#define DocumentLoader_h

#include "NavigationAction.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SubstituteData.h"
#include "Timer.h"

namespace WebCore {

    class ApplicationCache;
    class ApplicationCacheGroup;
    class ApplicationCacheResource;
    class Archive;
    class ArchiveResource;
    class ArchiveResourceCollection;
    class CachedPage;
    class Frame;
    class FrameLoader;
    class MainResourceLoader;
    class ResourceLoader;
    class SchedulePair;
    class SharedBuffer;
    class SubstituteResource;

    typedef HashSet<RefPtr<ResourceLoader> > ResourceLoaderSet;
    typedef Vector<ResourceResponse> ResponseVector;

    class DocumentLoader : public RefCounted<DocumentLoader> {
    public:
        static PassRefPtr<DocumentLoader> create(const ResourceRequest& request, const SubstituteData& data)
        {
            return adoptRef(new DocumentLoader(request, data));
        }
        virtual ~DocumentLoader();

        void setFrame(Frame*);
        Frame* frame() const { return m_frame; }

        virtual void attachToFrame();
        virtual void detachFromFrame();

        FrameLoader* frameLoader() const;
        MainResourceLoader* mainResourceLoader() const { return m_mainResourceLoader.get(); }
        PassRefPtr<SharedBuffer> mainResourceData() const;

        const ResourceRequest& originalRequest() const;
        const ResourceRequest& originalRequestCopy() const;

        const ResourceRequest& request() const;
        ResourceRequest& request();
        void setRequest(const ResourceRequest&);

        const SubstituteData& substituteData() const { return m_substituteData; }

        const KURL& url() const;
        const KURL& unreachableURL() const;

        const KURL& originalURL() const;
        const KURL& requestURL() const;
        const KURL& responseURL() const;
        const String& responseMIMEType() const;
        
        void replaceRequestURLForAnchorScroll(const KURL&);
        bool isStopping() const { return m_isStopping; }
        void stopLoading(DatabasePolicy = DatabasePolicyStop);
        void setCommitted(bool committed) { m_committed = committed; }
        bool isCommitted() const { return m_committed; }
        bool isLoading() const { return m_loading; }
        void setLoading(bool loading) { m_loading = loading; }
        void updateLoading();
        void receivedData(const char*, int);
        void setupForReplaceByMIMEType(const String& newMIMEType);
        void finishedLoading();
        const ResourceResponse& response() const { return m_response; }
        const ResourceError& mainDocumentError() const { return m_mainDocumentError; }
        void mainReceivedError(const ResourceError&, bool isComplete);
        void setResponse(const ResourceResponse& response) { m_response = response; }
        void prepareForLoadStart();
        bool isClientRedirect() const { return m_isClientRedirect; }
        void setIsClientRedirect(bool isClientRedirect) { m_isClientRedirect = isClientRedirect; }
        bool isLoadingInAPISense() const;
        void setPrimaryLoadComplete(bool);
        void setTitle(const String&);
        const String& overrideEncoding() const { return m_overrideEncoding; }

#if PLATFORM(MAC)
        void schedule(SchedulePair*);
        void unschedule(SchedulePair*);
#endif

        void addAllArchiveResources(Archive*);
        void addArchiveResource(PassRefPtr<ArchiveResource>);
        
        // Return an ArchiveResource for the URL, either creating from live data or
        // pulling from the ArchiveResourceCollection
        PassRefPtr<ArchiveResource> subresource(const KURL&) const;
        // Return the ArchiveResource for the URL only when loading an Archive
        ArchiveResource* archiveResourceForURL(const KURL&) const;
        
        PassRefPtr<Archive> popArchiveForSubframe(const String& frameName);
        void clearArchiveResources();
        void setParsedArchiveData(PassRefPtr<SharedBuffer>);
        SharedBuffer* parsedArchiveData() const;
        
        PassRefPtr<ArchiveResource> mainResource() const;
        void getSubresources(Vector<PassRefPtr<ArchiveResource> >&) const;
        
        bool scheduleArchiveLoad(ResourceLoader*, const ResourceRequest&, const KURL&);
#ifndef NDEBUG
        bool isSubstituteLoadPending(ResourceLoader*) const;
#endif
        void cancelPendingSubstituteLoad(ResourceLoader*);   
        
        void addResponse(const ResourceResponse&);
        const ResponseVector& responses() const { return m_responses; }

        const NavigationAction& triggeringAction() const { return m_triggeringAction; }
        void setTriggeringAction(const NavigationAction& action) { m_triggeringAction = action; }
        void setOverrideEncoding(const String& encoding) { m_overrideEncoding = encoding; }
        void setLastCheckedRequest(const ResourceRequest& request) { m_lastCheckedRequest = request; }
        const ResourceRequest& lastCheckedRequest()  { return m_lastCheckedRequest; }

        void stopRecordingResponses();
        const String& title() const { return m_pageTitle; }

        KURL urlForHistory() const;
        bool urlForHistoryReflectsFailure() const;

        // These accessors accomodate WebCore's somewhat fickle custom of creating history
        // items for redirects, but only sometimes. For "source" and "destination",
        // these accessors return the URL that would have been used if a history
        // item were created. This allows WebKit to link history items reflecting
        // redirects into a chain from start to finish.
        String clientRedirectSourceForHistory() const { return m_clientRedirectSourceForHistory; } // null if no client redirect occurred.
        String clientRedirectDestinationForHistory() const { return urlForHistory(); }
        void setClientRedirectSourceForHistory(const String& clientedirectSourceForHistory) { m_clientRedirectSourceForHistory = clientedirectSourceForHistory; }
        
        String serverRedirectSourceForHistory() const { return urlForHistory() == url() ? String() : urlForHistory(); } // null if no server redirect occurred.
        String serverRedirectDestinationForHistory() const { return url(); }

        bool didCreateGlobalHistoryEntry() const { return m_didCreateGlobalHistoryEntry; }
        void setDidCreateGlobalHistoryEntry(bool didCreateGlobalHistoryEntry) { m_didCreateGlobalHistoryEntry = didCreateGlobalHistoryEntry; }
        
        void loadFromCachedPage(PassRefPtr<CachedPage>);
        void setLoadingFromCachedPage(bool loading) { m_loadingFromCachedPage = loading; }
        bool isLoadingFromCachedPage() const { return m_loadingFromCachedPage; }
        
        void setDefersLoading(bool);

        bool startLoadingMainResource(unsigned long identifier);
        void cancelMainResourceLoad(const ResourceError&);
        
        void iconLoadDecisionAvailable();
        
        bool isLoadingMainResource() const;
        bool isLoadingSubresources() const;
        bool isLoadingPlugIns() const;
        bool isLoadingMultipartContent() const;

        void stopLoadingPlugIns();
        void stopLoadingSubresources();

        void addSubresourceLoader(ResourceLoader*);
        void removeSubresourceLoader(ResourceLoader*);
        void addPlugInStreamLoader(ResourceLoader*);
        void removePlugInStreamLoader(ResourceLoader*);

        void subresourceLoaderFinishedLoadingOnePart(ResourceLoader*);
        
        void setDeferMainResourceDataLoad(bool defer) { m_deferMainResourceDataLoad = defer; }
        bool deferMainResourceDataLoad() const { return m_deferMainResourceDataLoad; }
        
        void didTellClientAboutLoad(const String& url) { m_resourcesClientKnowsAbout.add(url); }
        bool haveToldClientAboutLoad(const String& url) { return m_resourcesClientKnowsAbout.contains(url); }
        void recordMemoryCacheLoadForFutureClientNotification(const String& url);
        void takeMemoryCacheLoadsForClientNotification(Vector<String>& loads);

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        bool scheduleApplicationCacheLoad(ResourceLoader*, const ResourceRequest&, const KURL& originalURL);
        bool scheduleLoadFallbackResourceFromApplicationCache(ResourceLoader*, const ResourceRequest&, ApplicationCache* = 0);
        bool shouldLoadResourceFromApplicationCache(const ResourceRequest&, ApplicationCacheResource*&);
        bool getApplicationCacheFallbackResource(const ResourceRequest&, ApplicationCacheResource*&, ApplicationCache* = 0);
        
        void setCandidateApplicationCacheGroup(ApplicationCacheGroup* group);
        ApplicationCacheGroup* candidateApplicationCacheGroup() const { return m_candidateApplicationCacheGroup; }
        
        void setApplicationCache(PassRefPtr<ApplicationCache> applicationCache);
        ApplicationCache* applicationCache() const { return m_applicationCache.get(); }

        ApplicationCache* mainResourceApplicationCache() const;
#endif

    protected:
        DocumentLoader(const ResourceRequest&, const SubstituteData&);

        bool m_deferMainResourceDataLoad;

    private:
        void setupForReplace();
        void commitIfReady();
        void clearErrors();
        void setMainDocumentError(const ResourceError&);
        void commitLoad(const char*, int);
        bool doesProgressiveLoad(const String& MIMEType) const;

        void deliverSubstituteResourcesAfterDelay();
        void substituteResourceDeliveryTimerFired(Timer<DocumentLoader>*);
                
        Frame* m_frame;

        RefPtr<MainResourceLoader> m_mainResourceLoader;
        ResourceLoaderSet m_subresourceLoaders;
        ResourceLoaderSet m_multipartSubresourceLoaders;
        ResourceLoaderSet m_plugInStreamLoaders;

        RefPtr<SharedBuffer> m_mainResourceData;

        // A reference to actual request used to create the data source.
        // This should only be used by the resourceLoadDelegate's
        // identifierForInitialRequest:fromDatasource: method. It is
        // not guaranteed to remain unchanged, as requests are mutable.
        ResourceRequest m_originalRequest;   

        SubstituteData m_substituteData;

        // A copy of the original request used to create the data source.
        // We have to copy the request because requests are mutable.
        ResourceRequest m_originalRequestCopy;
        
        // The 'working' request. It may be mutated
        // several times from the original request to include additional
        // headers, cookie information, canonicalization and redirects.
        ResourceRequest m_request;

        ResourceResponse m_response;
    
        ResourceError m_mainDocumentError;    

        bool m_committed;
        bool m_isStopping;
        bool m_loading;
        bool m_gotFirstByte;
        bool m_primaryLoadComplete;
        bool m_isClientRedirect;
        bool m_loadingFromCachedPage;

        String m_pageTitle;

        String m_overrideEncoding;

        // The action that triggered loading - we keep this around for the
        // benefit of the various policy handlers.
        NavigationAction m_triggeringAction;

        // The last request that we checked click policy for - kept around
        // so we can avoid asking again needlessly.
        ResourceRequest m_lastCheckedRequest;

        // We retain all the received responses so we can play back the
        // WebResourceLoadDelegate messages if the item is loaded from the
        // page cache.
        ResponseVector m_responses;
        bool m_stopRecordingResponses;
        
        typedef HashMap<RefPtr<ResourceLoader>, RefPtr<SubstituteResource> > SubstituteResourceMap;
        SubstituteResourceMap m_pendingSubstituteResources;
        Timer<DocumentLoader> m_substituteResourceDeliveryTimer;
                
        OwnPtr<ArchiveResourceCollection> m_archiveResourceCollection;
        RefPtr<SharedBuffer> m_parsedArchiveData;

        HashSet<String> m_resourcesClientKnowsAbout;
        Vector<String> m_resourcesLoadedFromMemoryCacheForClientNotification;
        
        String m_clientRedirectSourceForHistory;
        bool m_didCreateGlobalHistoryEntry;

#if ENABLE(OFFLINE_WEB_APPLICATIONS)  
        // The application cache that the document loader is associated with (if any).
        RefPtr<ApplicationCache> m_applicationCache;
        
        // Before an application cache has finished loading, this will be the candidate application
        // group that the document loader is associated with.
        ApplicationCacheGroup* m_candidateApplicationCacheGroup;
        
        // Once the main resource has finished loading, this is the application cache it was loaded from (if any).
        RefPtr<ApplicationCache> m_mainResourceApplicationCache;
#endif
    };

    inline void DocumentLoader::recordMemoryCacheLoadForFutureClientNotification(const String& url)
    {
        m_resourcesLoadedFromMemoryCacheForClientNotification.append(url);
    }

    inline void DocumentLoader::takeMemoryCacheLoadsForClientNotification(Vector<String>& loadsSet)
    {
        loadsSet.swap(m_resourcesLoadedFromMemoryCacheForClientNotification);
        m_resourcesLoadedFromMemoryCacheForClientNotification.clear();
    }

}

#endif // DocumentLoader_h
