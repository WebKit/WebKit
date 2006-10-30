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

#include "Shared.h"
#include "PlatformString.h"
#include <wtf/Vector.h>

#if PLATFORM(MAC)

#include "RetainPtr.h"

#ifndef __OBJC__

class NSData;
class NSDictionary;
class NSError;
class NSMutableURLRequest;
class NSURL;
class NSURLRequest;
class NSURLResponse;

#endif

#endif

namespace WebCore {

    class Frame;
    class FrameLoader;

#if PLATFORM(MAC)
    typedef Vector<RetainPtr<NSURLResponse> > ResponseVector;
#endif

    class DocumentLoader : public Shared<DocumentLoader> {
#if PLATFORM(MAC)
    public:
        DocumentLoader(NSURLRequest *);
        virtual ~DocumentLoader();

        void setFrame(Frame*);
        virtual void attachToFrame();
        virtual void detachFromFrame();

        FrameLoader* frameLoader() const;
        NSData *mainResourceData() const;
        NSURLRequest *originalRequest() const;
        NSURLRequest *originalRequestCopy() const;
        NSMutableURLRequest *request();
        void setRequest(NSURLRequest *);
        NSMutableURLRequest *actualRequest();
        NSURLRequest *initialRequest() const;
        NSURL *URL() const;
        NSURL *unreachableURL() const;
        void replaceRequestURLForAnchorScroll(NSURL *);
        bool isStopping() const;
        void stopLoading();
        void setCommitted(bool);
        bool isCommitted() const;
        bool isLoading() const;
        void setLoading(bool);
        void updateLoading();
        void receivedData(NSData *);
        void setupForReplaceByMIMEType(const String& newMIMEType);
        void finishedLoading();
        NSURLResponse *response() const;
        NSError *mainDocumentError() const;
        void mainReceivedError(NSError *, bool isComplete);
        void setResponse(NSURLResponse *);
        void prepareForLoadStart();
        bool isClientRedirect() const;
        void setIsClientRedirect(bool);
        bool isLoadingInAPISense() const;
        void setPrimaryLoadComplete(bool);
        void setTitle(const String&);
        String overrideEncoding() const;
        void addResponse(NSURLResponse *);
        const ResponseVector& responses() const;
        NSDictionary *triggeringAction() const;
        void setTriggeringAction(NSDictionary *);
        void setOverrideEncoding(const String&);
        void setLastCheckedRequest(NSURLRequest *request);
        NSURLRequest *lastCheckedRequest() const;
        void stopRecordingResponses();
        String title() const;
        NSURL *URLForHistory() const;

    private:
        void setMainResourceData(NSData *);
        void setupForReplace();
        void commitIfReady();
        void clearErrors();
        double loadingStartedTime() const;
        void setMainDocumentError(NSError *);
        void commitLoad(NSData *);
        bool doesProgressiveLoad(const String& MIMEType) const;

        Frame* m_frame;

        RetainPtr<NSData> m_mainResourceData;

        // A reference to actual request used to create the data source.
        // This should only be used by the resourceLoadDelegate's
        // identifierForInitialRequest:fromDatasource: method. It is
        // not guaranteed to remain unchanged, as requests are mutable.
        RetainPtr<NSURLRequest> m_originalRequest;    

        // A copy of the original request used to create the data source.
        // We have to copy the request because requests are mutable.
        RetainPtr<NSURLRequest> m_originalRequestCopy;
        
        // The 'working' request. It may be mutated
        // several times from the original request to include additional
        // headers, cookie information, canonicalization and redirects.
        RetainPtr<NSMutableURLRequest> m_request;

        RetainPtr<NSURLResponse> m_response;
    
        RetainPtr<NSError> m_mainDocumentError;    
    
        // The time when the data source was told to start loading.
        double m_loadingStartedTime;

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
        RetainPtr<NSDictionary> m_triggeringAction;

        // The last request that we checked click policy for - kept around
        // so we can avoid asking again needlessly.
        RetainPtr<NSURLRequest> m_lastCheckedRequest;

        // We retain all the received responses so we can play back the
        // WebResourceLoadDelegate messages if the item is loaded from the
        // page cache.
        ResponseVector m_responses;
        bool m_stopRecordingResponses;
#endif
    };

}
