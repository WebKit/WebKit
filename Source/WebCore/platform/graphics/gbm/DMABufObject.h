/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022-2024 Igalia S.L.
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

#include "DMABufColorSpace.h"
#include "DMABufFormat.h"
#include "DMABufReleaseFlag.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unistd.h>
#include <wtf/Noncopyable.h>
#include <wtf/unix/UnixFileDescriptor.h>

namespace WebCore {

class DMABufObject {
    WTF_MAKE_NONCOPYABLE(DMABufObject);

public:
    DMABufObject(DMABufObject&&) = default;

    explicit DMABufObject(intptr_t handle)
        : handle(handle)
    { }

    DMABufObject& operator=(DMABufObject&&) = default;

    uintptr_t handle { 0 };
    DMABufFormat format { };
    DMABufColorSpace colorSpace { DMABufColorSpace::Invalid };
    uint32_t width { 0 };
    uint32_t height { 0 };
    DMABufReleaseFlag releaseFlag { };
    std::array<UnixFileDescriptor, DMABufFormat::c_maxPlanes> fd { };
    std::array<size_t, DMABufFormat::c_maxPlanes> offset { 0, 0, 0, 0 };
    std::array<uint32_t, DMABufFormat::c_maxPlanes> stride { 0, 0, 0, 0 };
    std::array<bool, DMABufFormat::c_maxPlanes> modifierPresent { false, false, false, false };
    std::array<uint64_t, DMABufFormat::c_maxPlanes> modifierValue { 0, 0, 0, 0 };

private:
    explicit DMABufObject(uintptr_t handle, DMABufFormat format, DMABufColorSpace colorSpace, uint32_t width, uint32_t height,
        DMABufReleaseFlag releaseFlag, std::array<UnixFileDescriptor, DMABufFormat::c_maxPlanes>&& fd,
        std::array<size_t, DMABufFormat::c_maxPlanes>&& offset, std::array<uint32_t, DMABufFormat::c_maxPlanes>&& stride,
        std::array<bool, DMABufFormat::c_maxPlanes>&& modifierPresent, std::array<uint64_t, DMABufFormat::c_maxPlanes>&& modifierValue)
            : handle(handle)
            , format(format)
            , colorSpace(colorSpace)
            , width(width)
            , height(height)
            , releaseFlag(WTFMove(releaseFlag))
            , fd(WTFMove(fd))
            , offset(WTFMove(offset))
            , stride(WTFMove(stride))
            , modifierPresent(WTFMove(modifierPresent))
            , modifierValue(WTFMove(modifierValue))
    { }

    friend struct IPC::ArgumentCoder<DMABufObject, void>;
};

} // namespace WebCore
