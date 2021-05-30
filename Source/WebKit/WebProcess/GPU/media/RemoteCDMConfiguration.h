/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(ENCRYPTED_MEDIA)

#include <wtf/text/WTFString.h>

namespace WebKit {

struct RemoteCDMConfiguration {
    Vector<AtomString> supportedInitDataTypes;
    Vector<AtomString> supportedRobustnesses;
    bool supportsServerCertificates;
    bool supportsSessions;

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << supportedInitDataTypes;
        encoder << supportedRobustnesses;
        encoder << supportsServerCertificates;
        encoder << supportsSessions;
    }

    template <class Decoder>
    static std::optional<RemoteCDMConfiguration> decode(Decoder& decoder)
    {
        std::optional<Vector<AtomString>> supportedInitDataTypes;
        decoder >> supportedInitDataTypes;
        if (!supportedInitDataTypes)
            return std::nullopt;

        std::optional<Vector<AtomString>> supportedRobustnesses;
        decoder >> supportedRobustnesses;
        if (!supportedRobustnesses)
            return std::nullopt;

        std::optional<bool> supportsServerCertificates;
        decoder >> supportsServerCertificates;
        if (!supportsServerCertificates)
            return std::nullopt;

        std::optional<bool> supportsSessions;
        decoder >> supportsSessions;
        if (!supportsSessions)
            return std::nullopt;

        return {{
            WTFMove(*supportedInitDataTypes),
            WTFMove(*supportedRobustnesses),
            *supportsServerCertificates,
            *supportsSessions,
        }};
    }
};

} // namespace WebKit

#endif
