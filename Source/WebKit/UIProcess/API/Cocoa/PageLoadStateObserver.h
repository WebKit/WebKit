/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PageLoadStateObserver_h
#define PageLoadStateObserver_h

#import "PageLoadState.h"

namespace WebKit {

class PageLoadStateObserver : public PageLoadState::Observer {
public:
    PageLoadStateObserver(id object, NSString *activeURLKey = @"activeURL")
        : m_object(object)
        , m_activeURLKey(adoptNS([activeURLKey copy]))
    {
    }

private:
    void willChangeIsLoading() override
    {
        [m_object willChangeValueForKey:@"loading"];
    }

    void didChangeIsLoading() override
    {
        [m_object didChangeValueForKey:@"loading"];
    }

    void willChangeTitle() override
    {
        [m_object willChangeValueForKey:@"title"];
    }

    void didChangeTitle() override
    {
        [m_object didChangeValueForKey:@"title"];
    }

    void willChangeActiveURL() override
    {
        [m_object willChangeValueForKey:m_activeURLKey.get()];
    }

    void didChangeActiveURL() override
    {
        [m_object didChangeValueForKey:m_activeURLKey.get()];
    }

    void willChangeHasOnlySecureContent() override
    {
        [m_object willChangeValueForKey:@"hasOnlySecureContent"];
    }

    void didChangeHasOnlySecureContent() override
    {
        [m_object didChangeValueForKey:@"hasOnlySecureContent"];
    }

    void willChangeEstimatedProgress() override
    {
        [m_object willChangeValueForKey:@"estimatedProgress"];
    }

    void didChangeEstimatedProgress() override
    {
        [m_object didChangeValueForKey:@"estimatedProgress"];
    }

    void willChangeCanGoBack() override { }
    void didChangeCanGoBack() override { }
    void willChangeCanGoForward() override { }
    void didChangeCanGoForward() override { }
    void willChangeNetworkRequestsInProgress() override { }
    void didChangeNetworkRequestsInProgress() override { }
    void willChangeCertificateInfo() override { }
    void didChangeCertificateInfo() override { }
    void didSwapWebProcesses() override { }

    void willChangeWebProcessIsResponsive() override
    {
        [m_object willChangeValueForKey:@"_webProcessIsResponsive"];
    }

    void didChangeWebProcessIsResponsive() override
    {
        [m_object didChangeValueForKey:@"_webProcessIsResponsive"];
    }

    id m_object;
    RetainPtr<NSString> m_activeURLKey;
};

}

#endif // PageLoadStateObserver_h
