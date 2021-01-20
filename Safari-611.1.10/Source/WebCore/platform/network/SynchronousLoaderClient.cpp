/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SynchronousLoaderClient.h"

#include "AuthenticationChallenge.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

SynchronousLoaderClient::SynchronousLoaderClient()
    : m_messageQueue(SynchronousLoaderMessageQueue::create()) { }

SynchronousLoaderClient::~SynchronousLoaderClient() = default;

void SynchronousLoaderClient::willSendRequestAsync(ResourceHandle* handle, ResourceRequest&& request, ResourceResponse&&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    // FIXME: This needs to be fixed to follow the redirect correctly even for cross-domain requests.
    if (protocolHostAndPortAreEqual(handle->firstRequest().url(), request.url())) {
        completionHandler(WTFMove(request));
        return;
    }

    ASSERT(m_error.isNull());
    m_error = platformBadResponseError();
    completionHandler({ });
}

bool SynchronousLoaderClient::shouldUseCredentialStorage(ResourceHandle*)
{
    // FIXME: We should ask FrameLoaderClient whether using credential storage is globally forbidden.
    return m_allowStoredCredentials;
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void SynchronousLoaderClient::canAuthenticateAgainstProtectionSpaceAsync(ResourceHandle*, const ProtectionSpace&, CompletionHandler<void(bool)>&& completionHandler)
{
    // FIXME: We should ask FrameLoaderClient. <http://webkit.org/b/65196>
    completionHandler(true);
}
#endif

void SynchronousLoaderClient::didReceiveResponseAsync(ResourceHandle*, ResourceResponse&& response, CompletionHandler<void()>&& completionHandler)
{
    m_response = WTFMove(response);
    completionHandler();
}

void SynchronousLoaderClient::didReceiveData(ResourceHandle*, const char* data, unsigned length, int /*encodedDataLength*/)
{
    m_data.append(data, length);
}

void SynchronousLoaderClient::didFinishLoading(ResourceHandle* handle)
{
    m_messageQueue->kill();
#if PLATFORM(COCOA)
    if (handle)
        handle->releaseDelegate();
#else
    UNUSED_PARAM(handle);
#endif
}

void SynchronousLoaderClient::didFail(ResourceHandle* handle, const ResourceError& error)
{
    ASSERT(m_error.isNull());

    m_error = error;
    
    m_messageQueue->kill();
#if PLATFORM(COCOA)
    if (handle)
        handle->releaseDelegate();
#else
    UNUSED_PARAM(handle);
#endif
}

}
