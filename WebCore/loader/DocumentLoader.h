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

#ifndef DocumentLoader_h
#define DocumentLoader_h

#include "IconDatabase.h"
#include "NavigationAction.h"
#include <wtf/RefCounted.h>
#include "PlatformString.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SubstituteData.h"
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

    class CachedPage;
    class Frame;
    class FrameLoader;
    class HistoryItem;
    class KURL;
    class MainResourceLoader;
    class ResourceLoader;
    class SharedBuffer;
    class SubstituteData;

    typedef HashSet<RefPtr<ResourceLoader> > ResourceLoaderSet;
    typedef Vector<ResourceResponse> ResponseVector;

    class DocumentLoader : public RefCounted<DocumentLoader> {
    public:
        DocumentLoader(const ResourceRequest&, const SubstituteData&);
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
        const ResourceRequest& actualRequest() const;
        ResourceRequest& actualRequest();
        const ResourceRequest& initialRequest() const;

        const SubstituteData& substituteData() const { return m_substituteData; }

        const KURL& url() const;
        const KURL& unreachableURL() const;

        KURL originalURL() const;
        KURL requestURL() const;
        KURL responseURL() const;
        String responseMIMEType() const;
        
        void replaceRequestURLForAnchorScroll(const KURL&);
        bool isStopping() const;
        void stopLoading();
        void setCommitted(bool);
        bool isCommitted() const;
        bool isLoading() const;
        void setLoading(bool);
        void updateLoading();
        void receivedData(const char*, int);
        void setupForReplaceByMIMEType(const String& newMIMEType);
        void finishedLoading();
        const ResourceResponse& response() const;
        const ResourceError& mainDocumentError() const;
        void mainReceivedError(const ResourceError&, bool isComplete);
        void setResponse(const ResourceResponse& response);
        void prepareForLoadStart();
        bool isClientRedirect() const;
        void setIsClientRedirect(bool);
        bool isLoadingInAPISense() const;
        void setPrimaryLoadComplete(bool);
        void setTitle(const String&);
        String overrideEncoding() const;

        void addResponse(const ResourceResponse&);
        const ResponseVector& responses() const;

        const NavigationAction& triggeringAction() const;
        void setTriggeringAction(const NavigationAction&);
        void setOverrideEncoding(const String&);
        void setLastCheckedRequest(const ResourceRequest& request);
        const ResourceRequest& lastCheckedRequest() const;

        void stopRecordingResponses();
        String title() const;
        KURL urlForHistory() const;
        
        void loadFromCachedPage(PassRefPtr<CachedPage>);
        void setLoadingFromCachedPage(bool);
        bool isLoadingFromCachedPage() const;
        
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
        
        bool deferMainResourceDataLoad() const { return m_deferMainResourceDataLoad; }
    protected:
        bool m_deferMainResourceDataLoad;

    private:
        void setupForReplace();
        void commitIfReady();
        void clearErrors();
        void setMainDocumentError(const ResourceError&);
        void commitLoad(const char*, int);
        bool doesProgressiveLoad(const String& MIMEType) const;

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

        mutable ResourceRequest m_externalRequest;

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
    };

}

#endif // DocumentLoader_h
