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
    std::array<bool, DMABufFormat::c_maxPlanes> modifierPresent { false, false, false, false };
    std::array<uint64_t, DMABufFormat::c_maxPlanes> modifierValue { 0, 0, 0, 0 };
};

template<class Encoder>
void DMABufObject::encode(Encoder& encoder) &&
{
    encoder << handle << uint32_t(format.fourcc) << uint32_t(colorSpace) << width << height
        << WTFMove(releaseFlag) << WTFMove(fd) << offset << stride << modifierPresent << modifierValue;
}

template<class Decoder>
std::optional<DMABufObject> DMABufObject::decode(Decoder& decoder)
{
    auto handle = decoder.template decode<uintptr_t>();
    if (!handle)
        return std::nullopt;

    auto fourcc = decoder.template decode<uint32_t>();
    if (!fourcc)
        return std::nullopt;

    auto colorSpace = decoder.template decode<uint32_t>();
    if (!colorSpace)
        return std::nullopt;

    auto width = decoder.template decode<uint32_t>();
    if (!width)
        return std::nullopt;

    auto height = decoder.template decode<uint32_t>();
    if (!height)
        return std::nullopt;

    auto releaseFlag = decoder.template decode<DMABufReleaseFlag>();
    if (!releaseFlag)
        return std::nullopt;

    auto fd = decoder.template decode<std::array<UnixFileDescriptor, DMABufFormat::c_maxPlanes>>();
    if (!fd)
        return std::nullopt;

    auto offset = decoder.template decode<std::array<size_t, DMABufFormat::c_maxPlanes>>();
    if (!offset)
        return std::nullopt;

    auto stride = decoder.template decode<std::array<uint32_t, DMABufFormat::c_maxPlanes>>();
    if (!stride)
        return std::nullopt;

    auto modifierPresent = decoder.template decode<std::array<bool, DMABufFormat::c_maxPlanes>>();
    if (!modifierPresent)
        return std::nullopt;

    auto modifierValue = decoder.template decode<std::array<uint64_t, DMABufFormat::c_maxPlanes>>();
    if (!modifierValue)
        return std::nullopt;

    DMABufObject dmabufObject(*handle);
    dmabufObject.format = DMABufFormat::create(*fourcc);
    dmabufObject.colorSpace = DMABufColorSpace { *colorSpace };
    dmabufObject.width = *width;
    dmabufObject.height = *height;
    dmabufObject.releaseFlag = WTFMove(*releaseFlag);
    dmabufObject.fd = WTFMove(*fd);
    dmabufObject.offset = WTFMove(*offset);
    dmabufObject.stride = WTFMove(*stride);
    dmabufObject.modifierPresent = WTFMove(*modifierPresent);
    dmabufObject.modifierValue = WTFMove(*modifierValue);
    return dmabufObject;
}

} // namespace WebCore
