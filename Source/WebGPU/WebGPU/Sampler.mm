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

#import "config.h"
#import "Sampler.h"

#import "APIConversions.h"
#import "Device.h"
#import <cmath>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/Base64.h>

namespace WebGPU {

__attribute__((no_destroy)) std::unique_ptr<Sampler::CachedSamplerStateContainer> Sampler::cachedSamplerStates = nullptr;
__attribute__((no_destroy)) std::unique_ptr<Sampler::RetainedSamplerStateContainer> Sampler::retainedSamplerStates = nullptr;
__attribute__((no_destroy)) std::unique_ptr<Sampler::CachedKeyContainer> Sampler::lastAccessedKeys = nullptr;
Lock Sampler::samplerStateLock;

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
    case WGPUCompareFunction_Undefined:
    case WGPUCompareFunction_Always:
        return MTLCompareFunctionAlways;
    case WGPUCompareFunction_Force32:
        ASSERT_NOT_REACHED();
        return MTLCompareFunctionAlways;
    }
}

static uint32_t miscHash(MTLSamplerDescriptor* descriptor)
{
    struct MTLSamplerDescriptorHash {
        union {
            struct {
                // Pack this all down for faster equality/hashing.
                uint32_t minFilter:2;
                uint32_t magFilter:2;
                uint32_t mipFilter:2;
                uint32_t sAddressMode:3;
                uint32_t tAddressMode:3;
                uint32_t rAddressMode:3;
                uint32_t normalizedCoords:1;
                uint32_t borderColor:2;
                uint32_t lodAverage:1;
                uint32_t compareFunction:3;
                uint32_t supportArgumentBuffers:1;

            };
            uint32_t miscHash;
        };
    };
    MTLSamplerDescriptorHash h {
        .minFilter = static_cast<uint32_t>(descriptor.minFilter),
        .magFilter = static_cast<uint32_t>(descriptor.magFilter),
        .mipFilter = static_cast<uint32_t>(descriptor.mipFilter),
        .sAddressMode = static_cast<uint32_t>(descriptor.sAddressMode),
        .tAddressMode = static_cast<uint32_t>(descriptor.tAddressMode),
        .rAddressMode = static_cast<uint32_t>(descriptor.rAddressMode),
        .normalizedCoords = static_cast<uint32_t>(descriptor.normalizedCoordinates),
        .borderColor = static_cast<uint32_t>(descriptor.borderColor),
        .lodAverage = static_cast<uint32_t>(descriptor.lodAverage),
        .compareFunction = static_cast<uint32_t>(descriptor.compareFunction),
        .supportArgumentBuffers = static_cast<uint32_t>(descriptor.supportArgumentBuffers),
    };
    return h.miscHash;
}

static Sampler::UniqueSamplerIdentifier computeDescriptorHash(MTLSamplerDescriptor* descriptor)
{
    auto floatToUint32 = ^(float f) {
        return *reinterpret_cast<uint32_t*>(&f);
    };
    std::array<uint32_t, 4> uintData = { miscHash(descriptor), floatToUint32(descriptor.lodMinClamp), floatToUint32(descriptor.lodMaxClamp), floatToUint32(descriptor.maxAnisotropy) };
    return base64EncodeToString(asByteSpan(std::span { uintData }));
}

static MTLSamplerDescriptor *createMetalDescriptorFromDescriptor(const WGPUSamplerDescriptor &descriptor)
{
    MTLSamplerDescriptor *samplerDescriptor = [MTLSamplerDescriptor new];

    samplerDescriptor.sAddressMode = addressMode(descriptor.addressModeU);
    samplerDescriptor.tAddressMode = addressMode(descriptor.addressModeV);
    samplerDescriptor.rAddressMode = addressMode(descriptor.addressModeW);
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
    samplerDescriptor.label = descriptor.label.createNSString().get();

    return samplerDescriptor;
}

Ref<Sampler> Device::createSampler(const WGPUSamplerDescriptor& descriptor)
{
    if (!isValid())
        return Sampler::createInvalid(*this);

    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createsampler

    if (!validateCreateSampler(*this, descriptor)) {
        generateAValidationError("Validation failure."_s);
        return Sampler::createInvalid(*this);
    }

    MTLSamplerDescriptor* samplerDescriptor = createMetalDescriptorFromDescriptor(descriptor);
    return Sampler::create(computeDescriptorHash(samplerDescriptor), descriptor, *this);
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(Sampler);

Sampler::Sampler(UniqueSamplerIdentifier&& samplerIdentifier, const WGPUSamplerDescriptor& descriptor, Device& device)
    : m_samplerIdentifier(samplerIdentifier)
    , m_descriptor(descriptor)
    , m_device(device)
{
    m_cachedSamplerState = samplerState();
}

Sampler::Sampler(Device& device)
    : m_device(device)
{
}

Sampler::~Sampler()
{
    if (!m_samplerIdentifier)
        return;

    Locker locker { samplerStateLock };
    if (auto it = retainedSamplerStates->find(*m_samplerIdentifier); it != retainedSamplerStates->end()) {
        it->value.apiSamplerList.remove(this);
        if (!it->value.apiSamplerList.size())
            retainedSamplerStates->remove(it);
    }
}

void Sampler::setLabel(String&& label)
{
    m_descriptor.label = label;
}

bool Sampler::isValid() const
{
    return !!m_samplerIdentifier;
}

id<MTLSamplerState> Sampler::samplerState() const
{
    if (!m_samplerIdentifier)
        return nil;

    if (m_cachedSamplerState)
        return m_cachedSamplerState;

    Locker locker { samplerStateLock };
    if (!cachedSamplerStates) {
        cachedSamplerStates = WTF::makeUnique<CachedSamplerStateContainer>();
        retainedSamplerStates = WTF::makeUnique<RetainedSamplerStateContainer>();
        lastAccessedKeys = WTF::makeUnique<CachedKeyContainer>();
    }

    id<MTLSamplerState> samplerState = nil;
    auto samplerIdentifier = *m_samplerIdentifier;
    if (auto it = retainedSamplerStates->find(samplerIdentifier); it != retainedSamplerStates->end()) {
        samplerState = it->value.samplerState.get();
        it->value.apiSamplerList.add(this);
        lastAccessedKeys->appendOrMoveToLast(samplerIdentifier);
        if ((m_cachedSamplerState = samplerState))
            return samplerState;
    }

    id<MTLDevice> device = m_device->device();
    if (!device)
        return nil;
    auto maxArgumentBufferSamplerCount = std::min<NSUInteger>(2048, device.maxArgumentBufferSamplerCount);
    if (cachedSamplerStates->size() >= maxArgumentBufferSamplerCount / 2) {
        cachedSamplerStates->removeIf([&] (auto& pair) {
            if (!pair.value.get().get()) {
                lastAccessedKeys->remove(pair.key);
                return true;
            }
            return false;
        });
    }
    if (cachedSamplerStates->size() >= maxArgumentBufferSamplerCount)
        return nil;

    samplerState = [device newSamplerStateWithDescriptor:createMetalDescriptorFromDescriptor(m_descriptor)];
    if (!samplerState)
        return nil;

    cachedSamplerStates->set(samplerIdentifier, samplerState);
    auto addResult = retainedSamplerStates->add(samplerIdentifier, SamplerStateWithReferences {
        .samplerState = samplerState,
        .apiSamplerList = { }
    });
    addResult.iterator->value.apiSamplerList.add(this);
    lastAccessedKeys->appendOrMoveToLast(samplerIdentifier);

    m_cachedSamplerState = samplerState;

    return samplerState;
}

id<MTLSamplerState> Sampler::cachedSampler() const
{
    return m_cachedSamplerState;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuSamplerReference(WGPUSampler sampler)
{
    WebGPU::fromAPI(sampler).ref();
}

void wgpuSamplerRelease(WGPUSampler sampler)
{
    WebGPU::fromAPI(sampler).deref();
}

void wgpuSamplerSetLabel(WGPUSampler sampler, const char* label)
{
    WebGPU::protectedFromAPI(sampler)->setLabel(WebGPU::fromAPI(label));
}
