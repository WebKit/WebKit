/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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

#include "CertificateInfo.h"
#include "ContentSecurityPolicyResponseHeaders.h"
#include "CrossOriginEmbedderPolicy.h"
#include "ResourceError.h"
#include "ScriptBuffer.h"

namespace WebCore {

struct WorkerFetchResult {
    ScriptBuffer script;
    URL responseURL;
    CertificateInfo certificateInfo;
    ContentSecurityPolicyResponseHeaders contentSecurityPolicy;
    CrossOriginEmbedderPolicy crossOriginEmbedderPolicy;
    String referrerPolicy;
    ResourceError error;

    WorkerFetchResult isolatedCopy() const { return { script.isolatedCopy(), responseURL.isolatedCopy(), certificateInfo.isolatedCopy(), contentSecurityPolicy.isolatedCopy(), crossOriginEmbedderPolicy.isolatedCopy(), referrerPolicy.isolatedCopy(), error.isolatedCopy() }; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static WARN_UNUSED_RETURN bool decode(Decoder&, WorkerFetchResult&);
};

inline WorkerFetchResult workerFetchError(const ResourceError& error)
{
    return { { }, { }, { }, { }, { }, { }, error };
}

template<class Encoder>
void WorkerFetchResult::encode(Encoder& encoder) const
{
    encoder << script << responseURL << contentSecurityPolicy << crossOriginEmbedderPolicy << referrerPolicy << error << certificateInfo;
}

template<class Decoder>
bool WorkerFetchResult::decode(Decoder& decoder, WorkerFetchResult& result)
{
    if (!decoder.decode(result.script))
        return false;
    if (!decoder.decode(result.responseURL))
        return false;
    if (!decoder.decode(result.contentSecurityPolicy))
        return false;
    if (!decoder.decode(result.crossOriginEmbedderPolicy))
        return false;
    if (!decoder.decode(result.referrerPolicy))
        return false;
    if (!decoder.decode(result.error))
        return false;

    std::optional<CertificateInfo> certificateInfo;
    decoder >> certificateInfo;
    if (!certificateInfo)
        return false;
    result.certificateInfo = WTFMove(*certificateInfo);

    return true;
}

} // namespace WebCore
