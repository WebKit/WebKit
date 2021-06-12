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

#if ENABLE(GPU_PROCESS)

#include "SandboxExtension.h"
#include <wtf/text/WTFString.h>

namespace WebKit {

struct GPUProcessSessionParameters {
    String mediaCacheDirectory;
    SandboxExtension::Handle mediaCacheDirectorySandboxExtensionHandle;

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    String mediaKeysStorageDirectory;
    SandboxExtension::Handle mediaKeysStorageDirectorySandboxExtensionHandle;
#endif

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << mediaCacheDirectory << mediaCacheDirectorySandboxExtensionHandle;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
        encoder << mediaKeysStorageDirectory << mediaKeysStorageDirectorySandboxExtensionHandle;
#endif
    }

    template <class Decoder>
    static std::optional<GPUProcessSessionParameters> decode(Decoder& decoder)
    {
        std::optional<String> mediaCacheDirectory;
        decoder >> mediaCacheDirectory;
        if (!mediaCacheDirectory)
            return std::nullopt;

        std::optional<SandboxExtension::Handle> mediaCacheDirectorySandboxExtensionHandle;
        decoder >> mediaCacheDirectorySandboxExtensionHandle;
        if (!mediaCacheDirectorySandboxExtensionHandle)
            return std::nullopt;

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
        std::optional<String> mediaKeysStorageDirectory;
        decoder >> mediaKeysStorageDirectory;
        if (!mediaKeysStorageDirectory)
            return std::nullopt;

        std::optional<SandboxExtension::Handle> mediaKeysStorageDirectorySandboxExtensionHandle;
        decoder >> mediaKeysStorageDirectorySandboxExtensionHandle;
        if (!mediaKeysStorageDirectorySandboxExtensionHandle)
            return std::nullopt;
#endif

        return GPUProcessSessionParameters {
            WTFMove(*mediaCacheDirectory),
            WTFMove(*mediaCacheDirectorySandboxExtensionHandle),
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
            WTFMove(*mediaKeysStorageDirectory),
            WTFMove(*mediaKeysStorageDirectorySandboxExtensionHandle),
#endif
        };
    }
};

} // namespace WebKit

#endif
