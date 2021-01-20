/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#pragma once

#include <wtf/Ref.h>

namespace WebCore {

class CertificateInfo;
class CurlRequest;
class CurlResponse;
class NetworkLoadMetrics;
class ResourceError;
class SharedBuffer;

class CurlRequestClient {
public:
    virtual void ref() = 0;
    virtual void deref() = 0;

    virtual void curlDidSendData(CurlRequest&, unsigned long long bytesSent, unsigned long long totalBytesToBeSent) = 0;
    virtual void curlDidReceiveResponse(CurlRequest&, CurlResponse&&) = 0;
    virtual void curlDidReceiveBuffer(CurlRequest&, Ref<SharedBuffer>&&) = 0;
    virtual void curlDidComplete(CurlRequest&, NetworkLoadMetrics&&) = 0;
    virtual void curlDidFailWithError(CurlRequest&, ResourceError&&, CertificateInfo&&) = 0;
};

} // namespace WebCore
