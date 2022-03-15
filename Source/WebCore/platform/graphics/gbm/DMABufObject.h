/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
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

#include "DMABufFormat.h"
#include "DMABufReleaseFlag.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <unistd.h>

namespace WebCore {

struct DMABufObject {
    DMABufObject(uintptr_t handle)
        : handle(handle)
    { }

    ~DMABufObject()
    {
        for (unsigned i = 0; i < format.numPlanes; ++i) {
            if (fd[i] != -1)
                close(fd[i]);
        }
    }

    DMABufObject(const DMABufObject&) = delete;
    DMABufObject& operator=(const DMABufObject&) = delete;

    DMABufObject(DMABufObject&& o)
        : handle(o.handle)
        , format(o.format)
        , width(o.width)
        , height(o.height)
        , releaseFlag(WTFMove(o.releaseFlag))
    {
        for (unsigned i = 0; i < format.numPlanes; ++i) {
            fd[i] = o.fd[i];
            o.fd[i] = -1;

            offset[i] = o.offset[i];
            stride[i] = o.stride[i];
            modifier[i] = o.modifier[i];
        }
    }

    DMABufObject& operator=(DMABufObject&& o)
    {
        if (this == &o)
            return *this;

        this->~DMABufObject();
        new (this) DMABufObject(WTFMove(o));
        return *this;
    }

    uintptr_t handle { 0 };
    DMABufFormat format { };
    uint32_t width { 0 };
    uint32_t height { 0 };
    DMABufReleaseFlag releaseFlag { };
    std::array<int, DMABufFormat::c_maxPlanes> fd { -1, -1, -1, -1 };
    std::array<size_t, DMABufFormat::c_maxPlanes> offset { 0, 0, 0, 0 };
    std::array<uint32_t, DMABufFormat::c_maxPlanes> stride { 0, 0, 0, 0 };
    std::array<uint64_t, DMABufFormat::c_maxPlanes> modifier { 0, 0, 0, 0 };
};

} // namespace WebCore
