/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGPU)

#include <wtf/OptionSet.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace WHLSL {

enum class VertexFormat : uint8_t {
    FloatR32G32B32A32,
    FloatR32G32B32,
    FloatR32G32,
    FloatR32
};

struct VertexAttribute {
    VertexFormat vertexFormat;
    unsigned shaderLocation;
    unsigned metalLocation;
};

using VertexAttributes = Vector<VertexAttribute>;

// FIXME: Add back < 32 bit texture values and figure out how to box them in wider types when we load from them.
// https://bugs.webkit.org/show_bug.cgi?id=200315

enum class TextureFormat {
    R8Unorm,
    R8UnormSrgb,
    R8Snorm,
    R16Unorm,
    R16Snorm,
    R16Float,
    RG8Unorm,
    RG8UnormSrgb,
    RG8Snorm,
    B5G6R5Unorm,
    R32Uint,
    R32Sint,
    R32Float,
    RG16Unorm,
    RG16Snorm,
    RG16Float,
    RGBA8Unorm,
    RGBA8UnormSrgb,
    RGBA8Snorm,
    BGRA8Unorm,
    BGRA8UnormSrgb,
    RGB10A2Unorm,
    RG11B10Float,
    RG32Uint,
    RG32Sint,
    RG32Float,
    RGBA16Unorm,
    RGBA16Snorm,
    RGBA16Float,
    RGBA32Uint,
    RGBA32Sint,
    RGBA32Float
};

struct AttachmentDescriptor {
    TextureFormat textureFormat;
    unsigned name;
};

struct AttachmentsStateDescriptor {
    Vector<AttachmentDescriptor> attachmentDescriptors;
    Optional<AttachmentDescriptor> depthStencilAttachmentDescriptor;
};

enum class ShaderStage : uint8_t {
    None = 0,
    Vertex = 1 << 0,
    Fragment = 1 << 1,
    Compute = 1 << 2
};

struct UniformBufferBinding {
    unsigned lengthName;
};

struct SamplerBinding {
};

struct TextureBinding {
};

struct StorageBufferBinding {
    unsigned lengthName;
};

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=198168 Add the dynamic types

struct Binding {
    using BindingDetails = Variant<UniformBufferBinding, SamplerBinding, TextureBinding, StorageBufferBinding>;
    OptionSet<ShaderStage> visibility;
    BindingDetails binding;
    unsigned internalName;
    unsigned externalName;
};

struct BindGroup {
    Vector<Binding> bindings;
    unsigned name;
};

using Layout = Vector<BindGroup>;

struct RenderPipelineDescriptor {
    VertexAttributes vertexAttributes;
    AttachmentsStateDescriptor attachmentsStateDescriptor;
    Layout layout;
    String vertexEntryPointName;
    String fragmentEntryPointName;
};

struct ComputePipelineDescriptor {
    Layout layout;
    String entryPointName;
};

using PipelineDescriptor = Variant<RenderPipelineDescriptor, ComputePipelineDescriptor>;

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
