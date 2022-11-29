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

struct DMABufObject {
    WTF_MAKE_NONCOPYABLE(DMABufObject);

    DMABufObject(uintptr_t handle)
        : handle(handle)
    { }

    ~DMABufObject() = default;

    DMABufObject(DMABufObject&&) = default;
    DMABufObject& operator=(DMABufObject&&) = default;

    template<class Encoder> void encode(Encoder&) &&;
    template<class Decoder> static std::optional<DMABufObject> decode(Decoder&);

    uintptr_t handle { 0 };
    DMABufFormat format { };
    DMABufColorSpace colorSpace { DMABufColorSpace::Invalid };
    uint32_t width { 0 };
    uint32_t height { 0 };
    DMABufReleaseFlag releaseFlag { };
    std::array<UnixFileDescriptor, DMABufFormat::c_maxPlanes> fd { };
    std::array<size_t, DMABufFormat::c_maxPlanes> offset { 0, 0, 0, 0 };
    std::array<uint32_t, DMABufFormat::c_maxPlanes> stride { 0, 0, 0, 0 };
    std::array<uint64_t, DMABufFormat::c_maxPlanes> modifier { 0, 0, 0, 0 };
};

template<class Encoder>
void DMABufObject::encode(Encoder& encoder) &&
{
    encoder << handle << uint32_t(format.fourcc) << uint32_t(colorSpace) << width << height;
    encoder << WTFMove(releaseFlag.fd);

    for (unsigned i = 0; i < DMABufFormat::c_maxPlanes; ++i) {
        encoder << WTFMove(fd[i]);
        encoder << offset[i] << stride[i] << modifier[i];
    }
}

template<class Decoder>
std::optional<DMABufObject> DMABufObject::decode(Decoder& decoder)
{
    std::optional<uintptr_t> handle;
    decoder >> handle;
    if (!handle)
        return std::nullopt;
    std::optional<uint32_t> fourcc;
    decoder >> fourcc;
    if (!fourcc)
        return std::nullopt;
    std::optional<uint32_t> colorSpace;
    decoder >> colorSpace;
    if (!colorSpace)
        return std::nullopt;
    std::optional<uint32_t> width;
    decoder >> width;
    if (!width)
        return std::nullopt;
    std::optional<uint32_t> height;
    decoder >> height;
    if (!height)
        return std::nullopt;

    DMABufObject dmabufObject(*handle);
    dmabufObject.format = DMABufFormat::create(*fourcc);
    dmabufObject.colorSpace = DMABufColorSpace { *colorSpace };
    dmabufObject.width = *width;
    dmabufObject.height = *height;

    std::optional<WTF::UnixFileDescriptor> releaseFlag;
    decoder >> releaseFlag;
    if (!releaseFlag)
        return std::nullopt;
    dmabufObject.releaseFlag.fd = WTFMove(*releaseFlag);

    for (unsigned i = 0; i < DMABufFormat::c_maxPlanes; ++i) {
        std::optional<WTF::UnixFileDescriptor> fd;
        decoder >> fd;
        if (!fd)
            return std::nullopt;
        dmabufObject.fd[i] = WTFMove(*fd);

        std::optional<size_t> offset;
        decoder >> offset;
        if (!offset)
            return std::nullopt;
        dmabufObject.offset[i] = *offset;

        std::optional<uint32_t> stride;
        decoder >> stride;
        if (!stride)
            return std::nullopt;
        dmabufObject.stride[i] = *stride;

        std::optional<uint64_t> modifier;
        decoder >> modifier;
        if (!modifier)
            return std::nullopt;
        dmabufObject.modifier[i] = *modifier;
    }

    return dmabufObject;
}

} // namespace WebCore
