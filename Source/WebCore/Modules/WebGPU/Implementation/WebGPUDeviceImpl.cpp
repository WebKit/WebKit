/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebGPUDeviceImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUBindGroupDescriptor.h"
#include "WebGPUBindGroupImpl.h"
#include "WebGPUBindGroupLayoutDescriptor.h"
#include "WebGPUBindGroupLayoutImpl.h"
#include "WebGPUBufferDescriptor.h"
#include "WebGPUBufferImpl.h"
#include "WebGPUCommandEncoderDescriptor.h"
#include "WebGPUCommandEncoderImpl.h"
#include "WebGPUComputePipelineDescriptor.h"
#include "WebGPUComputePipelineImpl.h"
#include "WebGPUConvertToBackingContext.h"
#include "WebGPUExtent3D.h"
#include "WebGPUExternalTextureDescriptor.h"
#include "WebGPUExternalTextureImpl.h"
#include "WebGPUOutOfMemoryError.h"
#include "WebGPUPipelineLayoutDescriptor.h"
#include "WebGPUPipelineLayoutImpl.h"
#include "WebGPUPresentationContextImpl.h"
#include "WebGPUQuerySetDescriptor.h"
#include "WebGPUQuerySetImpl.h"
#include "WebGPURenderBundleEncoderDescriptor.h"
#include "WebGPURenderBundleEncoderImpl.h"
#include "WebGPURenderPipelineDescriptor.h"
#include "WebGPURenderPipelineImpl.h"
#include "WebGPUSamplerDescriptor.h"
#include "WebGPUSamplerImpl.h"
#include "WebGPUShaderModuleDescriptor.h"
#include "WebGPUShaderModuleImpl.h"
#include "WebGPUTextureDescriptor.h"
#include "WebGPUTextureImpl.h"
#include "WebGPUTextureViewImpl.h"
#include "WebGPUValidationError.h"
#include <CoreGraphics/CGColorSpace.h>
#include <WebGPU/WebGPUExt.h>
#include <wtf/BlockPtr.h>
#include <wtf/SegmentedVector.h>

namespace WebCore::WebGPU {

DeviceImpl::DeviceImpl(WebGPUPtr<WGPUDevice>&& device, Ref<SupportedFeatures>&& features, Ref<SupportedLimits>&& limits, ConvertToBackingContext& convertToBackingContext)
    : Device(WTFMove(features), WTFMove(limits))
    , m_backing(device.copyRef())
    , m_convertToBackingContext(convertToBackingContext)
    , m_queue(QueueImpl::create(WebGPUPtr<WGPUQueue> { wgpuDeviceGetQueue(device.get()) }, convertToBackingContext))
{
}

DeviceImpl::~DeviceImpl() = default;

Ref<Queue> DeviceImpl::queue()
{
    return m_queue;
}

void DeviceImpl::destroy()
{
    wgpuDeviceDestroy(m_backing.get());
}

Ref<Buffer> DeviceImpl::createBuffer(const BufferDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    WGPUBufferDescriptor backingDescriptor {
        nullptr,
        label.data(),
        m_convertToBackingContext->convertBufferUsageFlagsToBacking(descriptor.usage),
        descriptor.size,
        descriptor.mappedAtCreation,
    };

    return BufferImpl::create(adoptWebGPU(wgpuDeviceCreateBuffer(m_backing.get(), &backingDescriptor)), m_convertToBackingContext);
}

Ref<Texture> DeviceImpl::createTexture(const TextureDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    auto backingTextureFormats = descriptor.viewFormats.map([&convertToBackingContext = m_convertToBackingContext.get()](TextureFormat textureFormat) {
        return convertToBackingContext.convertToBacking(textureFormat);
    });

    WGPUTextureDescriptor backingDescriptor {
        nullptr,
        label.data(),
        m_convertToBackingContext->convertTextureUsageFlagsToBacking(descriptor.usage),
        m_convertToBackingContext->convertToBacking(descriptor.dimension),
        m_convertToBackingContext->convertToBacking(descriptor.size),
        m_convertToBackingContext->convertToBacking(descriptor.format),
        descriptor.mipLevelCount,
        descriptor.sampleCount,
        static_cast<uint32_t>(backingTextureFormats.size()),
        backingTextureFormats.data(),
    };

    return TextureImpl::create(adoptWebGPU(wgpuDeviceCreateTexture(m_backing.get(), &backingDescriptor)), descriptor.format, descriptor.dimension, m_convertToBackingContext);
}

Ref<Sampler> DeviceImpl::createSampler(const SamplerDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    WGPUSamplerDescriptor backingDescriptor {
        nullptr,
        label.data(),
        m_convertToBackingContext->convertToBacking(descriptor.addressModeU),
        m_convertToBackingContext->convertToBacking(descriptor.addressModeV),
        m_convertToBackingContext->convertToBacking(descriptor.addressModeW),
        m_convertToBackingContext->convertToBacking(descriptor.magFilter),
        m_convertToBackingContext->convertToBacking(descriptor.minFilter),
        m_convertToBackingContext->convertToBacking(descriptor.mipmapFilter),
        descriptor.lodMinClamp,
        descriptor.lodMaxClamp,
        descriptor.compare ? m_convertToBackingContext->convertToBacking(*descriptor.compare) : WGPUCompareFunction_Always,
        descriptor.maxAnisotropy,
    };

    return SamplerImpl::create(adoptWebGPU(wgpuDeviceCreateSampler(m_backing.get(), &backingDescriptor)), m_convertToBackingContext);
}

static WGPUColorSpace convertToWGPUColorSpace(const PredefinedColorSpace& colorSpace)
{
    switch (colorSpace) {
    case PredefinedColorSpace::SRGB:
        return WGPUColorSpace::SRGB;
    case PredefinedColorSpace::DisplayP3:
        return WGPUColorSpace::DisplayP3;
    }
}

Ref<ExternalTexture> DeviceImpl::importExternalTexture(const ExternalTextureDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    WGPUExternalTextureDescriptor backingDescriptor {
        .nextInChain = nullptr,
        label.data(),
        .pixelBuffer = descriptor.pixelBuffer.get(),
        .colorSpace = convertToWGPUColorSpace(descriptor.colorSpace),
    };
    return ExternalTextureImpl::create(adoptWebGPU(wgpuDeviceImportExternalTexture(m_backing.get(), &backingDescriptor)), descriptor, m_convertToBackingContext);
}

Ref<BindGroupLayout> DeviceImpl::createBindGroupLayout(const BindGroupLayoutDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    auto backingExternalTextureEntries = descriptor.entries.map([](const auto&) {
        return WGPUExternalTextureBindGroupLayoutEntry {
            {
                nullptr,
                static_cast<WGPUSType>(WGPUSTypeExtended_BindGroupLayoutEntryExternalTexture),
            }, {
                nullptr,
            },
        };
    });

    Vector<WGPUBindGroupLayoutEntry> backingEntries;
    backingEntries.reserveInitialCapacity(descriptor.entries.size());
    for (size_t i = 0; i < descriptor.entries.size(); ++i) {
        const auto& entry = descriptor.entries[i];
        backingEntries.uncheckedAppend({
            entry.externalTexture ? &backingExternalTextureEntries[i].chain : nullptr,
            entry.binding,
            m_convertToBackingContext->convertShaderStageFlagsToBacking(entry.visibility), {
                nullptr,
                entry.buffer ? m_convertToBackingContext->convertToBacking(entry.buffer->type) : WGPUBufferBindingType_Undefined,
                entry.buffer ? entry.buffer->hasDynamicOffset : false,
                entry.buffer ? entry.buffer->minBindingSize : 0,
            }, {
                nullptr,
                entry.sampler ? m_convertToBackingContext->convertToBacking(entry.sampler->type) : WGPUSamplerBindingType_Undefined,
            }, {
                nullptr,
                entry.texture ? m_convertToBackingContext->convertToBacking(entry.texture->sampleType) : WGPUTextureSampleType_Undefined,
                entry.texture ? m_convertToBackingContext->convertToBacking(entry.texture->viewDimension) : WGPUTextureViewDimension_Undefined,
                entry.texture ? entry.texture->multisampled : false,
            }, {
                nullptr,
                entry.storageTexture ? m_convertToBackingContext->convertToBacking(entry.storageTexture->access) : WGPUStorageTextureAccess_Undefined,
                entry.storageTexture ? m_convertToBackingContext->convertToBacking(entry.storageTexture->format) : WGPUTextureFormat_Undefined,
                entry.storageTexture ? m_convertToBackingContext->convertToBacking(entry.storageTexture->viewDimension) : WGPUTextureViewDimension_Undefined,
            },
        });
    }

    WGPUBindGroupLayoutDescriptor backingDescriptor {
        nullptr,
        label.data(),
        static_cast<uint32_t>(backingEntries.size()),
        backingEntries.data(),
    };

    return BindGroupLayoutImpl::create(adoptWebGPU(wgpuDeviceCreateBindGroupLayout(m_backing.get(), &backingDescriptor)), m_convertToBackingContext);
}

Ref<PipelineLayout> DeviceImpl::createPipelineLayout(const PipelineLayoutDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    Vector<WGPUBindGroupLayout> backingBindGroupLayouts;
    if (descriptor.bindGroupLayouts) {
        backingBindGroupLayouts = descriptor.bindGroupLayouts->map([&convertToBackingContext = m_convertToBackingContext.get()](auto bindGroupLayout) {
            return convertToBackingContext.convertToBacking(bindGroupLayout.get());
        });
    }

    WGPUPipelineLayoutDescriptor backingDescriptor {
        nullptr,
        label.data(),
        descriptor.bindGroupLayouts ? static_cast<uint32_t>(backingBindGroupLayouts.size()) : 0,
        descriptor.bindGroupLayouts ? backingBindGroupLayouts.data() : nullptr,
    };

    return PipelineLayoutImpl::create(adoptWebGPU(wgpuDeviceCreatePipelineLayout(m_backing.get(), &backingDescriptor)), m_convertToBackingContext);
}

Ref<BindGroup> DeviceImpl::createBindGroup(const BindGroupDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    SegmentedVector<WGPUBindGroupExternalTextureEntry, 1> chainedEntries;
    auto backingEntries = descriptor.entries.map([&convertToBackingContext = m_convertToBackingContext.get(), &chainedEntries](const auto& bindGroupEntry) {
        auto externalTexture = std::holds_alternative<std::reference_wrapper<ExternalTexture>>(bindGroupEntry.resource) ? convertToBackingContext.convertToBacking(std::get<std::reference_wrapper<ExternalTexture>>(bindGroupEntry.resource).get()) : nullptr;
        chainedEntries.append(WGPUBindGroupExternalTextureEntry {
            {
                .next = nullptr,
                .sType = static_cast<WGPUSType>(WGPUSTypeExtended_BindGroupEntryExternalTexture)
            },
            .externalTexture = externalTexture,
        });
        return WGPUBindGroupEntry {
            externalTexture ? reinterpret_cast<WGPUChainedStruct*>(&chainedEntries.last()) : nullptr,
            bindGroupEntry.binding,
            std::holds_alternative<BufferBinding>(bindGroupEntry.resource) ? convertToBackingContext.convertToBacking(std::get<BufferBinding>(bindGroupEntry.resource).buffer) : nullptr,
            std::holds_alternative<BufferBinding>(bindGroupEntry.resource) ? std::get<BufferBinding>(bindGroupEntry.resource).offset : 0,
            std::holds_alternative<BufferBinding>(bindGroupEntry.resource) ? std::get<BufferBinding>(bindGroupEntry.resource).size.value_or(WGPU_WHOLE_SIZE) : 0,
            std::holds_alternative<std::reference_wrapper<Sampler>>(bindGroupEntry.resource) ? convertToBackingContext.convertToBacking(std::get<std::reference_wrapper<Sampler>>(bindGroupEntry.resource).get()) : nullptr,
            std::holds_alternative<std::reference_wrapper<TextureView>>(bindGroupEntry.resource) ? convertToBackingContext.convertToBacking(std::get<std::reference_wrapper<TextureView>>(bindGroupEntry.resource).get()) : nullptr,
        };
    });

    WGPUBindGroupDescriptor backingDescriptor {
        nullptr,
        label.data(),
        m_convertToBackingContext->convertToBacking(descriptor.layout),
        static_cast<uint32_t>(backingEntries.size()),
        backingEntries.data(),
    };

    return BindGroupImpl::create(adoptWebGPU(wgpuDeviceCreateBindGroup(m_backing.get(), &backingDescriptor)), m_convertToBackingContext);
}

Ref<ShaderModule> DeviceImpl::createShaderModule(const ShaderModuleDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    auto source = descriptor.code.utf8();

    auto entryPoints = descriptor.hints.map([](const auto& hint) {
        return hint.key.utf8();
    });

    Vector<WGPUShaderModuleCompilationHint> hintsEntries;
    hintsEntries.reserveInitialCapacity(descriptor.hints.size());
    for (size_t i = 0; i < descriptor.hints.size(); ++i) {
        const auto& hint = descriptor.hints[i].value;
        hintsEntries.append(WGPUShaderModuleCompilationHint {
            nullptr,
            entryPoints[i].data(),
            m_convertToBackingContext->convertToBacking(hint.pipelineLayout)
        });
    }

    WGPUShaderModuleWGSLDescriptor backingWGSLDescriptor {
        {
            nullptr,
            WGPUSType_ShaderModuleWGSLDescriptor,
        },
        source.data(),
    };

    WGPUShaderModuleDescriptor backingDescriptor {
        &backingWGSLDescriptor.chain,
        label.data(),
        static_cast<uint32_t>(hintsEntries.size()),
        hintsEntries.size() ? &hintsEntries[0] : nullptr,
    };

    return ShaderModuleImpl::create(adoptWebGPU(wgpuDeviceCreateShaderModule(m_backing.get(), &backingDescriptor)), m_convertToBackingContext);
}

template <typename T>
static auto convertToBacking(const ComputePipelineDescriptor& descriptor, ConvertToBackingContext& convertToBackingContext, T&& callback)
{
    auto label = descriptor.label.utf8();

    auto entryPoint = descriptor.compute.entryPoint.utf8();

    auto constantNames = descriptor.compute.constants.map([](const auto& constant) {
        return constant.key.utf8();
    });

    Vector<WGPUConstantEntry> backingConstantEntries;
    backingConstantEntries.reserveInitialCapacity(descriptor.compute.constants.size());
    for (size_t i = 0; i < descriptor.compute.constants.size(); ++i) {
        const auto& constant = descriptor.compute.constants[i];
        backingConstantEntries.uncheckedAppend(WGPUConstantEntry {
            nullptr,
            constantNames[i].data(),
            constant.value
        });
    }

    WGPUComputePipelineDescriptor backingDescriptor {
        nullptr,
        label.data(),
        descriptor.layout ? convertToBackingContext.convertToBacking(*descriptor.layout) : nullptr, {
            nullptr,
            convertToBackingContext.convertToBacking(descriptor.compute.module),
            entryPoint.data(),
            static_cast<uint32_t>(backingConstantEntries.size()),
            backingConstantEntries.data(),
        }
    };

    return callback(backingDescriptor);
}

Ref<ComputePipeline> DeviceImpl::createComputePipeline(const ComputePipelineDescriptor& descriptor)
{
    return convertToBacking(descriptor, m_convertToBackingContext, [backing = m_backing, &convertToBackingContext = m_convertToBackingContext.get()](const WGPUComputePipelineDescriptor& backingDescriptor) {
        return ComputePipelineImpl::create(adoptWebGPU(wgpuDeviceCreateComputePipeline(backing.get(), &backingDescriptor)), convertToBackingContext);
    });
}

template <typename T>
static auto convertToBacking(const RenderPipelineDescriptor& descriptor, bool depthClipControlIsEnabled, ConvertToBackingContext& convertToBackingContext, T&& callback)
{
    auto label = descriptor.label.utf8();

    auto vertexEntryPoint = descriptor.vertex.entryPoint.utf8();

    auto vertexConstantNames = descriptor.vertex.constants.map([](const auto& constant) {
        return constant.key.utf8();
    });

    Vector<WGPUConstantEntry> vertexConstantEntries;
    vertexConstantEntries.reserveInitialCapacity(descriptor.vertex.constants.size());
    for (size_t i = 0; i < descriptor.vertex.constants.size(); ++i) {
        const auto& constant = descriptor.vertex.constants[i];
        vertexConstantEntries.uncheckedAppend(WGPUConstantEntry {
            nullptr,
            vertexConstantNames[i].data(),
            constant.value,
        });
    }

    auto backingAttributes = descriptor.vertex.buffers.map([&convertToBackingContext](const auto& buffer) -> Vector<WGPUVertexAttribute> {
        if (buffer) {
            return buffer->attributes.map([&convertToBackingContext](const auto& attribute) {
                return WGPUVertexAttribute {
                    convertToBackingContext.convertToBacking(attribute.format),
                    attribute.offset,
                    attribute.shaderLocation,
                };
            });
        } else
            return { };
    });

    Vector<WGPUVertexBufferLayout> backingBuffers;
    backingBuffers.reserveInitialCapacity(descriptor.vertex.buffers.size());
    for (size_t i = 0; i < descriptor.vertex.buffers.size(); ++i) {
        const auto& buffer = descriptor.vertex.buffers[i];
        backingBuffers.uncheckedAppend(WGPUVertexBufferLayout {
            buffer ? buffer->arrayStride : WGPU_COPY_STRIDE_UNDEFINED,
            buffer ? convertToBackingContext.convertToBacking(buffer->stepMode) : WGPUVertexStepMode_Vertex,
            static_cast<uint32_t>(backingAttributes[i].size()),
            backingAttributes[i].data(),
        });
    }

    WGPUDepthStencilState depthStencilState {
        nullptr,
        descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->format) : WGPUTextureFormat_Undefined,
        descriptor.depthStencil ? descriptor.depthStencil->depthWriteEnabled : false,
        descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->depthCompare) : WGPUCompareFunction_Undefined, {
            descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilFront.compare) : WGPUCompareFunction_Undefined,
            descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilFront.failOp) : WGPUStencilOperation_Keep,
            descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilFront.depthFailOp) : WGPUStencilOperation_Keep,
            descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilFront.passOp) : WGPUStencilOperation_Keep,
        }, {
            descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilBack.compare) : WGPUCompareFunction_Undefined,
            descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilBack.failOp) : WGPUStencilOperation_Keep,
            descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilBack.depthFailOp) : WGPUStencilOperation_Keep,
            descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilBack.passOp) : WGPUStencilOperation_Keep,
        },
        descriptor.depthStencil && descriptor.depthStencil->stencilReadMask ? *descriptor.depthStencil->stencilReadMask : 0,
        descriptor.depthStencil && descriptor.depthStencil->stencilWriteMask ? *descriptor.depthStencil->stencilWriteMask : 0,
        descriptor.depthStencil ? descriptor.depthStencil->depthBias : 0,
        descriptor.depthStencil ? descriptor.depthStencil->depthBiasSlopeScale : 0,
        descriptor.depthStencil ? descriptor.depthStencil->depthBiasClamp : 0,
    };

    auto fragmentEntryPoint = descriptor.fragment ? descriptor.fragment->entryPoint.utf8() : CString("");

    Vector<CString> fragmentConstantNames;
    if (descriptor.fragment) {
        fragmentConstantNames = descriptor.fragment->constants.map([](const auto& constant) {
            return constant.key.utf8();
        });
    }

    Vector<WGPUConstantEntry> fragmentConstantEntries;
    if (descriptor.fragment) {
        fragmentConstantEntries.reserveInitialCapacity(descriptor.fragment->constants.size());
        for (size_t i = 0; i < descriptor.fragment->constants.size(); ++i) {
            const auto& constant = descriptor.fragment->constants[i];
            fragmentConstantEntries.uncheckedAppend(WGPUConstantEntry {
                nullptr,
                fragmentConstantNames[i].data(),
                constant.value,
            });
        }
    }

    Vector<std::optional<WGPUBlendState>> blendStates;
    if (descriptor.fragment) {
        blendStates = descriptor.fragment->targets.map([&convertToBackingContext](const auto& target) -> std::optional<WGPUBlendState> {
            if (target && target->blend) {
                return WGPUBlendState {
                    {
                        convertToBackingContext.convertToBacking(target->blend->color.operation),
                        convertToBackingContext.convertToBacking(target->blend->color.srcFactor),
                        convertToBackingContext.convertToBacking(target->blend->color.dstFactor),
                    }, {
                        convertToBackingContext.convertToBacking(target->blend->alpha.operation),
                        convertToBackingContext.convertToBacking(target->blend->alpha.srcFactor),
                        convertToBackingContext.convertToBacking(target->blend->alpha.dstFactor),
                    }
                };
            } else
                return std::nullopt;
        });
    }

    Vector<WGPUColorTargetState> colorTargets;
    if (descriptor.fragment) {
        colorTargets.reserveInitialCapacity(descriptor.fragment->targets.size());
        for (size_t i = 0; i < descriptor.fragment->targets.size(); ++i) {
            if (const auto& target = descriptor.fragment->targets[i]) {
                colorTargets.uncheckedAppend(WGPUColorTargetState {
                    nullptr,
                    convertToBackingContext.convertToBacking(target->format),
                    blendStates[i] ? &*blendStates[i] : nullptr,
                    convertToBackingContext.convertColorWriteFlagsToBacking(target->writeMask),
                });
            } else {
                colorTargets.uncheckedAppend(WGPUColorTargetState {
                    nullptr,
                    WGPUTextureFormat_Undefined,
                    nullptr,
                    WGPUMapMode_None,
                });
            }
        }
    }

    WGPUFragmentState fragmentState {
        nullptr,
        descriptor.fragment ? convertToBackingContext.convertToBacking(descriptor.fragment->module) : nullptr,
        fragmentEntryPoint.data(),
        static_cast<uint32_t>(fragmentConstantEntries.size()),
        fragmentConstantEntries.data(),
        static_cast<uint32_t>(colorTargets.size()),
        colorTargets.data(),
    };

    WGPUPrimitiveDepthClipControl depthClipControl = {
        .chain = {
            nullptr,
            WGPUSType_PrimitiveDepthClipControl,
        },
        .unclippedDepth = descriptor.primitive && descriptor.primitive->unclippedDepth,
    };

    WGPURenderPipelineDescriptor backingDescriptor {
        nullptr,
        label.data(),
        descriptor.layout ? convertToBackingContext.convertToBacking(*descriptor.layout) : nullptr, {
            nullptr,
            convertToBackingContext.convertToBacking(descriptor.vertex.module),
            vertexEntryPoint.data(),
            static_cast<uint32_t>(vertexConstantEntries.size()),
            vertexConstantEntries.data(),
            static_cast<uint32_t>(backingBuffers.size()),
            backingBuffers.data(),
        }, {
            descriptor.primitive && depthClipControlIsEnabled ? &depthClipControl.chain : nullptr,
            descriptor.primitive ? convertToBackingContext.convertToBacking(descriptor.primitive->topology) : WGPUPrimitiveTopology_TriangleList,
            descriptor.primitive && descriptor.primitive->stripIndexFormat ? convertToBackingContext.convertToBacking(*descriptor.primitive->stripIndexFormat) : WGPUIndexFormat_Undefined,
            descriptor.primitive ? convertToBackingContext.convertToBacking(descriptor.primitive->frontFace) : WGPUFrontFace_CCW,
            descriptor.primitive ? convertToBackingContext.convertToBacking(descriptor.primitive->cullMode) : WGPUCullMode_None,
        },
        descriptor.depthStencil ? &depthStencilState : nullptr, {
            nullptr,
            descriptor.multisample ? descriptor.multisample->count : 1,
            descriptor.multisample ? descriptor.multisample->mask : 0xFFFFFFFFU,
            descriptor.multisample ? descriptor.multisample->alphaToCoverageEnabled : false,
        },
        descriptor.fragment ? &fragmentState : nullptr,
    };

    return callback(backingDescriptor);
}

Ref<RenderPipeline> DeviceImpl::createRenderPipeline(const RenderPipelineDescriptor& descriptor)
{
    bool depthClipControlIsEnabled = wgpuDeviceHasFeature(m_backing.get(), WGPUFeatureName_DepthClipControl);
    return convertToBacking(descriptor, depthClipControlIsEnabled, m_convertToBackingContext, [backing = m_backing.copyRef(), &convertToBackingContext = m_convertToBackingContext.get()](const WGPURenderPipelineDescriptor& backingDescriptor) {
        return RenderPipelineImpl::create(adoptWebGPU(wgpuDeviceCreateRenderPipeline(backing.get(), &backingDescriptor)), convertToBackingContext);
    });
}

static void createComputePipelineAsyncCallback(WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, const char* message, void* userdata)
{
    auto block = reinterpret_cast<void(^)(WGPUCreatePipelineAsyncStatus, WGPUComputePipeline, const char*)>(userdata);
    block(status, pipeline, message);
    Block_release(block); // Block_release is matched with Block_copy below in DeviceImpl::createComputePipelineAsync().
}

void DeviceImpl::createComputePipelineAsync(const ComputePipelineDescriptor& descriptor, CompletionHandler<void(RefPtr<ComputePipeline>&&)>&& callback)
{
    convertToBacking(descriptor, m_convertToBackingContext, [backing = m_backing.copyRef(), &convertToBackingContext = m_convertToBackingContext.get(), callback = WTFMove(callback)](const WGPUComputePipelineDescriptor& backingDescriptor) mutable {
        auto blockPtr = makeBlockPtr([convertToBackingContext = Ref { convertToBackingContext }, callback = WTFMove(callback)](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, const char*) mutable {
            if (status == WGPUCreatePipelineAsyncStatus_Success)
                callback(ComputePipelineImpl::create(adoptWebGPU(pipeline), convertToBackingContext));
            else
                callback(nullptr);
        });
        wgpuDeviceCreateComputePipelineAsync(backing.get(), &backingDescriptor, &createComputePipelineAsyncCallback, Block_copy(blockPtr.get())); // Block_copy is matched with Block_release above in createComputePipelineAsyncCallback().
    });
}

static void createRenderPipelineAsyncCallback(WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, const char* message, void* userdata)
{
    auto block = reinterpret_cast<void(^)(WGPUCreatePipelineAsyncStatus, WGPURenderPipeline, const char*)>(userdata);
    block(status, pipeline, message);
    Block_release(block); // Block_release is matched with Block_copy below in DeviceImpl::createRenderPipelineAsync().
}

void DeviceImpl::createRenderPipelineAsync(const RenderPipelineDescriptor& descriptor, CompletionHandler<void(RefPtr<RenderPipeline>&&)>&& callback)
{
    bool depthClipControlIsEnabled = wgpuDeviceHasFeature(m_backing.get(), WGPUFeatureName_DepthClipControl);
    convertToBacking(descriptor, depthClipControlIsEnabled, m_convertToBackingContext, [backing = m_backing.copyRef(), convertToBackingContext = m_convertToBackingContext.copyRef(), callback = WTFMove(callback)](const WGPURenderPipelineDescriptor& backingDescriptor) mutable {
        auto blockPtr = makeBlockPtr([convertToBackingContext = convertToBackingContext.copyRef(), callback = WTFMove(callback)](WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, const char*) mutable {
            if (status == WGPUCreatePipelineAsyncStatus_Success)
                callback(RenderPipelineImpl::create(adoptWebGPU(pipeline), convertToBackingContext));
            else
                callback(nullptr);
        });
        wgpuDeviceCreateRenderPipelineAsync(backing.get(), &backingDescriptor, &createRenderPipelineAsyncCallback, Block_copy(blockPtr.get())); // Block_copy is matched with Block_release above in createRenderPipelineAsyncCallback().
    });
}

Ref<CommandEncoder> DeviceImpl::createCommandEncoder(const std::optional<CommandEncoderDescriptor>& descriptor)
{
    CString label = descriptor ? descriptor->label.utf8() : CString("");

    WGPUCommandEncoderDescriptor backingDescriptor {
        nullptr,
        label.data(),
    };

    return CommandEncoderImpl::create(adoptWebGPU(wgpuDeviceCreateCommandEncoder(m_backing.get(), &backingDescriptor)), m_convertToBackingContext);
}

Ref<RenderBundleEncoder> DeviceImpl::createRenderBundleEncoder(const RenderBundleEncoderDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    auto backingColorFormats = descriptor.colorFormats.map([&convertToBackingContext = m_convertToBackingContext.get()](auto colorFormat) {
        return colorFormat ? convertToBackingContext.convertToBacking(*colorFormat) : WGPUTextureFormat_Undefined;
    });

    WGPURenderBundleEncoderDescriptor backingDescriptor {
        nullptr,
        label.data(),
        static_cast<uint32_t>(backingColorFormats.size()),
        backingColorFormats.data(),
        descriptor.depthStencilFormat ? m_convertToBackingContext->convertToBacking(*descriptor.depthStencilFormat) : WGPUTextureFormat_Undefined,
        descriptor.sampleCount,
        descriptor.depthReadOnly,
        descriptor.stencilReadOnly,
    };

    return RenderBundleEncoderImpl::create(adoptWebGPU(wgpuDeviceCreateRenderBundleEncoder(m_backing.get(), &backingDescriptor)), m_convertToBackingContext);
}

Ref<QuerySet> DeviceImpl::createQuerySet(const QuerySetDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    WGPUQuerySetDescriptor backingDescriptor {
        nullptr,
        label.data(),
        m_convertToBackingContext->convertToBacking(descriptor.type),
        descriptor.count,
        nullptr,
        0,
    };

    return QuerySetImpl::create(adoptWebGPU(wgpuDeviceCreateQuerySet(m_backing.get(), &backingDescriptor)), m_convertToBackingContext);
}

void DeviceImpl::pushErrorScope(ErrorFilter errorFilter)
{
    wgpuDevicePushErrorScope(m_backing.get(), m_convertToBackingContext->convertToBacking(errorFilter));
}

static void popErrorScopeCallback(WGPUErrorType type, const char* message, void* userdata)
{
    auto block = reinterpret_cast<void(^)(WGPUErrorType, const char*)>(userdata);
    block(type, message);
    Block_release(block); // Block_release is matched with Block_copy below in DeviceImpl::popErrorScope().
}

void DeviceImpl::popErrorScope(CompletionHandler<void(std::optional<Error>&&)>&& callback)
{
    auto blockPtr = makeBlockPtr([callback = WTFMove(callback)](WGPUErrorType errorType, const char* message) mutable {
        std::optional<Error> error;
        switch (errorType) {
        case WGPUErrorType_NoError:
        case WGPUErrorType_Force32:
            break;
        case WGPUErrorType_Internal:
        case WGPUErrorType_Validation:
            error = { { ValidationError::create(String::fromLatin1(message)) } };
            break;
        case WGPUErrorType_OutOfMemory:
            error = { { OutOfMemoryError::create() } };
            break;
        case WGPUErrorType_Unknown:
            error = { { OutOfMemoryError::create() } };
            break;
        case WGPUErrorType_DeviceLost:
            error = { { OutOfMemoryError::create() } };
            break;
        }

        callback(WTFMove(error));
    });
    wgpuDevicePopErrorScope(m_backing.get(), &popErrorScopeCallback, Block_copy(blockPtr.get())); // Block_copy is matched with Block_release above in popErrorScopeCallback().
}

void DeviceImpl::setLabelInternal(const String& label)
{
    wgpuDeviceSetLabel(m_backing.get(), label.utf8().data());
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
