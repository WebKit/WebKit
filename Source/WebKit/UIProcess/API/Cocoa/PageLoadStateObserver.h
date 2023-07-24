/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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

#import "PageLoadState.h"
#import <wtf/WeakObjCPtr.h>

namespace WebKit {

class PageLoadStateObserver : public PageLoadState::Observer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PageLoadStateObserver(id object, NSString *activeURLKey = @"activeURL")
        : m_object(object)
        , m_activeURLKey(adoptNS([activeURLKey copy]))
    {
    }

    void clearObject()
    {
        m_object = nil;
    }

private:
    void willChangeIsLoading() override
    {
        [m_object.get() willChangeValueForKey:@"loading"];
    }

    void didChangeIsLoading() override
    {
        [m_object.get() didChangeValueForKey:@"loading"];
    }

    void willChangeTitle() override
    {
        [m_object.get() willChangeValueForKey:@"title"];
    }

    void didChangeTitle() override
    {
        [m_object.get() didChangeValueForKey:@"title"];
    }

    void willChangeActiveURL() override
    {
        [m_object.get() willChangeValueForKey:m_activeURLKey.get()];
    }

    void didChangeActiveURL() override
    {
        [m_object.get() didChangeValueForKey:m_activeURLKey.get()];
    }

    void willChangeHasOnlySecureContent() override
    {
        [m_object.get() willChangeValueForKey:@"hasOnlySecureContent"];
    }

    void didChangeHasOnlySecureContent() override
    {
        [m_object.get() didChangeValueForKey:@"hasOnlySecureContent"];
    }

    void willChangeEstimatedProgress() override
    {
        [m_object.get() willChangeValueForKey:@"estimatedProgress"];
    }

    void didChangeEstimatedProgress() override
    {
        [m_object.get() didChangeValueForKey:@"estimatedProgress"];
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
        [m_object.get() willChangeValueForKey:@"_webProcessIsResponsive"];
    }

    void didChangeWebProcessIsResponsive() override
    {
        [m_object.get() didChangeValueForKey:@"_webProcessIsResponsive"];
    }

    WeakObjCPtr<id> m_object;
    RetainPtr<NSString> m_activeURLKey;
};

}
