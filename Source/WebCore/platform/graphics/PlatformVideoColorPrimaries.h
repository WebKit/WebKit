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

enum class PlatformVideoColorPrimaries : uint8_t {
    Bt709,
    Bt470bg,
    Smpte170m,
    Bt470m,
    Smpte240m,
    Film,
    Bt2020,
    SmpteSt4281,
    SmpteRp431,
    SmpteEg432,
    JedecP22Phosphors,
    Unspecified,
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::PlatformVideoColorPrimaries> {
    using values = EnumValues<
        WebCore::PlatformVideoColorPrimaries,
        WebCore::PlatformVideoColorPrimaries::Bt709,
        WebCore::PlatformVideoColorPrimaries::Bt470bg,
        WebCore::PlatformVideoColorPrimaries::Smpte170m,
        WebCore::PlatformVideoColorPrimaries::Bt470m,
        WebCore::PlatformVideoColorPrimaries::Smpte240m,
        WebCore::PlatformVideoColorPrimaries::Film,
        WebCore::PlatformVideoColorPrimaries::Bt2020,
        WebCore::PlatformVideoColorPrimaries::SmpteSt4281,
        WebCore::PlatformVideoColorPrimaries::SmpteRp431,
        WebCore::PlatformVideoColorPrimaries::SmpteEg432,
        WebCore::PlatformVideoColorPrimaries::JedecP22Phosphors,
        WebCore::PlatformVideoColorPrimaries::Unspecified
    >;
};

}
