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

#import "config.h"
#import "Sampler.h"

#import "APIConversions.h"
#import "Device.h"
#import <cmath>

namespace WebGPU {

static bool validateCreateSampler(Device&, const WGPUSamplerDescriptor& descriptor)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpusamplerdescriptor

    // FIXME: "device is valid."

    // "descriptor.lodMinClamp is greater than or equal to 0."
    if (std::isnan(descriptor.lodMinClamp) || descriptor.lodMinClamp < 0)
        return false;

    // "descriptor.lodMaxClamp is greater than or equal to descriptor.lodMinClamp."
    if (std::isnan(descriptor.lodMaxClamp) || descriptor.lodMaxClamp < descriptor.lodMinClamp)
        return false;

    // "descriptor.maxAnisotropy is greater than or equal to 1."
    if (descriptor.maxAnisotropy < 1)
        return false;

    // "descriptor.maxAnisotropy is less than or equal to 16."
    if (descriptor.maxAnisotropy > 16)
        return false;

    // "When descriptor.maxAnisotropy is greater than 1"
    if (descriptor.maxAnisotropy > 1) {
        // "descriptor.magFilter, descriptor.minFilter, and descriptor.mipmapFilter must be equal to "linear"."
        if (descriptor.magFilter != WGPUFilterMode_Linear
            || descriptor.minFilter != WGPUFilterMode_Linear
            || descriptor.mipmapFilter != WGPUFilterMode_Linear)
            return false;
    }

    return true;
}

RefPtr<Sampler> Device::createSampler(const WGPUSamplerDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return nullptr;

    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createsampler

    // "If validating GPUSamplerDescriptor(this, descriptor) returns false:"
    if (!validateCreateSampler(*this, descriptor)) {
        // FIXME: "Generate a validation error."

        // "Create a new invalid GPUSampler and return the result."
        return nullptr;
    }

    MTLSamplerDescriptor *samplerDescriptor = [MTLSamplerDescriptor new];

    switch (descriptor.addressModeU) {
    case WGPUAddressMode_Repeat:
        samplerDescriptor.rAddressMode = MTLSamplerAddressModeRepeat;
        break;
    case WGPUAddressMode_MirrorRepeat:
        samplerDescriptor.rAddressMode = MTLSamplerAddressModeMirrorRepeat;
        break;
    case WGPUAddressMode_ClampToEdge:
        samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
        break;
    default:
        return nullptr;
    }

    switch (descriptor.addressModeV) {
    case WGPUAddressMode_Repeat:
        samplerDescriptor.sAddressMode = MTLSamplerAddressModeRepeat;
        break;
    case WGPUAddressMode_MirrorRepeat:
        samplerDescriptor.sAddressMode = MTLSamplerAddressModeMirrorRepeat;
        break;
    case WGPUAddressMode_ClampToEdge:
        samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
        break;
    default:
        return nullptr;
    }

    switch (descriptor.addressModeW) {
    case WGPUAddressMode_Repeat:
        samplerDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
        break;
    case WGPUAddressMode_MirrorRepeat:
        samplerDescriptor.tAddressMode = MTLSamplerAddressModeMirrorRepeat;
        break;
    case WGPUAddressMode_ClampToEdge:
        samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
        break;
    default:
        return nullptr;
    }

    switch (descriptor.magFilter) {
    case WGPUFilterMode_Nearest:
        samplerDescriptor.magFilter = MTLSamplerMinMagFilterNearest;
        break;
    case WGPUFilterMode_Linear:
        samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
        break;
    default:
        return nullptr;
    }

    switch (descriptor.minFilter) {
    case WGPUFilterMode_Nearest:
        samplerDescriptor.minFilter = MTLSamplerMinMagFilterNearest;
        break;
    case WGPUFilterMode_Linear:
        samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
        break;
    default:
        return nullptr;
    }

    switch (descriptor.mipmapFilter) {
    case WGPUFilterMode_Nearest:
        samplerDescriptor.mipFilter = MTLSamplerMipFilterNearest;
        break;
    case WGPUFilterMode_Linear:
        samplerDescriptor.mipFilter = MTLSamplerMipFilterLinear;
        break;
    default:
        return nullptr;
    }

    samplerDescriptor.lodMinClamp = descriptor.lodMinClamp;

    samplerDescriptor.lodMaxClamp = descriptor.lodMaxClamp;

    switch (descriptor.compare) {
    case WGPUCompareFunction_Undefined:
        break;
    case WGPUCompareFunction_Never:
        samplerDescriptor.compareFunction = MTLCompareFunctionNever;
        break;
    case WGPUCompareFunction_Less:
        samplerDescriptor.compareFunction = MTLCompareFunctionLess;
        break;
    case WGPUCompareFunction_LessEqual:
        samplerDescriptor.compareFunction = MTLCompareFunctionLessEqual;
        break;
    case WGPUCompareFunction_Greater:
        samplerDescriptor.compareFunction = MTLCompareFunctionGreater;
        break;
    case WGPUCompareFunction_GreaterEqual:
        samplerDescriptor.compareFunction = MTLCompareFunctionGreaterEqual;
        break;
    case WGPUCompareFunction_Equal:
        samplerDescriptor.compareFunction = MTLCompareFunctionEqual;
        break;
    case WGPUCompareFunction_NotEqual:
        samplerDescriptor.compareFunction = MTLCompareFunctionNotEqual;
        break;
    case WGPUCompareFunction_Always:
        samplerDescriptor.compareFunction = MTLCompareFunctionAlways;
        break;
    default:
        return nullptr;
    }

    samplerDescriptor.maxAnisotropy = descriptor.maxAnisotropy;

    samplerDescriptor.label = descriptor.label ? [NSString stringWithCString:descriptor.label encoding:NSUTF8StringEncoding] : nil;

    id<MTLSamplerState> samplerState = [m_device newSamplerStateWithDescriptor:samplerDescriptor];
    if (!samplerState)
        return nullptr;

    return Sampler::create(samplerState, descriptor);
}

Sampler::Sampler(id<MTLSamplerState> samplerState, const WGPUSamplerDescriptor& descriptor)
    : m_samplerState(samplerState)
    , m_descriptor(descriptor)
{
}

Sampler::~Sampler() = default;

void Sampler::setLabel(String&&)
{
    // MTLRenderPipelineState's labels are read-only.
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuSamplerRelease(WGPUSampler sampler)
{
    delete sampler;
}

void wgpuSamplerSetLabel(WGPUSampler sampler, const char* label)
{
    WebGPU::fromAPI(sampler).setLabel(WebGPU::fromAPI(label));
}
