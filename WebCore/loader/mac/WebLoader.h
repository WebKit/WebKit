/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "RetainPtr.h"
#include "Shared.h"

@class NSError;
@class NSURLAuthenticationChallenge;
@class NSURLConnection;
@class NSURLRequest;
@class NSURLResponse;
@class WebFrameLoader;
@class WebCoreResourceLoaderDelegate;

namespace WebCore {

    // FIXME: Rename to ResourceLoader after resolving conflict with existing class of that name.
    class WebResourceLoader : public Shared<WebResourceLoader> {
    public:
        virtual ~WebResourceLoader();

        virtual bool load(NSURLRequest *);

        virtual void signalFinish();

        void setFrameLoader(WebFrameLoader *);
        WebFrameLoader *frameLoader() const;

        virtual void cancel();
        virtual void cancel(NSError *);
        NSError *cancelledError();

        virtual void setDefersCallbacks(bool);
        bool defersCallbacks() const;

        void setIdentifier(id);

        virtual void releaseResources();
        NSURLResponse *response() const;

        virtual void addData(NSData *, bool allAtOnce);
        NSData *resourceData();
        void clearResourceData();

        virtual NSURLRequest *willSendRequest(NSURLRequest *, NSURLResponse *redirectResponse);
        void didReceiveAuthenticationChallenge(NSURLAuthenticationChallenge *);
        void didCancelAuthenticationChallenge(NSURLAuthenticationChallenge *);
        virtual void didReceiveResponse(NSURLResponse *);
        virtual void didReceiveData(NSData *, long long lengthReceived, bool allAtOnce);
        void willStopBufferingData(NSData *data);
        virtual void didFinishLoading();
        virtual void didFail(NSError *);
        NSCachedURLResponse *willCacheResponse(NSCachedURLResponse *);

        void receivedCredential(NSURLAuthenticationChallenge *, NSURLCredential *);
        void receivedRequestToContinueWithoutCredential(NSURLAuthenticationChallenge *);
        void receivedCancellation(NSURLAuthenticationChallenge *);

        // Used to work around the fact that you don't get any more NSURLConnection callbacks until you return from the first one.
        static bool inConnectionCallback();

    protected:
        WebResourceLoader(WebFrameLoader *);
        WebCoreResourceLoaderDelegate *delegate();
        virtual void releaseDelegate();

        NSURLConnection *connection() const { return m_connection.get(); }
        NSURLRequest *request() const { return m_request.get(); }
        bool reachedTerminalState() const { return m_reachedTerminalState; }
        bool signalledFinish() const { return m_signalledFinish; }
        bool cancelled() const { return m_cancelled; }
        id identifier() const { return m_identifier.get(); }

        RetainPtr<NSURLConnection> m_connection;

    private:
        RetainPtr<NSURLRequest> m_request;
        bool m_reachedTerminalState;
        bool m_signalledFinish;
        bool m_cancelled;

        RetainPtr<WebFrameLoader> m_frameLoader;
        RetainPtr<id> m_identifier;
        RetainPtr<NSURLResponse> m_response;
        NSURLAuthenticationChallenge *m_currentConnectionChallenge;
        RetainPtr<NSURLAuthenticationChallenge> m_currentWebChallenge;
        bool m_defersCallbacks;
        RetainPtr<NSURL> m_originalURL;
        RetainPtr<NSMutableData> m_resourceData;
        RetainPtr<WebCoreResourceLoaderDelegate> m_delegate;
    };

}
