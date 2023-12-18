/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

namespace WTF {
class PrintStream;
class String;
}

namespace WGSL {

#define ENUM_AddressSpace(value) \
    value(Function, function) \
    value(Handle, handle) \
    value(Private, private) \
    value(Storage, storage) \
    value(Uniform, uniform) \
    value(Workgroup, workgroup) \

#define ENUM_AccessMode(value) \
    value(Read, read) \
    value(ReadWrite, read_write) \
    value(Write, write) \

#define ENUM_TexelFormat(value) \
    value(BGRA8unorm, bgra8unorm) \
    value(R32float, r32float) \
    value(R32sint, r32sint) \
    value(R32uint, r32uint) \
    value(RG32float, rg32float) \
    value(RG32sint, rg32sint) \
    value(RG32uint, rg32uint) \
    value(RGBA16float, rgba16float) \
    value(RGBA16sint, rgba16sint) \
    value(RGBA16uint, rgba16uint) \
    value(RGBA32float, rgba32float) \
    value(RGBA32sint, rgba32sint) \
    value(RGBA32uint, rgba32uint) \
    value(RGBA8sint, rgba8sint) \
    value(RGBA8snorm, rgba8snorm) \
    value(RGBA8uint, rgba8uint) \
    value(RGBA8unorm, rgba8unorm) \

#define ENUM_InterpolationType(value) \
    value(Flat, flat) \
    value(Linear, linear) \
    value(Perspective, perspective)

#define ENUM_InterpolationSampling(value) \
    value(Center, center) \
    value(Centroid, centroid) \
    value(Sample, sample)

#define ENUM_ShaderStage(value) \
    value(Compute,  compute,  1 << 2) \
    value(Fragment, fragment, 1 << 1) \
    value(Vertex,   vertex,   1 << 0) \

#define ENUM_SeverityControl(value) \
    value(Error, error) \
    value(Info, info) \
    value(Off, off) \
    value(Warning, warning) \

#define ENUM_Builtin(value) \
    value(FragDepth, frag_depth) \
    value(FrontFacing, front_facing) \
    value(GlobalInvocationId, global_invocation_id) \
    value(InstanceIndex, instance_index) \
    value(LocalInvocationId, local_invocation_id) \
    value(LocalInvocationIndex, local_invocation_index) \
    value(NumWorkgroups, num_workgroups) \
    value(Position, position) \
    value(SampleIndex, sample_index) \
    value(SampleMask, sample_mask) \
    value(VertexIndex, vertex_index) \
    value(WorkgroupId, workgroup_id) \

#define ENUM_Extension(value) \
    value(F16, f16, 1 << 0) \

#define ENUM_LanguageFeature(value) \
    value(ReadonlyAndReadwriteStorageTextures, readonly_and_readwrite_storage_textures, 1 << 0)

#define ENUM_DECLARE_VALUE(__value, _, ...) \
    __value __VA_OPT__(=) __VA_ARGS__,

#define ENUM_DECLARE_PRINT_INTERNAL(__name) \
    void printInternal(WTF::PrintStream& out, __name)

#define ENUM_DECLARE_PARSE(__name) \
    const __name* parse##__name(const WTF::String&)

#define ENUM_DECLARE(__name) \
    enum class __name : uint8_t { \
    ENUM_##__name(ENUM_DECLARE_VALUE) \
    }; \
    ENUM_DECLARE_PRINT_INTERNAL(__name); \
    ENUM_DECLARE_PARSE(__name);

ENUM_DECLARE(AddressSpace);
ENUM_DECLARE(AccessMode);
ENUM_DECLARE(TexelFormat);
ENUM_DECLARE(InterpolationType);
ENUM_DECLARE(InterpolationSampling);
ENUM_DECLARE(ShaderStage);
ENUM_DECLARE(SeverityControl);
ENUM_DECLARE(Builtin);
ENUM_DECLARE(Extension);
ENUM_DECLARE(LanguageFeature);

#undef ENUM_DECLARE

AccessMode defaultAccessModeForAddressSpace(AddressSpace);

} // namespace WGSL
