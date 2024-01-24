/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#import <Metal/Metal.h>
#import <wtf/OptionSet.h>
#import <wtf/Vector.h>
#import <wtf/WeakPtr.h>

namespace WebGPU {

class Buffer;
class ExternalTexture;
class TextureView;

enum class BindGroupEntryUsage {
    Undefined = 0,
    Input = 1 << 0,
    Constant = 1 << 1,
    Storage = 1 << 2,
    StorageRead = 1 << 3,
    Attachment = 1 << 4,
    AttachmentRead = 1 << 5,
    ConstantTexture = 1 << 6,
    StorageTextureWriteOnly = 1 << 7,
    StorageTextureRead = 1 << 8,
    StorageTextureReadWrite = 1 << 9,
};

static constexpr auto isTextureBindGroupEntryUsage(OptionSet<BindGroupEntryUsage> usage)
{
    return usage.toRaw() >= static_cast<std::underlying_type<BindGroupEntryUsage>::type>(BindGroupEntryUsage::Attachment);
}

struct BindGroupEntryUsageData {
    OptionSet<BindGroupEntryUsage> usage { BindGroupEntryUsage::Undefined };
    uint32_t binding { 0 };
    using Resource = std::variant<WeakPtr<const Buffer>, WeakPtr<const TextureView>, WeakPtr<const ExternalTexture>>;
    Resource resource;
    static constexpr uint32_t invalidBindingIndex = INT_MAX;
    static constexpr BindGroupEntryUsage invalidBindGroupUsage = static_cast<BindGroupEntryUsage>(std::numeric_limits<std::underlying_type<BindGroupEntryUsage>::type>::max());
};

struct BindableResources {
    Vector<id<MTLResource>> mtlResources;
    Vector<BindGroupEntryUsageData> resourceUsages;
    MTLResourceUsage usage;
    MTLRenderStages renderStages;
};

} // namespace WebGPU
