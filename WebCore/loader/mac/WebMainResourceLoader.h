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

#import "FrameLoaderTypes.h"
#import "WebLoader.h"
#import <wtf/Forward.h>

@class WebCoreMainResourceLoaderAsPolicyDelegate;

namespace WebCore {

    class MainResourceLoader : public WebResourceLoader {
    public:
        static PassRefPtr<MainResourceLoader> create(Frame*);

        virtual ~MainResourceLoader();

        virtual bool load(NSURLRequest *);

        virtual void setDefersCallbacks(bool);

        virtual void addData(NSData *, bool allAtOnce);

        virtual NSURLRequest *willSendRequest(NSURLRequest *, NSURLResponse *redirectResponse);
        virtual void didReceiveResponse(NSURLResponse *);
        virtual void didReceiveData(NSData *, long long lengthReceived, bool allAtOnce);
        virtual void didFinishLoading();
        virtual void didFail(NSError *);

        void continueAfterNavigationPolicy(NSURLRequest *);
        void continueAfterContentPolicy(PolicyAction);

    private:
        virtual void didCancel(NSError *);

        MainResourceLoader(Frame*);

        virtual void releaseDelegate();

        WebCoreMainResourceLoaderAsPolicyDelegate *policyDelegate();
        void releasePolicyDelegate();

        NSURLRequest *loadNow(NSURLRequest *);

        void receivedError(NSError *);
        NSError *interruptionForPolicyChangeError() const;
        void stopLoadingForPolicyChange();
        bool isPostOrRedirectAfterPost(NSURLRequest *newRequest, NSURLResponse *redirectResponse);

        void continueAfterContentPolicy(PolicyAction, NSURLResponse *);

        int m_contentLength; // for logging only
        int m_bytesReceived; // for logging only
        RetainPtr<NSURLResponse> m_response;
        RetainPtr<id> m_proxy;
        RetainPtr<NSURLRequest> m_initialRequest;
        RetainPtr<WebCoreMainResourceLoaderAsPolicyDelegate> m_policyDelegate;
        bool m_loadingMultipartContent;
    };

}
