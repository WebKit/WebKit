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
#ifndef DocumentLoader_H_
#define DocumentLoader_H_

#include "NavigationAction.h"
#include "Shared.h"
#include "PlatformString.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <wtf/Vector.h>

#if PLATFORM(MAC)

#include "RetainPtr.h"

#ifndef __OBJC__

class NSData;
class NSDictionary;
class NSError;
class NSMutableURLRequest;
class NSURLRequest;
class NSURLResponse;

#endif

#endif

namespace WebCore {

    class Frame;
    class FrameLoader;
    class KURL;

    typedef Vector<ResourceResponse> ResponseVector;

    class DocumentLoader : public Shared<DocumentLoader> {
    public:
        DocumentLoader(const ResourceRequest&);
        virtual ~DocumentLoader();

        void setFrame(Frame*);
        virtual void attachToFrame();
        virtual void detachFromFrame();

        FrameLoader* frameLoader() const;
#if PLATFORM(MAC)
        NSData *mainResourceData() const;
#endif
        const ResourceRequest& originalRequest() const;
        const ResourceRequest& originalRequestCopy() const;

        const ResourceRequest& request() const;
        ResourceRequest& request();
        void setRequest(const ResourceRequest&);
        const ResourceRequest& actualRequest() const;
        ResourceRequest& actualRequest();
        const ResourceRequest& initialRequest() const;

        const KURL& URL() const;
        const KURL unreachableURL() const;
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
#if PLATFORM(MAC)
        const ResourceResponse& response() const;
        const ResourceError& mainDocumentError() const;
        void mainReceivedError(const ResourceError&, bool isComplete);
        void setResponse(const ResourceResponse);
#endif
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
        KURL URLForHistory() const;

    private:
#if PLATFORM(MAC)
        void setMainResourceData(NSData *);
#endif
        void setupForReplace();
        void commitIfReady();
        void clearErrors();
#if PLATFORM(MAC)
        void setMainDocumentError(const ResourceError&);
        void commitLoad(const char*, int);
#endif
        bool doesProgressiveLoad(const String& MIMEType) const;

        Frame* m_frame;

#if PLATFORM(MAC)
        RetainPtr<NSData> m_mainResourceData;
#endif

        // A reference to actual request used to create the data source.
        // This should only be used by the resourceLoadDelegate's
        // identifierForInitialRequest:fromDatasource: method. It is
        // not guaranteed to remain unchanged, as requests are mutable.
        ResourceRequest m_originalRequest;    

        // A copy of the original request used to create the data source.
        // We have to copy the request because requests are mutable.
        ResourceRequest m_originalRequestCopy;
        
        // The 'working' request. It may be mutated
        // several times from the original request to include additional
        // headers, cookie information, canonicalization and redirects.
        ResourceRequest m_request;

        mutable ResourceRequest m_externalRequest;

#if PLATFORM(MAC)
        RetainPtr<NSURLResponse> m_response;
    
        ResourceError m_mainDocumentError;    
#endif

        bool m_committed;
        bool m_stopping;
        bool m_loading;
        bool m_gotFirstByte;
        bool m_primaryLoadComplete;
        bool m_isClientRedirect;

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

#endif // DocumentLoader_H_
