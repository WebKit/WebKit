/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include <cstdint>
#include <wtf/EnumTraits.h>
#include <wtf/OptionSet.h>

namespace WebCore::WebGPU {

enum class BufferUsage : uint16_t {
    MapRead         = 1 << 0,
    MapWrite        = 1 << 1,
    CopySource      = 1 << 2,
    CopyDestination = 1 << 3,
    Index           = 1 << 4,
    Vertex          = 1 << 5,
    Uniform         = 1 << 6,
    Storage         = 1 << 7,
    Indirect        = 1 << 8,
    QueryResolve    = 1 << 9,
};
using BufferUsageFlags = OptionSet<BufferUsage>;

} // namespace WebCore::WebGPU

namespace WTF {

template<> struct EnumTraits<WebCore::WebGPU::BufferUsage> {
    using values = EnumValues<
        WebCore::WebGPU::BufferUsage,
        WebCore::WebGPU::BufferUsage::MapRead,
        WebCore::WebGPU::BufferUsage::MapWrite,
        WebCore::WebGPU::BufferUsage::CopySource,
        WebCore::WebGPU::BufferUsage::CopyDestination,
        WebCore::WebGPU::BufferUsage::Index,
        WebCore::WebGPU::BufferUsage::Vertex,
        WebCore::WebGPU::BufferUsage::Uniform,
        WebCore::WebGPU::BufferUsage::Storage,
        WebCore::WebGPU::BufferUsage::Indirect,
        WebCore::WebGPU::BufferUsage::QueryResolve

    >;
};

} // namespace WTF
