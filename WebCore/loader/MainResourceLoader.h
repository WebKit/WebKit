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

#include "FrameLoaderTypes.h"
#include "ResourceLoader.h"
#include <wtf/Forward.h>

namespace WebCore {

    class FormState;

    class MainResourceLoader : public ResourceLoader {
    public:
        static PassRefPtr<MainResourceLoader> create(Frame*);
        virtual ~MainResourceLoader();

#if PLATFORM(MAC)
        virtual bool load(NSURLRequest *);
        virtual void addData(NSData *, bool allAtOnce);
#endif

        virtual void setDefersLoading(bool);

#if PLATFORM(MAC)
        virtual NSURLRequest *willSendRequest(NSURLRequest *, NSURLResponse *redirectResponse);
        virtual void didReceiveResponse(NSURLResponse *);
        virtual void didReceiveData(NSData *, long long lengthReceived, bool allAtOnce);
        virtual void didFinishLoading();
        virtual void didFail(NSError *);
#endif

    private:
        MainResourceLoader(Frame*);

#if PLATFORM(MAC)
        virtual void didCancel(NSError *);

        NSURLRequest *loadNow(NSURLRequest *);

        void receivedError(NSError *);
        NSError *interruptionForPolicyChangeError() const;
        void stopLoadingForPolicyChange();
        bool isPostOrRedirectAfterPost(NSURLRequest *newRequest, NSURLResponse *redirectResponse);

        static void callContinueAfterNavigationPolicy(void*, NSURLRequest *, PassRefPtr<FormState>);
        void continueAfterNavigationPolicy(NSURLRequest *);

        static void callContinueAfterContentPolicy(void*, PolicyAction);
        void continueAfterContentPolicy(PolicyAction);
        void continueAfterContentPolicy(PolicyAction, NSURLResponse *);

        RetainPtr<NSURLResponse> m_response;
        RetainPtr<NSURLRequest> m_initialRequest;
#endif

        bool m_loadingMultipartContent;
        bool m_waitingForContentPolicy;
    };

}
