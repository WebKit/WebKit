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
    bool isLockdownModeEnabled { false };
#if ENABLE(IPC_TESTING_API)
    bool ignoreInvalidMessageForTesting { false };
#endif
#if HAVE(AUDIT_TOKEN)
    std::optional<audit_token_t> presentingApplicationAuditToken;
#endif
#if ENABLE(VP9)
    std::optional<bool> hasVP9HardwareDecoder;
#endif

    void encode(IPC::Encoder& encoder) const
    {
        encoder << webProcessIdentity;
        encoder << overrideLanguages;
        encoder << isLockdownModeEnabled;
#if ENABLE(IPC_TESTING_API)
        encoder << ignoreInvalidMessageForTesting;
#endif
#if HAVE(AUDIT_TOKEN)
        encoder << presentingApplicationAuditToken;
#endif
#if ENABLE(VP9)
        encoder << hasVP9HardwareDecoder;
#endif
    }

    static std::optional<GPUProcessConnectionParameters> decode(IPC::Decoder& decoder)
    {
        auto webProcessIdentity = decoder.decode<WebCore::ProcessIdentity>();
        auto overrideLanguages = decoder.decode<Vector<String>>();
        auto isLockdownModeEnabled = decoder.decode<bool>();
#if ENABLE(IPC_TESTING_API)
        auto ignoreInvalidMessageForTesting = decoder.decode<bool>();
#endif
#if HAVE(AUDIT_TOKEN)
        auto presentingApplicationAuditToken = decoder.decode<std::optional<audit_token_t>>();
#endif
#if ENABLE(VP9)
        auto hasVP9HardwareDecoder = decoder.decode<std::optional<bool>>();
#endif
        if (!decoder.isValid())
            return std::nullopt;

        return GPUProcessConnectionParameters {
            WTFMove(*webProcessIdentity),
            WTFMove(*overrideLanguages),
            *isLockdownModeEnabled,
#if ENABLE(IPC_TESTING_API)
            *ignoreInvalidMessageForTesting,
#endif
#if HAVE(AUDIT_TOKEN)
            WTFMove(*presentingApplicationAuditToken),
#endif
#if ENABLE(VP9)
            WTFMove(*hasVP9HardwareDecoder),
#endif
        };
    }
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
