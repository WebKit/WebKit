/*
 * Copyright (C) 2024 Igalia S.L.
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

#if USE(SKIA)

#include <skia/core/SkColorSpace.h>

namespace WebKit {

class CoreIPCSkColorSpace {
public:
    CoreIPCSkColorSpace(sk_sp<SkColorSpace> skColorSpace)
        : m_skColorSpace(WTFMove(skColorSpace))
    {
    }

    CoreIPCSkColorSpace(std::span<const uint8_t> data)
        : m_skColorSpace(SkColorSpace::Deserialize(data.data(), data.size()))
    {
    }

    sk_sp<SkColorSpace> skColorSpace() const { return m_skColorSpace; }

    std::span<const uint8_t> dataReference() const
    {
        if (auto data = m_skColorSpace->serialize())
            return { data->bytes(), data->size() };
        return { };
    }

private:
    sk_sp<SkColorSpace> m_skColorSpace;
};

} // namespace WebKit

#endif // USE(SKIA)
