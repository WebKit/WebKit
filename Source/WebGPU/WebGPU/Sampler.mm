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

static bool validateCreateSampler(Device& device, const WGPUSamplerDescriptor& descriptor)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpusamplerdescriptor

    if (!device.isValid())
        return false;

    if (std::isnan(descriptor.lodMinClamp) || descriptor.lodMinClamp < 0)
        return false;

    if (std::isnan(descriptor.lodMaxClamp) || descriptor.lodMaxClamp < descriptor.lodMinClamp)
        return false;

    if (descriptor.maxAnisotropy < 1)
        return false;

    if (descriptor.maxAnisotropy > 1) {
        if (descriptor.magFilter != WGPUFilterMode_Linear
            || descriptor.minFilter != WGPUFilterMode_Linear
            || descriptor.mipmapFilter != WGPUMipmapFilterMode_Linear)
            return false;
    }

    return true;
}

static MTLSamplerAddressMode addressMode(WGPUAddressMode addressMode)
{
    switch (addressMode) {
    case WGPUAddressMode_Repeat:
        return MTLSamplerAddressModeRepeat;
    case WGPUAddressMode_MirrorRepeat:
        return MTLSamplerAddressModeMirrorRepeat;
    case WGPUAddressMode_ClampToEdge:
        return MTLSamplerAddressModeClampToEdge;
    case WGPUAddressMode_Force32:
        ASSERT_NOT_REACHED();
        return MTLSamplerAddressModeClampToEdge;
    }
}

static MTLSamplerMinMagFilter minMagFilter(WGPUFilterMode filterMode)
{
    switch (filterMode) {
    case WGPUFilterMode_Nearest:
        return MTLSamplerMinMagFilterNearest;
    case WGPUFilterMode_Linear:
        return MTLSamplerMinMagFilterLinear;
    case WGPUFilterMode_Force32:
        ASSERT_NOT_REACHED();
        return MTLSamplerMinMagFilterNearest;
    }
}

static MTLSamplerMipFilter mipFilter(WGPUMipmapFilterMode filterMode)
{
    switch (filterMode) {
    case WGPUMipmapFilterMode_Nearest:
        return MTLSamplerMipFilterNearest;
    case WGPUMipmapFilterMode_Linear:
        return MTLSamplerMipFilterLinear;
    case WGPUMipmapFilterMode_Force32:
        ASSERT_NOT_REACHED();
        return MTLSamplerMipFilterNearest;
    }
}

static MTLCompareFunction compareFunction(WGPUCompareFunction compareFunction)
{
    switch (compareFunction) {
    case WGPUCompareFunction_Never:
        return MTLCompareFunctionNever;
    case WGPUCompareFunction_Less:
        return MTLCompareFunctionLess;
    case WGPUCompareFunction_LessEqual:
        return MTLCompareFunctionLessEqual;
    case WGPUCompareFunction_Greater:
        return MTLCompareFunctionGreater;
    case WGPUCompareFunction_GreaterEqual:
        return MTLCompareFunctionGreaterEqual;
    case WGPUCompareFunction_Equal:
        return MTLCompareFunctionEqual;
    case WGPUCompareFunction_NotEqual:
        return MTLCompareFunctionNotEqual;
    case WGPUCompareFunction_Always:
        return MTLCompareFunctionAlways;
    case WGPUCompareFunction_Undefined:
    case WGPUCompareFunction_Force32:
        ASSERT_NOT_REACHED();
        return MTLCompareFunctionAlways;
    }
}

Ref<Sampler> Device::createSampler(const WGPUSamplerDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return Sampler::createInvalid(*this);

    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createsampler

    if (!validateCreateSampler(*this, descriptor)) {
        generateAValidationError("Validation failure."_s);
        return Sampler::createInvalid(*this);
    }

    MTLSamplerDescriptor *samplerDescriptor = [MTLSamplerDescriptor new];

    samplerDescriptor.rAddressMode = addressMode(descriptor.addressModeU);
    samplerDescriptor.sAddressMode = addressMode(descriptor.addressModeV);
    samplerDescriptor.tAddressMode = addressMode(descriptor.addressModeW);
    samplerDescriptor.magFilter = minMagFilter(descriptor.magFilter);
    samplerDescriptor.minFilter = minMagFilter(descriptor.minFilter);
    samplerDescriptor.mipFilter = mipFilter(descriptor.mipmapFilter);
    samplerDescriptor.lodMinClamp = descriptor.lodMinClamp;
    samplerDescriptor.lodMaxClamp = descriptor.lodMaxClamp;
    samplerDescriptor.compareFunction = compareFunction(descriptor.compare);
    samplerDescriptor.supportArgumentBuffers = YES;

    // https://developer.apple.com/documentation/metal/mtlsamplerdescriptor/1516164-maxanisotropy?language=objc
    // "Values must be between 1 and 16, inclusive."
    samplerDescriptor.maxAnisotropy = std::min<uint16_t>(descriptor.maxAnisotropy, 16);

    samplerDescriptor.label = fromAPI(descriptor.label);

    id<MTLSamplerState> samplerState = [m_device newSamplerStateWithDescriptor:samplerDescriptor];
    if (!samplerState)
        return Sampler::createInvalid(*this);

    return Sampler::create(samplerState, descriptor, *this);
}

Sampler::Sampler(id<MTLSamplerState> samplerState, const WGPUSamplerDescriptor& descriptor, Device& device)
    : m_samplerState(samplerState)
    , m_descriptor(descriptor)
    , m_device(device)
{
}

Sampler::Sampler(Device& device)
    : m_device(device)
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
    WebGPU::fromAPI(sampler).deref();
}

void wgpuSamplerSetLabel(WGPUSampler sampler, const char* label)
{
    WebGPU::fromAPI(sampler).setLabel(WebGPU::fromAPI(label));
}
