/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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

#import <wtf/FastMalloc.h>
#import <wtf/HashMap.h>
#import <wtf/HashTraits.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/Vector.h>

struct WGPUBindGroupLayoutImpl {
};

namespace WebGPU {

enum class ShaderStage {
    Vertex = 0,
    Fragment = 1,
    Compute = 2
};

class Device;

// https://gpuweb.github.io/gpuweb/#gpubindgrouplayout
class BindGroupLayout : public WGPUBindGroupLayoutImpl, public RefCounted<BindGroupLayout> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct Entry {
        uint32_t binding;
        WGPUShaderStageFlags visibility;
        using BindingLayout = std::variant<WGPUBufferBindingLayout, WGPUSamplerBindingLayout, WGPUTextureBindingLayout, WGPUStorageTextureBindingLayout, WGPUExternalTextureBindingLayout>;
        BindingLayout bindingLayout;
        std::optional<uint32_t> vertexDynamicOffset;
        std::optional<uint32_t> fragmentDynamicOffset;
        std::optional<uint32_t> computeDynamicOffset;
    };
    using EntriesContainer = HashMap<uint32_t, Entry, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;

#if USE(METAL_ARGUMENT_ACCESS_ENUMS)
    using BindingAccess = MTLArgumentAccess;
    static constexpr auto BindingAccessReadOnly = MTLArgumentAccessReadOnly;
    static constexpr auto BindingAccessReadWrite = MTLArgumentAccessReadWrite;
    static constexpr auto BindingAccessWriteOnly = MTLArgumentAccessWriteOnly;
#else
    using BindingAccess = MTLBindingAccess;
    static constexpr auto BindingAccessReadOnly = MTLBindingAccessReadOnly;
    static constexpr auto BindingAccessReadWrite = MTLBindingAccessReadWrite;
    static constexpr auto BindingAccessWriteOnly = MTLBindingAccessWriteOnly;
#endif
    using StageMapValue = BindingAccess;
    using StageMapTable = HashMap<uint64_t, StageMapValue, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>>;

    static Ref<BindGroupLayout> create(StageMapTable&& stageMapTable, id<MTLArgumentEncoder> vertexArgumentEncoder, id<MTLArgumentEncoder> fragmentArgumentEncoder, id<MTLArgumentEncoder> computeArgumentEncoder, EntriesContainer&& entries, size_t sizeOfVertexDynamicOffsets, size_t sizeOfFragmentDynamicOffsets, size_t sizeOfComputeDynamicOffsets)
    {
        return adoptRef(*new BindGroupLayout(WTFMove(stageMapTable), vertexArgumentEncoder, fragmentArgumentEncoder, computeArgumentEncoder, WTFMove(entries), sizeOfVertexDynamicOffsets, sizeOfFragmentDynamicOffsets, sizeOfComputeDynamicOffsets));
    }
    static Ref<BindGroupLayout> createInvalid(Device&)
    {
        return adoptRef(*new BindGroupLayout());
    }

    ~BindGroupLayout();

    void setLabel(String&&);

    bool isValid() const { return m_valid; }

    NSUInteger encodedLength(ShaderStage) const;

    id<MTLArgumentEncoder> vertexArgumentEncoder() const { return m_vertexArgumentEncoder; }
    id<MTLArgumentEncoder> fragmentArgumentEncoder() const { return m_fragmentArgumentEncoder; }
    id<MTLArgumentEncoder> computeArgumentEncoder() const { return m_computeArgumentEncoder; }

    std::optional<StageMapValue> bindingAccessForBindingIndex(uint32_t bindingIndex, ShaderStage renderStage) const;

    static bool isPresent(const WGPUBufferBindingLayout&);
    static bool isPresent(const WGPUSamplerBindingLayout&);
    static bool isPresent(const WGPUTextureBindingLayout&);
    static bool isPresent(const WGPUStorageTextureBindingLayout&);
    static bool isPresent(const WGPUExternalTextureBindingLayout&);

    const EntriesContainer& entries() const { return m_bindGroupLayoutEntries; }
    uint32_t sizeOfVertexDynamicOffsets() const;
    uint32_t sizeOfFragmentDynamicOffsets() const;
    uint32_t sizeOfComputeDynamicOffsets() const;

private:
    BindGroupLayout(StageMapTable&&, id<MTLArgumentEncoder>, id<MTLArgumentEncoder>, id<MTLArgumentEncoder>, EntriesContainer&&, size_t sizeOfVertexDynamicOffsets, size_t sizeOfFragmentDynamicOffsets, size_t sizeOfComputeDynamicOffsets);
    explicit BindGroupLayout();

    const StageMapTable m_indicesForBinding;

    const id<MTLArgumentEncoder> m_vertexArgumentEncoder { nil };
    const id<MTLArgumentEncoder> m_fragmentArgumentEncoder { nil };
    const id<MTLArgumentEncoder> m_computeArgumentEncoder { nil };

    const EntriesContainer m_bindGroupLayoutEntries;
    const bool m_valid { true };
    const size_t m_sizeOfVertexDynamicOffsets { 0 };
    const size_t m_sizeOfFragmentDynamicOffsets { 0 };
    const size_t m_sizeOfComputeDynamicOffsets { 0 };
};

} // namespace WebGPU
