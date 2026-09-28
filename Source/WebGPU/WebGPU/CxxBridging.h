/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CommandBuffer.h"
#include "ComputePassEncoder.h"
#include "IsValidToUseWith.h"
#include "RenderPassEncoder.h"
#include <span>
#include <wtf/CheckedArithmetic.h>
#include <wtf/MathExtras.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

inline unsigned long roundUpToMultipleOfNonPowerOfTwoCheckedUInt32UnsignedLong(Checked<uint32_t> x, unsigned long y) { return WTF::roundUpToMultipleOfNonPowerOfTwo(x, y); }
inline uint32_t roundUpToMultipleOfNonPowerOfTwoUInt32UInt32(uint32_t a, uint32_t b) { return WTF::roundUpToMultipleOfNonPowerOfTwo<uint32_t, Checked<uint32_t>>(a, b); }

namespace CxxBridging {

using RefComputePassEncoder = Ref<WebGPU::Metal::ComputePassEncoder>;
using RefRenderPassEncoder = Ref<WebGPU::Metal::RenderPassEncoder>;
using RefCommandBuffer = Ref<WebGPU::Metal::CommandBuffer>;

inline bool isValidToUseWithTextureViewCommandEncoder(const WebGPU::Metal::TextureView& texture, const WebGPU::Metal::CommandEncoder& commandEncoder)
{
    return WebGPU::Metal::isValidToUseWith(texture, commandEncoder);
}

inline bool isValidToUseWithQuerySetCommandEncoder(const WebGPU::Metal::QuerySet& querySet, const WebGPU::Metal::CommandEncoder& commandEncoder)
{
    return WebGPU::Metal::isValidToUseWith(querySet, commandEncoder);
}

inline bool isValidToUseWithBufferCommandEncoder(const WebGPU::Metal::Buffer& buffer, const WebGPU::Metal::CommandEncoder& commandEncoder)
{
    return WebGPU::Metal::isValidToUseWith(buffer, commandEncoder);
}

inline bool isValidToUseWithTextureCommandEncoder(const WebGPU::Metal::Texture& texture, const WebGPU::Metal::CommandEncoder& commandEncoder)
{
    return WebGPU::Metal::isValidToUseWith(texture, commandEncoder);
}

inline bool isValidToUseWith(const WebGPU::Metal::TextureOrTextureView& texture, const WebGPU::Metal::CommandEncoder& commandEncoder)
{
    return WebGPU::Metal::isValidToUseWith(texture, commandEncoder);
}

// FIXME: rdar://138415945
inline bool areBuffersEqual(const WebGPU::Metal::Buffer& a, const WebGPU::Metal::Buffer& b)
{
    return &a == &b;
}

inline NSString * convertWTFStringToNSString(const String& input)
{
    return nsStringNilIfEmpty(input).autorelease();
}

inline ThreadSafeWeakPtr<WebGPU::Metal::CommandBuffer> commandBufferThreadSafeWeakPtr(const WebGPU::Metal::CommandBuffer* input)
{
    return ThreadSafeWeakPtr(input);
}

}
