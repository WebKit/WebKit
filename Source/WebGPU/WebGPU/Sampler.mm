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
#import <wtf/TZoneMallocInlines.h>

@implementation SamplerIdentifier
- (instancetype)initWithFirst:(uint64_t)first second:(uint64_t)second
{
    if (!(self = [super init]))
        return nil;

    _first = first;
    _second = second;
    return self;
}
- (instancetype)copyWithZone:(NSZone *)zone
{
    return self;
}
@end

namespace WebGPU {

NSMutableDictionary<SamplerIdentifier*, id<MTLSamplerState>> *Sampler::cachedSamplerStates = nil;
NSMutableOrderedSet<SamplerIdentifier*> *Sampler::lastAccessedKeys = nil;
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

static uint64_t floatToUint64(float f)
{
    return static_cast<uint64_t>(*reinterpret_cast<uint32_t*>(&f));
}

static std::pair<uint64_t, uint64_t> computeDescriptorHash(MTLSamplerDescriptor* descriptor)
{
    std::pair<uint64_t, uint64_t> hash;
    hash.first = miscHash(descriptor) | (floatToUint64(descriptor.lodMinClamp) << 32);
    hash.second = floatToUint64(descriptor.lodMaxClamp) | (floatToUint64(descriptor.maxAnisotropy) << 32);
    return hash;
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
    samplerDescriptor.label = descriptor.label;

    return samplerDescriptor;
}

Ref<Sampler> Device::createSampler(const WGPUSamplerDescriptor& descriptor)
{
    if (descriptor.nextInChain || !isValid())
        return Sampler::createInvalid(*this);

    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createsampler

    if (!validateCreateSampler(*this, descriptor)) {
        generateAValidationError("Validation failure."_s);
        return Sampler::createInvalid(*this);
    }

    MTLSamplerDescriptor * samplerDescriptor = createMetalDescriptorFromDescriptor(descriptor);
    auto newDescriptorHash = computeDescriptorHash(samplerDescriptor);

    return Sampler::create([[SamplerIdentifier alloc] initWithFirst:newDescriptorHash.first second:newDescriptorHash.second], descriptor, *this);
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(Sampler);

Sampler::Sampler(SamplerIdentifier* samplerIdentifier, const WGPUSamplerDescriptor& descriptor, Device& device)
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
    if (m_samplerIdentifier) {
        Locker locker { samplerStateLock };
        [cachedSamplerStates removeObjectForKey:m_samplerIdentifier];
        [lastAccessedKeys removeObject:m_samplerIdentifier];
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

    Locker locker { samplerStateLock };
    if (!cachedSamplerStates) {
        cachedSamplerStates = [NSMutableDictionary dictionary];
        lastAccessedKeys = [NSMutableOrderedSet orderedSet];
    }

    id<MTLSamplerState> samplerState = [cachedSamplerStates objectForKey:m_samplerIdentifier];
    if (samplerState)
        return samplerState;

    id<MTLDevice> device = m_device->device();
    if (!device)
        return nil;
    if (cachedSamplerStates.count >= device.maxArgumentBufferSamplerCount) {
        if (!lastAccessedKeys.count)
            return nil;

        SamplerIdentifier* key = [lastAccessedKeys objectAtIndex:0];
        if (key)
            [cachedSamplerStates removeObjectForKey:key];
        [lastAccessedKeys removeObjectAtIndex:0];
        ASSERT(cachedSamplerStates.count < device.maxArgumentBufferSamplerCount);
    }

    samplerState = [device newSamplerStateWithDescriptor:createMetalDescriptorFromDescriptor(m_descriptor)];
    if (!samplerState)
        return nil;

    [cachedSamplerStates setObject:samplerState forKey:m_samplerIdentifier];
    [lastAccessedKeys addObject:m_samplerIdentifier];
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
