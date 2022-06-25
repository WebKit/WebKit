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

#include <wtf/Forward.h>

namespace WebCore {

enum class PlatformVideoTransferCharacteristics : uint8_t {
    Bt709,
    Smpte170m,
    Iec6196621,
    Gamma22curve,
    Gamma28curve,
    Smpte240m,
    Linear,
    Log,
    LogSqrt,
    Iec6196624,
    Bt1361ExtendedColourGamut,
    Bt2020_10bit,
    Bt2020_12bit,
    SmpteSt2084,
    SmpteSt4281,
    AribStdB67Hlg,
    Unspecified
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::PlatformVideoTransferCharacteristics> {
    using values = EnumValues<
        WebCore::PlatformVideoTransferCharacteristics,
        WebCore::PlatformVideoTransferCharacteristics::Bt709,
        WebCore::PlatformVideoTransferCharacteristics::Smpte170m,
        WebCore::PlatformVideoTransferCharacteristics::Iec6196621,
        WebCore::PlatformVideoTransferCharacteristics::Gamma22curve,
        WebCore::PlatformVideoTransferCharacteristics::Gamma28curve,
        WebCore::PlatformVideoTransferCharacteristics::Smpte240m,
        WebCore::PlatformVideoTransferCharacteristics::Linear,
        WebCore::PlatformVideoTransferCharacteristics::Log,
        WebCore::PlatformVideoTransferCharacteristics::LogSqrt,
        WebCore::PlatformVideoTransferCharacteristics::Iec6196624,
        WebCore::PlatformVideoTransferCharacteristics::Bt1361ExtendedColourGamut,
        WebCore::PlatformVideoTransferCharacteristics::Bt2020_10bit,
        WebCore::PlatformVideoTransferCharacteristics::Bt2020_12bit,
        WebCore::PlatformVideoTransferCharacteristics::SmpteSt2084,
        WebCore::PlatformVideoTransferCharacteristics::SmpteSt4281,
        WebCore::PlatformVideoTransferCharacteristics::AribStdB67Hlg,
        WebCore::PlatformVideoTransferCharacteristics::Unspecified
    >;
};

}
