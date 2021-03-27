/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "GPUProcessCreationParameters.h"

#if ENABLE(GPU_PROCESS)

#include "ArgumentCoders.h"
#include "WebCoreArgumentCoders.h"

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#endif

namespace WebKit {

GPUProcessCreationParameters::GPUProcessCreationParameters() = default;

void GPUProcessCreationParameters::encode(IPC::Encoder& encoder) const
{
#if ENABLE(MEDIA_STREAM)
    encoder << useMockCaptureDevices;
    encoder << cameraSandboxExtensionHandle;
#if HAVE(AUDIT_TOKEN)
    encoder << appleCameraServicePathSandboxExtensionHandle;
#if HAVE(ADDITIONAL_APPLE_CAMERA_SERVICE)
    encoder << additionalAppleCameraServicePathSandboxExtensionHandle;
#endif
#endif // HAVE(AUDIT_TOKEN)
    encoder << microphoneSandboxExtensionHandle;
#if PLATFORM(IOS)
    encoder << tccSandboxExtensionHandle;
#endif
#endif
    encoder << parentPID;

#if USE(SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS)
    encoder << containerCachesDirectoryExtensionHandle;
    encoder << containerTemporaryDirectoryExtensionHandle;
#endif
}

bool GPUProcessCreationParameters::decode(IPC::Decoder& decoder, GPUProcessCreationParameters& result)
{
#if ENABLE(MEDIA_STREAM)
    if (!decoder.decode(result.useMockCaptureDevices))
        return false;
    if (!decoder.decode(result.cameraSandboxExtensionHandle))
        return false;
#if HAVE(AUDIT_TOKEN)
    if (!decoder.decode(result.appleCameraServicePathSandboxExtensionHandle))
        return false;
#if HAVE(ADDITIONAL_APPLE_CAMERA_SERVICE)
    if (!decoder.decode(result.additionalAppleCameraServicePathSandboxExtensionHandle))
        return false;
#endif
#endif // HAVE(AUDIT_TOKEN)
    if (!decoder.decode(result.microphoneSandboxExtensionHandle))
        return false;
#if PLATFORM(IOS)
    if (!decoder.decode(result.tccSandboxExtensionHandle))
        return false;
#endif
#endif
    if (!decoder.decode(result.parentPID))
        return false;

#if USE(SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS)
    Optional<SandboxExtension::Handle> containerCachesDirectoryExtensionHandle;
    decoder >> containerCachesDirectoryExtensionHandle;
    if (!containerCachesDirectoryExtensionHandle)
        return false;
    result.containerCachesDirectoryExtensionHandle = WTFMove(*containerCachesDirectoryExtensionHandle);

    Optional<SandboxExtension::Handle> containerTemporaryDirectoryExtensionHandle;
    decoder >> containerTemporaryDirectoryExtensionHandle;
    if (!containerTemporaryDirectoryExtensionHandle)
        return false;
    result.containerTemporaryDirectoryExtensionHandle = WTFMove(*containerTemporaryDirectoryExtensionHandle);
#endif

    return true;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
