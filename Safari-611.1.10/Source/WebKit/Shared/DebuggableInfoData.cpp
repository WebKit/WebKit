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

#include "config.h"
#include "DebuggableInfoData.h"

#include "WebCoreArgumentCoders.h"
#include <WebCore/InspectorDebuggableType.h>

namespace WebKit {

DebuggableInfoData DebuggableInfoData::empty()
{
    return {
        Inspector::DebuggableType::Page,
        "Unknown"_s,
        "Unknown"_s,
        "Unknown"_s,
        false
    };
}

void DebuggableInfoData::encode(IPC::Encoder& encoder) const
{
    encoder << debuggableType;
    encoder << targetPlatformName;
    encoder << targetBuildVersion;
    encoder << targetProductVersion;
    encoder << targetIsSimulator;
}

Optional<DebuggableInfoData> DebuggableInfoData::decode(IPC::Decoder& decoder)
{
    Optional<Inspector::DebuggableType> debuggableType;
    decoder >> debuggableType;
    if (!debuggableType)
        return WTF::nullopt;

    Optional<String> targetPlatformName;
    decoder >> targetPlatformName;
    if (!targetPlatformName)
        return WTF::nullopt;

    Optional<String> targetBuildVersion;
    decoder >> targetBuildVersion;
    if (!targetBuildVersion)
        return WTF::nullopt;

    Optional<String> targetProductVersion;
    decoder >> targetProductVersion;
    if (!targetProductVersion)
        return WTF::nullopt;

    Optional<bool> targetIsSimulator;
    decoder >> targetIsSimulator;
    if (!targetIsSimulator)
        return WTF::nullopt;

    return {{
        *debuggableType,
        *targetPlatformName,
        *targetBuildVersion,
        *targetProductVersion,
        *targetIsSimulator
    }};
}

} // namespace WebKit
