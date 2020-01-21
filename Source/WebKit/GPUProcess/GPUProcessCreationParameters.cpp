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
    encoder << microphoneSandboxExtensionHandle;
#if PLATFORM(IOS)
    encoder << tccSandboxExtensionHandle;
#endif
#endif
}

bool GPUProcessCreationParameters::decode(IPC::Decoder& decoder, GPUProcessCreationParameters& result)
{
#if ENABLE(MEDIA_STREAM)
    if (!decoder.decode(result.useMockCaptureDevices))
        return false;
    if (!decoder.decode(result.cameraSandboxExtensionHandle))
        return false;
    if (!decoder.decode(result.microphoneSandboxExtensionHandle))
        return false;
#if PLATFORM(IOS)
    if (!decoder.decode(result.tccSandboxExtensionHandle))
        return false;
#endif
#endif
    return true;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
