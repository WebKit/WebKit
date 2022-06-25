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

#include "PlatformVideoColorPrimaries.h"
#include "PlatformVideoMatrixCoefficients.h"
#include "PlatformVideoTransferCharacteristics.h"
#include <optional>
#include <wtf/FastMalloc.h>

namespace WebCore {

struct PlatformVideoColorSpace {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    std::optional<PlatformVideoColorPrimaries> primaries;
    std::optional<PlatformVideoTransferCharacteristics> transfer;
    std::optional<PlatformVideoMatrixCoefficients> matrix;
    std::optional<bool> fullRange;
    template <class Encoder> void encode(Encoder&) const;
    template <class Decoder> static WARN_UNUSED_RETURN bool decode(Decoder&, PlatformVideoColorSpace&);
};

inline bool operator==(const PlatformVideoColorSpace& a, const PlatformVideoColorSpace& b)
{
    return a.primaries == b.primaries
        && a.transfer == b.transfer
        && a.matrix == b.matrix
        && a.fullRange == b.fullRange;
}

inline bool operator!=(const PlatformVideoColorSpace& a, const PlatformVideoColorSpace& b)
{
    return !(a == b);
}

template <class Encoder>
void PlatformVideoColorSpace::encode(Encoder& encoder) const
{
    encoder << primaries;
    encoder << transfer;
    encoder << matrix;
    encoder << fullRange;
}

template <class Decoder>
bool PlatformVideoColorSpace::decode(Decoder& decoder, PlatformVideoColorSpace& colorSpace)
{
    if (!decoder.decode(colorSpace.primaries))
        return false;
    if (!decoder.decode(colorSpace.transfer))
        return false;
    if (!decoder.decode(colorSpace.matrix))
        return false;
    if (!decoder.decode(colorSpace.fullRange))
        return false;
    return true;
}

}
