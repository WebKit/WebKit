/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "GPUTexture.h"

#if ENABLE(WEBGPU)

#import "GPUDevice.h"
#import "GPUTextureDescriptor.h"
#import "GPUUtils.h"
#import "Logging.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/Optional.h>

namespace WebCore {

static MTLTextureType mtlTextureTypeForGPUTextureDescriptor(const GPUTextureDescriptor& descriptor)
{
    switch (descriptor.dimension) {
    case GPUTextureDimension::_1d:
        return (descriptor.arrayLayerCount == 1) ? MTLTextureType1D : MTLTextureType1DArray;
    case GPUTextureDimension::_2d: {
        if (descriptor.arrayLayerCount == 1)
            return (descriptor.sampleCount == 1) ? MTLTextureType2D : MTLTextureType2DMultisample;

        return MTLTextureType2DArray;
    }
    case GPUTextureDimension::_3d:
        return MTLTextureType3D;
    }
}

static Optional<MTLTextureUsage> mtlTextureUsageForGPUTextureUsageFlags(OptionSet<GPUTextureUsage::Flags> flags, const char* const functionName)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    if (flags.containsAny({ GPUTextureUsage::Flags::TransferSource, GPUTextureUsage::Flags::Sampled }) && (flags & GPUTextureUsage::Flags::Storage)) {
        LOG(WebGPU, "%s: Texture cannot have both STORAGE and a read-only usage!", functionName);
        return WTF::nullopt;
    }

    if (flags & GPUTextureUsage::Flags::OutputAttachment) {
        if (flags.containsAny({ GPUTextureUsage::Flags::Storage, GPUTextureUsage::Flags::Sampled })) {
            LOG(WebGPU, "%s: Texture cannot have OUTPUT_ATTACHMENT usage with STORAGE or SAMPLED usages!", functionName);
            return WTF::nullopt;
        }

        return MTLTextureUsageRenderTarget | MTLTextureUsagePixelFormatView;
    }

    if (flags & GPUTextureUsage::Flags::Storage)
        return MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView;

    if (flags & GPUTextureUsage::Flags::Sampled)
        return MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView;

    return MTLTextureUsageUnknown;
}

static MTLStorageMode storageModeForPixelFormatAndSampleCount(MTLPixelFormat format, unsigned long samples)
{
    // Depth, Stencil, DepthStencil, and Multisample textures must be allocated with the MTLStorageModePrivate resource option.
    if (format == MTLPixelFormatDepth32Float_Stencil8 || samples > 1)
        return MTLStorageModePrivate;

#if PLATFORM(MAC)
    return MTLStorageModeManaged;
#else
    return MTLStorageModeShared;
#endif
}

static RetainPtr<MTLTextureDescriptor> tryCreateMtlTextureDescriptor(const char* const functionName, const GPUTextureDescriptor& descriptor, OptionSet<GPUTextureUsage::Flags> usage)
{
    RetainPtr<MTLTextureDescriptor> mtlDescriptor;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    mtlDescriptor = adoptNS([MTLTextureDescriptor new]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlDescriptor) {
        LOG(WebGPU, "%s: Unable to create new MTLTextureDescriptor!", functionName);
        return nullptr;
    }

    // FIXME: Add more validation as constraints are added to spec.
    auto pixelFormat = static_cast<MTLPixelFormat>(platformTextureFormatForGPUTextureFormat(descriptor.format));

    auto mtlUsage = mtlTextureUsageForGPUTextureUsageFlags(usage, functionName);
    if (!mtlUsage)
        return nullptr;

    auto storageMode = storageModeForPixelFormatAndSampleCount(pixelFormat, descriptor.sampleCount);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    [mtlDescriptor setWidth:descriptor.size.width];
    [mtlDescriptor setHeight:descriptor.size.height];
    [mtlDescriptor setDepth:descriptor.size.depth];
    [mtlDescriptor setArrayLength:descriptor.arrayLayerCount];
    [mtlDescriptor setMipmapLevelCount:descriptor.mipLevelCount];
    [mtlDescriptor setSampleCount:descriptor.sampleCount];
    [mtlDescriptor setTextureType:mtlTextureTypeForGPUTextureDescriptor(descriptor)];
    [mtlDescriptor setPixelFormat:pixelFormat];
    [mtlDescriptor setUsage:*mtlUsage];

    [mtlDescriptor setStorageMode:storageMode];

    END_BLOCK_OBJC_EXCEPTIONS;

    return mtlDescriptor;
}

RefPtr<GPUTexture> GPUTexture::tryCreate(const GPUDevice& device, const GPUTextureDescriptor& descriptor)
{
    const char* const functionName = "GPUTexture::tryCreate()";

    if (!device.platformDevice()) {
        LOG(WebGPU, "%s: Invalid GPUDevice!", functionName);
        return nullptr;
    }

    auto usage = OptionSet<GPUTextureUsage::Flags>::fromRaw(descriptor.usage);
    auto mtlDescriptor = tryCreateMtlTextureDescriptor(functionName, descriptor, usage);
    if (!mtlDescriptor)
        return nullptr;

    RetainPtr<MTLTexture> mtlTexture;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    mtlTexture = adoptNS([device.platformDevice() newTextureWithDescriptor:mtlDescriptor.get()]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlTexture) {
        LOG(WebGPU, "%s: Unable to create MTLTexture!", functionName);
        return nullptr;
    }

    return adoptRef(new GPUTexture(WTFMove(mtlTexture), usage));
}

Ref<GPUTexture> GPUTexture::create(PlatformTextureSmartPtr&& texture, OptionSet<GPUTextureUsage::Flags> usage)
{
    return adoptRef(*new GPUTexture(WTFMove(texture), usage));
}

GPUTexture::GPUTexture(PlatformTextureSmartPtr&& texture, OptionSet<GPUTextureUsage::Flags> usage)
    : m_platformTexture(WTFMove(texture))
    , m_usage(usage)
{
}

RefPtr<GPUTexture> GPUTexture::createDefaultTextureView()
{
    RetainPtr<MTLTexture> texture;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    texture = adoptNS([m_platformTexture newTextureViewWithPixelFormat:m_platformTexture.get().pixelFormat]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!texture) {
        LOG(WebGPU, "GPUTexture::createDefaultTextureView(): Unable to create MTLTexture view!");
        return nullptr;
    }

    return GPUTexture::create(WTFMove(texture), m_usage);
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
