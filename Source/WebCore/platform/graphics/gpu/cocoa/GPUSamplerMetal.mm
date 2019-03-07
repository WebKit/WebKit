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

#import "config.h"
#import "GPUSampler.h"

#if ENABLE(WEBGPU)

#import "GPUDevice.h"
#import "GPUSamplerDescriptor.h"
#import "GPUUtils.h"
#import "Logging.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

static MTLSamplerAddressMode mtlAddressModeForAddressMode(GPUSamplerDescriptor::AddressMode mode)
{
    switch (mode) {
    case GPUSamplerDescriptor::AddressMode::ClampToEdge:
        return MTLSamplerAddressModeClampToEdge;
    case GPUSamplerDescriptor::AddressMode::Repeat:
        return MTLSamplerAddressModeRepeat;
    case GPUSamplerDescriptor::AddressMode::MirrorRepeat:
        return MTLSamplerAddressModeMirrorRepeat;
    case GPUSamplerDescriptor::AddressMode::ClampToBorderColor:
        return MTLSamplerAddressModeClampToBorderColor;
    }

    ASSERT_NOT_REACHED();
}

static MTLSamplerBorderColor mtlBorderColorForBorderColor(GPUSamplerDescriptor::BorderColor color)
{
    switch (color) {
    case GPUSamplerDescriptor::BorderColor::TransparentBlack:
        return MTLSamplerBorderColorTransparentBlack;
    case GPUSamplerDescriptor::BorderColor::OpaqueBlack:
        return MTLSamplerBorderColorOpaqueBlack;
    case GPUSamplerDescriptor::BorderColor::OpaqueWhite:
        return MTLSamplerBorderColorOpaqueWhite;
    }

    ASSERT_NOT_REACHED();
}

static MTLSamplerMinMagFilter mtlMinMagFilterForFilterMode(GPUSamplerDescriptor::FilterMode mode)
{
    switch (mode) {
    case GPUSamplerDescriptor::FilterMode::Nearest:
        return MTLSamplerMinMagFilterNearest;
    case GPUSamplerDescriptor::FilterMode::Linear:
        return MTLSamplerMinMagFilterLinear;
    }

    ASSERT_NOT_REACHED();
}

static MTLSamplerMipFilter mtlMipFilterForFilterMode(GPUSamplerDescriptor::FilterMode mode)
{
    switch (mode) {
    case GPUSamplerDescriptor::FilterMode::Nearest:
        return MTLSamplerMipFilterNearest;
    case GPUSamplerDescriptor::FilterMode::Linear:
        return MTLSamplerMipFilterLinear;
    }

    ASSERT_NOT_REACHED();
}

static RetainPtr<MTLSamplerState> tryCreateMtlSamplerState(const GPUDevice& device, const GPUSamplerDescriptor& descriptor)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUSampler::tryCreate(): Invalid GPUDevice!");
        return nullptr;
    }

    RetainPtr<MTLSamplerDescriptor> mtlDescriptor;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    mtlDescriptor = adoptNS([MTLSamplerDescriptor new]);
    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlDescriptor) {
        LOG(WebGPU, "GPUSampler::tryCreate(): Error creating MTLSamplerDescriptor!");
        return nullptr;
    }

    RetainPtr<MTLSamplerState> sampler;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [mtlDescriptor setRAddressMode:mtlAddressModeForAddressMode(descriptor.addressModeU)];
    [mtlDescriptor setSAddressMode:mtlAddressModeForAddressMode(descriptor.addressModeV)];
    [mtlDescriptor setTAddressMode:mtlAddressModeForAddressMode(descriptor.addressModeW)];
    [mtlDescriptor setBorderColor:mtlBorderColorForBorderColor(descriptor.borderColor)];
    [mtlDescriptor setMinFilter:mtlMinMagFilterForFilterMode(descriptor.minFilter)];
    [mtlDescriptor setMagFilter:mtlMinMagFilterForFilterMode(descriptor.magFilter)];
    [mtlDescriptor setMipFilter:mtlMipFilterForFilterMode(descriptor.mipmapFilter)];
    [mtlDescriptor setLodMinClamp:descriptor.lodMinClamp];
    [mtlDescriptor setLodMaxClamp:descriptor.lodMaxClamp];
    [mtlDescriptor setMaxAnisotropy:descriptor.maxAnisotropy];
    [mtlDescriptor setCompareFunction:static_cast<MTLCompareFunction>(platformCompareFunctionForGPUCompareFunction(descriptor.compareFunction))];

    [mtlDescriptor setSupportArgumentBuffers:YES];

    sampler = adoptNS([device.platformDevice() newSamplerStateWithDescriptor:mtlDescriptor.get()]);
    END_BLOCK_OBJC_EXCEPTIONS;

    if (!sampler)
        LOG(WebGPU, "GPUSampler::tryCreate(): Error creating MTLSamplerState!");

    return sampler;
}

RefPtr<GPUSampler> GPUSampler::tryCreate(const GPUDevice& device, const GPUSamplerDescriptor& descriptor)
{
    auto sampler = tryCreateMtlSamplerState(device, descriptor);
    if (!sampler)
        return nullptr;

    return adoptRef(new GPUSampler(WTFMove(sampler)));
}

GPUSampler::GPUSampler(RetainPtr<MTLSamplerState>&& sampler)
    : m_platformSampler(WTFMove(sampler))
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
