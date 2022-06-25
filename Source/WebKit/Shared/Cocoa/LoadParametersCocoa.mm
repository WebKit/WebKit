/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "config.h"
#import "LoadParameters.h"

#if PLATFORM(COCOA)

#import "ArgumentCodersCocoa.h"
#import "WebCoreArgumentCoders.h"

namespace WebKit {

void LoadParameters::platformEncode(IPC::Encoder& encoder) const
{
    IPC::encode(encoder, dataDetectionContext.get());

#if !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    encoder << networkExtensionSandboxExtensionHandles;
#if PLATFORM(IOS)
    encoder << contentFilterExtensionHandle;
    encoder << frontboardServiceExtensionHandle;
#endif
#endif
}

bool LoadParameters::platformDecode(IPC::Decoder& decoder, LoadParameters& parameters)
{
    if (!IPC::decode(decoder, parameters.dataDetectionContext))
        return false;

#if !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    std::optional<Vector<SandboxExtension::Handle>> networkExtensionSandboxExtensionHandles;
    decoder >> networkExtensionSandboxExtensionHandles;
    if (!networkExtensionSandboxExtensionHandles)
        return false;
    parameters.networkExtensionSandboxExtensionHandles = WTFMove(*networkExtensionSandboxExtensionHandles);
#if PLATFORM(IOS)
    std::optional<std::optional<SandboxExtension::Handle>> contentFilterExtensionHandle;
    decoder >> contentFilterExtensionHandle;
    if (!contentFilterExtensionHandle)
        return false;
    parameters.contentFilterExtensionHandle = WTFMove(*contentFilterExtensionHandle);

    std::optional<std::optional<SandboxExtension::Handle>> frontboardServiceExtensionHandle;
    decoder >> frontboardServiceExtensionHandle;
    if (!frontboardServiceExtensionHandle)
        return false;
    parameters.frontboardServiceExtensionHandle = WTFMove(*frontboardServiceExtensionHandle);
#endif // PLATFORM(IOS)
#endif // !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)

    return true;
}

} // namespace WebKit

#endif
