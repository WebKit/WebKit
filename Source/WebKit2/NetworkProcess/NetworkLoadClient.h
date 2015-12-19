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

#ifndef NetworkLoadClient_h
#define NetworkLoadClient_h

#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/Forward.h>

#if PLATFORM(COCOA)
typedef const struct _CFCachedURLResponse* CFCachedURLResponseRef;
#endif

namespace WebCore {
class ProtectionSpace;
}

namespace WebKit {

class NetworkLoadClient {
public:
    virtual ~NetworkLoadClient() { }

    virtual bool isSynchronous() const = 0;

    virtual void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) = 0;
    virtual void canAuthenticateAgainstProtectionSpaceAsync(const WebCore::ProtectionSpace&) = 0;
    virtual void willSendRedirectedRequest(const WebCore::ResourceRequest&, const WebCore::ResourceRequest& redirectRequest, const WebCore::ResourceResponse& redirectResponse) = 0;
    enum class ShouldContinueDidReceiveResponse { No, Yes };
    virtual ShouldContinueDidReceiveResponse didReceiveResponse(const WebCore::ResourceResponse&) = 0;
    virtual void didReceiveBuffer(RefPtr<WebCore::SharedBuffer>&&, int reportedEncodedDataLength) = 0;
    virtual void didFinishLoading(double finishTime) = 0;
    virtual void didFailLoading(const WebCore::ResourceError&) = 0;
    virtual void didConvertToDownload() = 0;

#if PLATFORM(COCOA)
    virtual void willCacheResponseAsync(CFCachedURLResponseRef) = 0;
#endif
};

} // namespace WebKit

#endif // NetworkLoadClient_h

