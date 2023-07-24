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

#include "GPUIntegralTypes.h"
#include "WebGPUBufferUsage.h"
#include <cstdint>
#include <wtf/RefCounted.h>

namespace WebCore {

using GPUBufferUsageFlags = uint32_t;

class GPUBufferUsage : public RefCounted<GPUBufferUsage> {
public:
    static constexpr GPUFlagsConstant MAP_READ      = 0x0001;
    static constexpr GPUFlagsConstant MAP_WRITE     = 0x0002;
    static constexpr GPUFlagsConstant COPY_SRC      = 0x0004;
    static constexpr GPUFlagsConstant COPY_DST      = 0x0008;
    static constexpr GPUFlagsConstant INDEX         = 0x0010;
    static constexpr GPUFlagsConstant VERTEX        = 0x0020;
    static constexpr GPUFlagsConstant UNIFORM       = 0x0040;
    static constexpr GPUFlagsConstant STORAGE       = 0x0080;
    static constexpr GPUFlagsConstant INDIRECT      = 0x0100;
    static constexpr GPUFlagsConstant QUERY_RESOLVE = 0x0200;
};

inline WebGPU::BufferUsageFlags convertBufferUsageFlagsToBacking(GPUBufferUsageFlags bufferUsageFlags)
{
    WebGPU::BufferUsageFlags result;
    if (bufferUsageFlags & GPUBufferUsage::MAP_READ)
        result.add(WebGPU::BufferUsage::MapRead);
    if (bufferUsageFlags & GPUBufferUsage::MAP_WRITE)
        result.add(WebGPU::BufferUsage::MapWrite);
    if (bufferUsageFlags & GPUBufferUsage::COPY_SRC)
        result.add(WebGPU::BufferUsage::CopySource);
    if (bufferUsageFlags & GPUBufferUsage::COPY_DST)
        result.add(WebGPU::BufferUsage::CopyDestination);
    if (bufferUsageFlags & GPUBufferUsage::INDEX)
        result.add(WebGPU::BufferUsage::Index);
    if (bufferUsageFlags & GPUBufferUsage::VERTEX)
        result.add(WebGPU::BufferUsage::Vertex);
    if (bufferUsageFlags & GPUBufferUsage::UNIFORM)
        result.add(WebGPU::BufferUsage::Uniform);
    if (bufferUsageFlags & GPUBufferUsage::STORAGE)
        result.add(WebGPU::BufferUsage::Storage);
    if (bufferUsageFlags & GPUBufferUsage::INDIRECT)
        result.add(WebGPU::BufferUsage::Indirect);
    if (bufferUsageFlags & GPUBufferUsage::QUERY_RESOLVE)
        result.add(WebGPU::BufferUsage::QueryResolve);
    return result;
}

}
