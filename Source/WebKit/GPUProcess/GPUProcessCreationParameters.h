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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "AuxiliaryProcessCreationParameters.h"
#include "SandboxExtension.h"
#include <wtf/ProcessID.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

struct GPUProcessCreationParameters {
    AuxiliaryProcessCreationParameters auxiliaryProcessParameters;
#if ENABLE(MEDIA_STREAM)
    bool useMockCaptureDevices { false };
#if PLATFORM(MAC)
    SandboxExtension::Handle microphoneSandboxExtensionHandle;
    SandboxExtension::Handle launchServicesExtensionHandle;
#endif
#endif
#if USE(MODERN_AVCONTENTKEYSESSION)
    bool shouldUseModernAVContentKeySession { false };
#endif

#if USE(SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS)
    SandboxExtension::Handle containerCachesDirectoryExtensionHandle;
    SandboxExtension::Handle containerTemporaryDirectoryExtensionHandle;
    String containerCachesDirectory;
#endif
#if PLATFORM(IOS_FAMILY)
    Vector<SandboxExtension::Handle> compilerServiceExtensionHandles;
    Vector<SandboxExtension::Handle> dynamicIOKitExtensionHandles;
#endif
    std::optional<SandboxExtension::Handle> mobileGestaltExtensionHandle;
#if PLATFORM(COCOA) && ENABLE(REMOTE_INSPECTOR)
    Vector<SandboxExtension::Handle> gpuToolsExtensionHandles;
#endif

    String applicationVisibleName;

#if USE(GBM)
    String renderDeviceFile;
#endif
    Vector<String> overrideLanguages;
#if PLATFORM(COCOA)
    bool enableMetalDebugDeviceForTesting { false };
    bool enableMetalShaderValidationForTesting { false };
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
