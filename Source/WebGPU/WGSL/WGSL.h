/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#include "CompilationMessage.h"
#include <cinttypes>
#include <cstdint>
#include <memory>
#include <variant>
#include <wtf/HashMap.h>
#include <wtf/OptionSet.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WGSL {

//
// Step 1
//

namespace AST {
class ShaderModule;
}

struct SuccessfulCheck {
    SuccessfulCheck() = delete;
    SuccessfulCheck(SuccessfulCheck&&);
    SuccessfulCheck(Vector<Warning>&&, UniqueRef<AST::ShaderModule>&&);
    ~SuccessfulCheck();
    Vector<Warning> warnings;
    UniqueRef<AST::ShaderModule> ast;
};

struct FailedCheck {
    Vector<Error> errors;
    Vector<Warning> warnings;
};

struct SourceMap {
    // I don't know what goes in here.
    // https://sourcemaps.info/spec.html
};

struct Configuration {
    uint32_t maxBuffersPlusVertexBuffersForVertexStage;
};

std::variant<SuccessfulCheck, FailedCheck> staticCheck(const String& wgsl, const std::optional<SourceMap>&, const Configuration&);

//
// Step 2
//

enum class BufferBindingType : uint8_t {
    Uniform,
    Storage,
    ReadOnlyStorage
};

struct BufferBindingLayout {
    BufferBindingType type;
    bool hasDynamicOffset;
    uint64_t minBindingSize;
};

enum class SamplerBindingType : uint8_t {
    Filtering,
    NonFiltering,
    Comparison
};

struct SamplerBindingLayout {
    SamplerBindingType type;
};

enum class TextureSampleType : uint8_t {
    Float,
    UnfilterableFloat,
    Depth,
    SignedInt,
    UnsignedInt
};

enum class TextureViewDimension : uint8_t {
    OneDimensional,
    TwoDimensional,
    TwoDimensionalArray,
    Cube,
    CubeArray,
    ThreeDimensional
};

struct TextureBindingLayout {
    TextureSampleType sampleType;
    TextureViewDimension viewDimension;
    bool multisampled;
};

/* enum class StorageTextureAccess : uint8_t {
    writeOnly
}; */

struct StorageTextureBindingLayout {
    // StorageTextureAccess access; // There's only one field in this enum
    // TextureFormat format; // Not sure this is necessary
    TextureViewDimension viewDimension;
};

struct ExternalTextureBindingLayout {
    // Sentinel
};

enum class ShaderStage : uint8_t {
    Vertex = 0x1,
    Fragment = 0x2,
    Compute = 0x4
};

struct BindGroupLayoutEntry {
    uint32_t binding;
    OptionSet<ShaderStage> visibility;
    std::variant<BufferBindingLayout, SamplerBindingLayout, TextureBindingLayout, StorageTextureBindingLayout, ExternalTextureBindingLayout> bindingMember;
};

struct BindGroupLayout {
    // Metal's [[id(n)]] indices are equal to the index into this vector.
    Vector<BindGroupLayoutEntry> entries;
};

struct PipelineLayout {
    // Metal's [[buffer(n)]] indices are equal to the index into this vector.
    Vector<BindGroupLayout> bindGroupLayouts;
};

namespace Reflection {

struct Vertex {
    bool invariantIsPresent;
    // Tons of reflection data to appear here in the future.
};

struct Fragment {
    // Tons of reflection data to appear here in the future.
};

struct WorkgroupSize {
    unsigned width;
    unsigned height;
    unsigned depth;
};

struct Compute {
    WorkgroupSize workgroupSize;
};

enum class SpecializationConstantType : uint8_t {
    Boolean,
    Float,
    Int,
    Unsigned
};

struct SpecializationConstant {
    String mangledName;
    SpecializationConstantType type;
};

struct EntryPointInformation {
    // FIXME: This can probably be factored better.
    String mangledName;
    std::optional<PipelineLayout> defaultLayout; // If the input PipelineLayout is nullopt, the compiler computes a layout and returns it. https://gpuweb.github.io/gpuweb/#default-pipeline-layout
    HashMap<std::pair<size_t, size_t>, size_t> bufferLengthLocations; // Metal buffer identity -> offset within helper buffer where its size needs to lie
    HashMap<size_t, SpecializationConstant> specializationConstants;
    HashMap<String, size_t> specializationConstantIndices; // Points into specializationConstantsByIndex
    std::variant<Vertex, Fragment, Compute> typedEntryPoint;
};

} // namespace Reflection

struct PrepareResult {
    String msl;
    HashMap<String, Reflection::EntryPointInformation> entryPoints;
};

// These are not allowed to fail.
// All failures must have already been caught in check().
PrepareResult prepare(AST::ShaderModule&, const HashMap<String, PipelineLayout>&);
PrepareResult prepare(AST::ShaderModule&, const String& entryPointName, const std::optional<PipelineLayout>&);

} // namespace WGSL
