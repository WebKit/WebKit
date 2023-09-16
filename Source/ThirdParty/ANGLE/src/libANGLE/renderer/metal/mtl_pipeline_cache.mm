//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_pipeline_cache.mm:
//    Defines classes for caching of mtl pipelines
//

#include "libANGLE/renderer/metal/mtl_pipeline_cache.h"

#include "libANGLE/renderer/metal/ContextMtl.h"

namespace rx
{
namespace mtl
{

namespace
{
bool HasDefaultAttribs(const RenderPipelineDesc &rpdesc)
{
    const VertexDesc &desc = rpdesc.vertexDescriptor;
    for (uint8_t i = 0; i < desc.numAttribs; ++i)
    {
        if (desc.attributes[i].bufferIndex == kDefaultAttribsBindingIndex)
        {
            return true;
        }
    }

    return false;
}

bool HasValidRenderTarget(const mtl::ContextDevice &device,
                          const MTLRenderPipelineDescriptor *descriptor)
{
    const NSUInteger maxColorRenderTargets = GetMaxNumberOfRenderTargetsForDevice(device);
    for (NSUInteger i = 0; i < maxColorRenderTargets; ++i)
    {
        auto colorAttachment = descriptor.colorAttachments[i];
        if (colorAttachment && colorAttachment.pixelFormat != MTLPixelFormatInvalid)
        {
            return true;
        }
    }

    if (descriptor.depthAttachmentPixelFormat != MTLPixelFormatInvalid)
    {
        return true;
    }

    if (descriptor.stencilAttachmentPixelFormat != MTLPixelFormatInvalid)
    {
        return true;
    }

    return false;
}

angle::Result ValidateRenderPipelineState(ContextMtl *context,
                                          const MTLRenderPipelineDescriptor *descriptor)
{
    const mtl::ContextDevice &device = context->getMetalDevice();

    if (!context->getDisplay()->getFeatures().allowRenderpassWithoutAttachment.enabled &&
        !HasValidRenderTarget(device, descriptor))
    {
        ANGLE_MTL_HANDLE_ERROR(
            context, "Render pipeline requires at least one render target for this device.",
            GL_INVALID_OPERATION);
        return angle::Result::Stop;
    }

    // Ensure the device can support the storage requirement for render targets.
    if (DeviceHasMaximumRenderTargetSize(device))
    {
        NSUInteger maxSize = GetMaxRenderTargetSizeForDeviceInBytes(device);
        NSUInteger renderTargetSize =
            ComputeTotalSizeUsedForMTLRenderPipelineDescriptor(descriptor, context, device);
        if (renderTargetSize > maxSize)
        {
            std::stringstream errorStream;
            errorStream << "This set of render targets requires " << renderTargetSize
                        << " bytes of pixel storage. This device supports " << maxSize << " bytes.";
            ANGLE_MTL_HANDLE_ERROR(context, errorStream.str().c_str(), GL_INVALID_OPERATION);
            return angle::Result::Stop;
        }
    }

    return angle::Result::Continue;
}

angle::Result CreateRenderPipelineState(ContextMtl *context,
                                        const PipelineKey &key,
                                        AutoObjCPtr<id<MTLRenderPipelineState>> *outRenderPipeline)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        ASSERT(key.isRenderPipeline());
        if (!key.vertexShader)
        {
            // Render pipeline without vertex shader is invalid.
            ANGLE_MTL_HANDLE_ERROR(context, "Render pipeline without vertex shader is invalid.",
                                   GL_INVALID_OPERATION);
            return angle::Result::Stop;
        }

        const mtl::ContextDevice &metalDevice = context->getMetalDevice();

        auto objCDesc = key.pipelineDesc.createMetalDesc(key.vertexShader, key.fragmentShader);

        ANGLE_TRY(ValidateRenderPipelineState(context, objCDesc));

        // Special attribute slot for default attribute
        if (HasDefaultAttribs(key.pipelineDesc))
        {
            auto defaultAttribLayoutObjCDesc = adoptObjCObj<MTLVertexBufferLayoutDescriptor>(
                [[MTLVertexBufferLayoutDescriptor alloc] init]);
            defaultAttribLayoutObjCDesc.get().stepFunction = MTLVertexStepFunctionConstant;
            defaultAttribLayoutObjCDesc.get().stepRate     = 0;
            defaultAttribLayoutObjCDesc.get().stride = kDefaultAttributeSize * kMaxVertexAttribs;

            [objCDesc.get().vertexDescriptor.layouts setObject:defaultAttribLayoutObjCDesc
                                            atIndexedSubscript:kDefaultAttribsBindingIndex];
        }
        // Create pipeline state
        NSError *err  = nil;
        auto newState = metalDevice.newRenderPipelineStateWithDescriptor(objCDesc, &err);
        if (err)
        {
            ANGLE_MTL_HANDLE_ERROR(context, mtl::FormatMetalErrorMessage(err).c_str(),
                                   GL_INVALID_OPERATION);
            return angle::Result::Stop;
        }

        *outRenderPipeline = newState;
        return angle::Result::Continue;
    }
}

angle::Result CreateComputePipelineState(
    ContextMtl *context,
    const PipelineKey &key,
    AutoObjCPtr<id<MTLComputePipelineState>> *outComputePipeline)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        ASSERT(!key.isRenderPipeline());
        if (!key.computeShader)
        {
            ANGLE_MTL_HANDLE_ERROR(context, "Compute pipeline without a shader is invalid.",
                                   GL_INVALID_OPERATION);
            return angle::Result::Stop;
        }

        const mtl::ContextDevice &metalDevice = context->getMetalDevice();

        // Create pipeline state
        NSError *err  = nil;
        auto newState = metalDevice.newComputePipelineStateWithFunction(key.computeShader, &err);
        if (err)
        {
            ANGLE_MTL_HANDLE_ERROR(context, mtl::FormatMetalErrorMessage(err).c_str(),
                                   GL_INVALID_OPERATION);
            return angle::Result::Stop;
        }

        *outComputePipeline = newState;
        return angle::Result::Continue;
    }
}

}  // namespace

bool PipelineKey::isRenderPipeline() const
{
    if (vertexShader)
    {
        ASSERT(!computeShader);
        return true;
    }
    else
    {
        ASSERT(computeShader);
        return false;
    }
}

bool PipelineKey::operator==(const PipelineKey &rhs) const
{
    if (isRenderPipeline() != rhs.isRenderPipeline())
    {
        return false;
    }

    if (isRenderPipeline())
    {
        return std::tie(vertexShader, fragmentShader, pipelineDesc) ==
               std::tie(rhs.vertexShader, rhs.fragmentShader, rhs.pipelineDesc);
    }
    else
    {
        return computeShader == rhs.computeShader;
    }
}

size_t PipelineKey::hash() const
{
    if (isRenderPipeline())
    {
        return angle::HashMultiple(vertexShader.get(), fragmentShader.get(), pipelineDesc);
    }
    else
    {
        return angle::HashMultiple(computeShader.get());
    }
}

PipelineCache::PipelineCache() : mPipelineCache(kMaxPipelines) {}

angle::Result PipelineCache::getRenderPipeline(
    ContextMtl *context,
    id<MTLFunction> vertexShader,
    id<MTLFunction> fragmentShader,
    const RenderPipelineDesc &desc,
    AutoObjCPtr<id<MTLRenderPipelineState>> *outRenderPipeline)
{
    PipelineKey key;
    key.vertexShader.retainAssign(vertexShader);
    key.fragmentShader.retainAssign(fragmentShader);
    key.pipelineDesc = desc;

    auto iter = mPipelineCache.Get(key);
    if (iter != mPipelineCache.end())
    {
        // Should be no way that this key matched a compute pipeline entry
        ASSERT(iter->second.renderPipeline);
        *outRenderPipeline = iter->second.renderPipeline;
        return angle::Result::Continue;
    }

    angle::TrimCache(kMaxPipelines, kGCLimit, "render pipeline", &mPipelineCache);

    PipelineVariant newPipeline;
    ANGLE_TRY(CreateRenderPipelineState(context, key, &newPipeline.renderPipeline));

    iter = mPipelineCache.Put(std::move(key), std::move(newPipeline));

    *outRenderPipeline = iter->second.renderPipeline;
    return angle::Result::Continue;
}

angle::Result PipelineCache::getComputePipeline(
    ContextMtl *context,
    id<MTLFunction> computeShader,
    AutoObjCPtr<id<MTLComputePipelineState>> *outComputePipeline)
{
    PipelineKey key;
    key.computeShader.retainAssign(computeShader);

    auto iter = mPipelineCache.Get(key);
    if (iter != mPipelineCache.end())
    {
        // Should be no way that this key matched a render pipeline entry
        ASSERT(iter->second.computePipeline);
        *outComputePipeline = iter->second.computePipeline;
        return angle::Result::Continue;
    }

    angle::TrimCache(kMaxPipelines, kGCLimit, "render pipeline", &mPipelineCache);

    PipelineVariant newPipeline;
    ANGLE_TRY(CreateComputePipelineState(context, key, &newPipeline.computePipeline));

    iter = mPipelineCache.Put(std::move(key), std::move(newPipeline));

    *outComputePipeline = iter->second.computePipeline;
    return angle::Result::Continue;
}

}  // namespace mtl
}  // namespace rx
