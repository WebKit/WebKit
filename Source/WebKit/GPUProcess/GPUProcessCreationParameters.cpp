/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
    encoder << auxiliaryProcessParameters;
#if ENABLE(MEDIA_STREAM)
    encoder << useMockCaptureDevices;
#if PLATFORM(MAC)
    encoder << microphoneSandboxExtensionHandle;
    encoder << launchServicesExtensionHandle;
#endif
#endif
#if HAVE(AVCONTENTKEYSPECIFIER)
    encoder << sampleBufferContentKeySessionSupportEnabled;
#endif
    encoder << parentPID;

#if USE(SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS)
    encoder << containerCachesDirectoryExtensionHandle;
    encoder << containerTemporaryDirectoryExtensionHandle;
#endif
#if PLATFORM(IOS_FAMILY)
    encoder << compilerServiceExtensionHandles;
    encoder << dynamicIOKitExtensionHandles;
#endif
    encoder << mobileGestaltExtensionHandle;
#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    encoder << gpuToolsExtensionHandles;
#endif

    encoder << applicationVisibleName;
#if PLATFORM(COCOA)
    encoder << strictSecureDecodingForAllObjCEnabled;
#endif

#if USE(GBM)
    encoder << renderDeviceFile;
#endif

    encoder << overrideLanguages;
}

bool GPUProcessCreationParameters::decode(IPC::Decoder& decoder, GPUProcessCreationParameters& result)
{
    if (!decoder.decode(result.auxiliaryProcessParameters))
        return false;
#if ENABLE(MEDIA_STREAM)
    if (!decoder.decode(result.useMockCaptureDevices))
        return false;
#if PLATFORM(MAC)
    if (!decoder.decode(result.microphoneSandboxExtensionHandle))
        return false;
    if (!decoder.decode(result.launchServicesExtensionHandle))
        return false;
#endif
#endif
#if HAVE(AVCONTENTKEYSPECIFIER)
    if (!decoder.decode(result.sampleBufferContentKeySessionSupportEnabled))
        return false;
#endif

    if (!decoder.decode(result.parentPID))
        return false;

#if USE(SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS)
    std::optional<SandboxExtension::Handle> containerCachesDirectoryExtensionHandle;
    decoder >> containerCachesDirectoryExtensionHandle;
    if (!containerCachesDirectoryExtensionHandle)
        return false;
    result.containerCachesDirectoryExtensionHandle = WTFMove(*containerCachesDirectoryExtensionHandle);

    std::optional<SandboxExtension::Handle> containerTemporaryDirectoryExtensionHandle;
    decoder >> containerTemporaryDirectoryExtensionHandle;
    if (!containerTemporaryDirectoryExtensionHandle)
        return false;
    result.containerTemporaryDirectoryExtensionHandle = WTFMove(*containerTemporaryDirectoryExtensionHandle);
#endif
#if PLATFORM(IOS_FAMILY)
    std::optional<Vector<SandboxExtension::Handle>> compilerServiceExtensionHandles;
    decoder >> compilerServiceExtensionHandles;
    if (!compilerServiceExtensionHandles)
        return false;
    result.compilerServiceExtensionHandles = WTFMove(*compilerServiceExtensionHandles);

    std::optional<Vector<SandboxExtension::Handle>> dynamicIOKitExtensionHandles;
    decoder >> dynamicIOKitExtensionHandles;
    if (!dynamicIOKitExtensionHandles)
        return false;
    result.dynamicIOKitExtensionHandles = WTFMove(*dynamicIOKitExtensionHandles);
#endif

    std::optional<std::optional<SandboxExtension::Handle>> mobileGestaltExtensionHandle;
    decoder >> mobileGestaltExtensionHandle;
    if (!mobileGestaltExtensionHandle)
        return false;
    result.mobileGestaltExtensionHandle = WTFMove(*mobileGestaltExtensionHandle);

#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    std::optional<Vector<SandboxExtension::Handle>> gpuToolsExtensionHandles;
    decoder >> gpuToolsExtensionHandles;
    if (!gpuToolsExtensionHandles)
        return false;
    result.gpuToolsExtensionHandles = WTFMove(*gpuToolsExtensionHandles);
#endif

    if (!decoder.decode(result.applicationVisibleName))
        return false;

#if PLATFORM(COCOA)
    if (!decoder.decode(result.strictSecureDecodingForAllObjCEnabled))
        return false;
#endif

#if USE(GBM)
    std::optional<String> renderDeviceFile;
    decoder >> renderDeviceFile;
    if (!renderDeviceFile)
        return false;
    result.renderDeviceFile = WTFMove(*renderDeviceFile);
#endif

    std::optional<Vector<String>> overrideLanguages;
    decoder >> overrideLanguages;
    if (!overrideLanguages)
        return false;
    result.overrideLanguages = WTFMove(*overrideLanguages);

    return true;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
