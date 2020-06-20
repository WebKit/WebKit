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

    encoder << neHelperExtensionHandle;
    encoder << neSessionManagerExtensionHandle;
#if PLATFORM(IOS)
    encoder << contentFilterExtensionHandle;
    encoder << frontboardServiceExtensionHandle;
#endif
}

bool LoadParameters::platformDecode(IPC::Decoder& decoder, LoadParameters& parameters)
{
    if (!IPC::decode(decoder, parameters.dataDetectionContext))
        return false;

    Optional<Optional<SandboxExtension::Handle>> neHelperExtensionHandle;
    decoder >> neHelperExtensionHandle;
    if (!neHelperExtensionHandle)
        return false;
    parameters.neHelperExtensionHandle = WTFMove(*neHelperExtensionHandle);

    Optional<Optional<SandboxExtension::Handle>> neSessionManagerExtensionHandle;
    decoder >> neSessionManagerExtensionHandle;
    if (!neSessionManagerExtensionHandle)
        return false;
    parameters.neSessionManagerExtensionHandle = WTFMove(*neSessionManagerExtensionHandle);

#if PLATFORM(IOS)
    Optional<Optional<SandboxExtension::Handle>> contentFilterExtensionHandle;
    decoder >> contentFilterExtensionHandle;
    if (!contentFilterExtensionHandle)
        return false;
    parameters.contentFilterExtensionHandle = WTFMove(*contentFilterExtensionHandle);

    Optional<Optional<SandboxExtension::Handle>> frontboardServiceExtensionHandle;
    decoder >> frontboardServiceExtensionHandle;
    if (!frontboardServiceExtensionHandle)
        return false;
    parameters.frontboardServiceExtensionHandle = WTFMove(*frontboardServiceExtensionHandle);
#endif

    return true;
}

} // namespace WebKit

#endif
