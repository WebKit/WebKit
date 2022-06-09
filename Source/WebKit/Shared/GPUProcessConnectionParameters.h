/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "WebCoreArgumentCoders.h"
#include <WebCore/ProcessIdentity.h>
#include <wtf/MachSendRight.h>

namespace WebKit {

struct GPUProcessConnectionParameters {
    WebCore::ProcessIdentity webProcessIdentity;
    Vector<String> overrideLanguages;
#if ENABLE(IPC_TESTING_API)
    bool ignoreInvalidMessageForTesting { false };
#endif
#if HAVE(AUDIT_TOKEN)
    std::optional<audit_token_t> presentingApplicationAuditToken;
#endif

    void encode(IPC::Encoder& encoder) const
    {
        encoder << webProcessIdentity;
        encoder << overrideLanguages;
#if ENABLE(IPC_TESTING_API)
        encoder << ignoreInvalidMessageForTesting;
#endif
#if HAVE(AUDIT_TOKEN)
        encoder << presentingApplicationAuditToken;
#endif
    }

    static std::optional<GPUProcessConnectionParameters> decode(IPC::Decoder& decoder)
    {
        std::optional<WebCore::ProcessIdentity> webProcessIdentity;
        decoder >> webProcessIdentity;
        if (!webProcessIdentity)
            return std::nullopt;

        std::optional<Vector<String>> overrideLanguages;
        decoder >> overrideLanguages;
        if (!overrideLanguages)
            return std::nullopt;

#if ENABLE(IPC_TESTING_API)
        std::optional<bool> ignoreInvalidMessageForTesting;
        decoder >> ignoreInvalidMessageForTesting;
        if (!ignoreInvalidMessageForTesting)
            return std::nullopt;
#endif

#if HAVE(AUDIT_TOKEN)
        std::optional<audit_token_t> presentingApplicationAuditToken;
        if (!decoder.decode(presentingApplicationAuditToken))
            return std::nullopt;
#endif

        return GPUProcessConnectionParameters {
            WTFMove(*webProcessIdentity),
            WTFMove(*overrideLanguages),
#if ENABLE(IPC_TESTING_API)
            *ignoreInvalidMessageForTesting,
#endif
#if HAVE(AUDIT_TOKEN)
            WTFMove(presentingApplicationAuditToken),
#endif
        };
    }
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
