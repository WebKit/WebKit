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
#include "WebGPUInternalError.h"
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
#include "WebGPUXRBindingImpl.h"
#include <CoreGraphics/CGColorSpace.h>
#include <WebGPU/WebGPUExt.h>
#include <wtf/BlockPtr.h>
#include <wtf/SegmentedVector.h>

namespace WebCore::WebGPU {

static auto invalidEntryPointName()
{
    return CString("");
}

DeviceImpl::DeviceImpl(WebGPUPtr<WGPUDevice>&& device, Ref<SupportedFeatures>&& features, Ref<SupportedLimits>&& limits, ConvertToBackingContext& convertToBackingContext)
    : Device(WTFMove(features), WTFMove(limits))
    , m_backing(device.copyRef())
    , m_convertToBackingContext(convertToBackingContext)
    , m_queue(QueueImpl::create(WebGPUPtr<WGPUQueue> { wgpuDeviceGetQueue(device.get()) }, convertToBackingContext))
{
}

DeviceImpl::~DeviceImpl()
{
    wgpuDeviceSetUncapturedErrorCallback(m_backing.get(), nullptr, nullptr);
}

Ref<Queue> DeviceImpl::queue()
{
    return m_queue;
}

void DeviceImpl::destroy()
{
    wgpuDeviceDestroy(m_backing.get());
}

RefPtr<XRBinding> DeviceImpl::createXRBinding()
{
    return XRBindingImpl::create(adoptWebGPU(wgpuDeviceCreateXRBinding(m_backing.get())), m_convertToBackingContext);
}

RefPtr<Buffer> DeviceImpl::createBuffer(const BufferDescriptor& descriptor)
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

RefPtr<Texture> DeviceImpl::createTexture(const TextureDescriptor& descriptor)
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

RefPtr<Sampler> DeviceImpl::createSampler(const SamplerDescriptor& descriptor)
{
    WGPUSamplerDescriptor backingDescriptor {
        .nextInChain = nullptr,
        .label = descriptor.label,
        .addressModeU = m_convertToBackingContext->convertToBacking(descriptor.addressModeU),
        .addressModeV = m_convertToBackingContext->convertToBacking(descriptor.addressModeV),
        .addressModeW = m_convertToBackingContext->convertToBacking(descriptor.addressModeW),
        .magFilter = m_convertToBackingContext->convertToBacking(descriptor.magFilter),
        .minFilter = m_convertToBackingContext->convertToBacking(descriptor.minFilter),
        .mipmapFilter = m_convertToBackingContext->convertToBacking(descriptor.mipmapFilter),
        .lodMinClamp = descriptor.lodMinClamp,
        .lodMaxClamp = descriptor.lodMaxClamp,
        .compare = descriptor.compare ? m_convertToBackingContext->convertToBacking(*descriptor.compare) : WGPUCompareFunction_Undefined,
        .maxAnisotropy = descriptor.maxAnisotropy,
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

void DeviceImpl::updateExternalTexture(const WebCore::WebGPU::ExternalTexture&, const WebCore::MediaPlayerIdentifier&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<ExternalTexture> DeviceImpl::importExternalTexture(const ExternalTextureDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    auto pixelBuffer = std::get_if<RetainPtr<CVPixelBufferRef>>(&descriptor.videoBacking);
    WGPUExternalTextureDescriptor backingDescriptor {
        .nextInChain = nullptr,
        .label = label.data(),
        .pixelBuffer = pixelBuffer ? pixelBuffer->get() : nullptr,
        .colorSpace = convertToWGPUColorSpace(descriptor.colorSpace),
    };
    return ExternalTextureImpl::create(adoptWebGPU(wgpuDeviceImportExternalTexture(m_backing.get(), &backingDescriptor)), descriptor, m_convertToBackingContext);
}

RefPtr<BindGroupLayout> DeviceImpl::createBindGroupLayout(const BindGroupLayoutDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    auto backingEntries = WTF::map(descriptor.entries, [&](auto& entry) {
        return WGPUBindGroupLayoutEntry {
            .nextInChain = nullptr,
            .binding = entry.binding,
            .metalBinding = { entry.binding, entry.binding, entry.binding },
            .visibility = m_convertToBackingContext->convertShaderStageFlagsToBacking(entry.visibility),
            .buffer = {
                nullptr,
                entry.buffer ? m_convertToBackingContext->convertToBacking(entry.buffer->type) : WGPUBufferBindingType_Undefined,
                entry.buffer ? entry.buffer->hasDynamicOffset : false,
                entry.buffer ? entry.buffer->minBindingSize : 0,
                0,
            },
            .sampler = {
                nullptr,
                entry.sampler ? m_convertToBackingContext->convertToBacking(entry.sampler->type) : WGPUSamplerBindingType_Undefined,
            },
            .texture = {
                nullptr,
                entry.externalTexture ? static_cast<WGPUTextureSampleType>(WGPUTextureSampleType_ExternalTexture) : (entry.texture ? m_convertToBackingContext->convertToBacking(entry.texture->sampleType) : WGPUTextureSampleType_Undefined),
                (!entry.externalTexture && entry.texture) ? m_convertToBackingContext->convertToBacking(entry.texture->viewDimension) : WGPUTextureViewDimension_Undefined,
                (!entry.externalTexture && entry.texture) ? entry.texture->multisampled : false,
            },
            .storageTexture = {
                nullptr,
                entry.storageTexture ? m_convertToBackingContext->convertToBacking(entry.storageTexture->access) : WGPUStorageTextureAccess_Undefined,
                entry.storageTexture ? m_convertToBackingContext->convertToBacking(entry.storageTexture->format) : WGPUTextureFormat_Undefined,
                entry.storageTexture ? m_convertToBackingContext->convertToBacking(entry.storageTexture->viewDimension) : WGPUTextureViewDimension_Undefined,
            },
        };
    });

    WGPUBindGroupLayoutDescriptor backingDescriptor {
        nullptr,
        label.data(),
        static_cast<uint32_t>(backingEntries.size()),
        backingEntries.data(),
    };

    return BindGroupLayoutImpl::create(adoptWebGPU(wgpuDeviceCreateBindGroupLayout(m_backing.get(), &backingDescriptor)), m_convertToBackingContext);
}

RefPtr<PipelineLayout> DeviceImpl::createPipelineLayout(const PipelineLayoutDescriptor& descriptor)
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

RefPtr<BindGroup> DeviceImpl::createBindGroup(const BindGroupDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    SegmentedVector<WGPUBindGroupExternalTextureEntry, 1> chainedEntries;
    auto backingEntries = descriptor.entries.map([&convertToBackingContext = m_convertToBackingContext.get(), &chainedEntries](const auto& bindGroupEntry) {
        auto externalTexture = std::holds_alternative<std::reference_wrapper<ExternalTexture>>(bindGroupEntry.resource) ? convertToBackingContext.convertToBacking(std::get<std::reference_wrapper<ExternalTexture>>(bindGroupEntry.resource).get()) : nullptr;
        chainedEntries.append(WGPUBindGroupExternalTextureEntry {
            .chain = {
                .next = nullptr,
                .sType = static_cast<WGPUSType>(WGPUSTypeExtended_BindGroupEntryExternalTexture)
            },
            .externalTexture = externalTexture,
        });
        return WGPUBindGroupEntry {
            .nextInChain = externalTexture ? reinterpret_cast<WGPUChainedStruct*>(&chainedEntries.last()) : nullptr,
            .binding = bindGroupEntry.binding,
            .buffer = std::holds_alternative<BufferBinding>(bindGroupEntry.resource) ? convertToBackingContext.convertToBacking(std::get<BufferBinding>(bindGroupEntry.resource).buffer) : nullptr,
            .offset = std::holds_alternative<BufferBinding>(bindGroupEntry.resource) ? std::get<BufferBinding>(bindGroupEntry.resource).offset : 0,
            .size = std::holds_alternative<BufferBinding>(bindGroupEntry.resource) ? std::get<BufferBinding>(bindGroupEntry.resource).size.value_or(WGPU_WHOLE_SIZE) : 0,
            .sampler = std::holds_alternative<std::reference_wrapper<Sampler>>(bindGroupEntry.resource) ? convertToBackingContext.convertToBacking(std::get<std::reference_wrapper<Sampler>>(bindGroupEntry.resource).get()) : nullptr,
            .textureView = std::holds_alternative<std::reference_wrapper<TextureView>>(bindGroupEntry.resource) ? convertToBackingContext.convertToBacking(std::get<std::reference_wrapper<TextureView>>(bindGroupEntry.resource).get()) : nullptr,
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

RefPtr<ShaderModule> DeviceImpl::createShaderModule(const ShaderModuleDescriptor& descriptor)
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

    std::optional<CString> entryPoint;
    if (auto& descriptorEntryPoint = descriptor.compute.entryPoint) {
        entryPoint = descriptorEntryPoint->utf8();
        if (descriptorEntryPoint->length() != String::fromUTF8(entryPoint->data()).length())
            entryPoint = invalidEntryPointName();
    }

    auto constantNames = descriptor.compute.constants.map([](const auto& constant) {
        bool lengthsMatch = constant.key.length() == String::fromUTF8(constant.key.utf8().data()).length();
        return lengthsMatch ? constant.key.utf8() : "";
    });

    Vector<WGPUConstantEntry> backingConstantEntries(descriptor.compute.constants.size(), [&](size_t i) {
        const auto& constant = descriptor.compute.constants[i];
        return WGPUConstantEntry {
            nullptr,
            constantNames[i].data(),
            constant.value
        };
    });

    WGPUComputePipelineDescriptor backingDescriptor {
        nullptr,
        label.data(),
        descriptor.layout ? convertToBackingContext.convertToBacking(*descriptor.layout) : nullptr, {
            nullptr,
            convertToBackingContext.convertToBacking(descriptor.compute.module),
            entryPoint ? entryPoint->data() : nullptr,
            static_cast<uint32_t>(backingConstantEntries.size()),
            backingConstantEntries.data(),
        }
    };

    return callback(backingDescriptor);
}

RefPtr<ComputePipeline> DeviceImpl::createComputePipeline(const ComputePipelineDescriptor& descriptor)
{
    return convertToBacking(descriptor, m_convertToBackingContext, [backing = m_backing, &convertToBackingContext = m_convertToBackingContext.get()](const WGPUComputePipelineDescriptor& backingDescriptor) {
        return ComputePipelineImpl::create(adoptWebGPU(wgpuDeviceCreateComputePipeline(backing.get(), &backingDescriptor)), convertToBackingContext);
    });
}

template <typename T>
static auto convertToBacking(const RenderPipelineDescriptor& descriptor, ConvertToBackingContext& convertToBackingContext, T&& callback)
{
    auto label = descriptor.label.utf8();

    std::optional<CString> vertexEntryPoint;
    if (auto& descriptorEntryPoint = descriptor.vertex.entryPoint) {
        vertexEntryPoint = descriptorEntryPoint->utf8();
        if (descriptorEntryPoint->length() != String::fromUTF8(vertexEntryPoint->data()).length())
            vertexEntryPoint = invalidEntryPointName();
    }

    auto vertexConstantNames = descriptor.vertex.constants.map([](const auto& constant) {
        bool lengthsMatch = constant.key.length() == String::fromUTF8(constant.key.utf8().data()).length();
        return lengthsMatch ? constant.key.utf8() : "";
    });

    Vector<WGPUConstantEntry> vertexConstantEntries(descriptor.vertex.constants.size(), [&](size_t i) {
        const auto& constant = descriptor.vertex.constants[i];
        return WGPUConstantEntry {
            nullptr,
            vertexConstantNames[i].data(),
            constant.value
        };
    });

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

    Vector<WGPUVertexBufferLayout> backingBuffers(descriptor.vertex.buffers.size(), [&](size_t i) {
        const auto& buffer = descriptor.vertex.buffers[i];
        return WGPUVertexBufferLayout {
            buffer ? buffer->arrayStride : WGPU_COPY_STRIDE_UNDEFINED,
            buffer ? convertToBackingContext.convertToBacking(buffer->stepMode) : WGPUVertexStepMode_Vertex,
            static_cast<uint32_t>(backingAttributes[i].size()),
            backingAttributes[i].data(),
        };
    });

    WGPUDepthStencilState depthStencilState {
        .nextInChain = nullptr,
        .format = descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->format) : WGPUTextureFormat_Undefined,
        .depthWriteEnabled = descriptor.depthStencil ? descriptor.depthStencil->depthWriteEnabled : false,
        .depthCompare = (descriptor.depthStencil && descriptor.depthStencil->depthCompare) ? convertToBackingContext.convertToBacking(*descriptor.depthStencil->depthCompare) : WGPUCompareFunction_Undefined,
        .stencilFront = {
            .compare = descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilFront.compare) : WGPUCompareFunction_Undefined,
            .failOp = descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilFront.failOp) : WGPUStencilOperation_Keep,
            .depthFailOp = descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilFront.depthFailOp) : WGPUStencilOperation_Keep,
            .passOp = descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilFront.passOp) : WGPUStencilOperation_Keep,
        },
        .stencilBack = {
            .compare = descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilBack.compare) : WGPUCompareFunction_Undefined,
            .failOp = descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilBack.failOp) : WGPUStencilOperation_Keep,
            .depthFailOp = descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilBack.depthFailOp) : WGPUStencilOperation_Keep,
            .passOp = descriptor.depthStencil ? convertToBackingContext.convertToBacking(descriptor.depthStencil->stencilBack.passOp) : WGPUStencilOperation_Keep,
        },
        .stencilReadMask = descriptor.depthStencil && descriptor.depthStencil->stencilReadMask ? *descriptor.depthStencil->stencilReadMask : 0,
        .stencilWriteMask = descriptor.depthStencil && descriptor.depthStencil->stencilWriteMask ? *descriptor.depthStencil->stencilWriteMask : 0,
        .depthBias = descriptor.depthStencil ? descriptor.depthStencil->depthBias : 0,
        .depthBiasSlopeScale = descriptor.depthStencil ? descriptor.depthStencil->depthBiasSlopeScale : 0,
        .depthBiasClamp = descriptor.depthStencil ? descriptor.depthStencil->depthBiasClamp : 0,
    };

    std::optional<CString> fragmentEntryPoint;
    Vector<CString> fragmentConstantNames;
    if (descriptor.fragment) {
        if (auto& descriptorEntryPoint = descriptor.fragment->entryPoint) {
            fragmentEntryPoint = descriptorEntryPoint->utf8();
            if (descriptorEntryPoint->length() != String::fromUTF8(descriptor.fragment->entryPoint->utf8().data()).length())
                fragmentEntryPoint = invalidEntryPointName();
        }

        fragmentConstantNames = descriptor.fragment->constants.map([](const auto& constant) {
            bool lengthsMatch = constant.key.length() == String::fromUTF8(constant.key.utf8().data()).length();
            return lengthsMatch ? constant.key.utf8() : "";
        });
    }

    Vector<WGPUConstantEntry> fragmentConstantEntries;
    if (descriptor.fragment) {
        fragmentConstantEntries = Vector<WGPUConstantEntry>(descriptor.fragment->constants.size(), [&](size_t i) {
            const auto& constant = descriptor.fragment->constants[i];
            return WGPUConstantEntry {
                nullptr,
                fragmentConstantNames[i].data(),
                constant.value,
            };
        });
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
        colorTargets = Vector<WGPUColorTargetState>(descriptor.fragment->targets.size(), [&](size_t i) {
            if (auto& target = descriptor.fragment->targets[i]) {
                return WGPUColorTargetState {
                    nullptr,
                    convertToBackingContext.convertToBacking(target->format),
                    blendStates[i] ? &*blendStates[i] : nullptr,
                    convertToBackingContext.convertColorWriteFlagsToBacking(target->writeMask),
                };
            }
            return WGPUColorTargetState {
                nullptr,
                    WGPUTextureFormat_Undefined,
                    nullptr,
                    WGPUMapMode_None,
            };
        });
    }

    WGPUFragmentState fragmentState {
        nullptr,
        descriptor.fragment ? convertToBackingContext.convertToBacking(descriptor.fragment->module) : nullptr,
        fragmentEntryPoint ? fragmentEntryPoint->data() : nullptr,
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
        .nextInChain = nullptr,
        .label = label.data(),
        .layout = descriptor.layout ? convertToBackingContext.convertToBacking(*descriptor.layout) : nullptr,
        .vertex = {
            nullptr,
            convertToBackingContext.convertToBacking(descriptor.vertex.module),
            vertexEntryPoint ? vertexEntryPoint->data() : nullptr,
            static_cast<uint32_t>(vertexConstantEntries.size()),
            vertexConstantEntries.data(),
            static_cast<uint32_t>(backingBuffers.size()),
            backingBuffers.data(),
        },
        .primitive = {
            descriptor.primitive && descriptor.primitive->unclippedDepth ? &depthClipControl.chain : nullptr,
            descriptor.primitive ? convertToBackingContext.convertToBacking(descriptor.primitive->topology) : WGPUPrimitiveTopology_TriangleList,
            descriptor.primitive && descriptor.primitive->stripIndexFormat ? convertToBackingContext.convertToBacking(*descriptor.primitive->stripIndexFormat) : WGPUIndexFormat_Undefined,
            descriptor.primitive ? convertToBackingContext.convertToBacking(descriptor.primitive->frontFace) : WGPUFrontFace_CCW,
            descriptor.primitive ? convertToBackingContext.convertToBacking(descriptor.primitive->cullMode) : WGPUCullMode_None,
        },
        .depthStencil = descriptor.depthStencil ? &depthStencilState : nullptr,
        .multisample = {
            nullptr,
            descriptor.multisample ? descriptor.multisample->count : 1,
            descriptor.multisample ? descriptor.multisample->mask : 0xFFFFFFFFU,
            descriptor.multisample ? descriptor.multisample->alphaToCoverageEnabled : false,
        },
        .fragment = descriptor.fragment ? &fragmentState : nullptr,
    };

    return callback(backingDescriptor);
}

RefPtr<RenderPipeline> DeviceImpl::createRenderPipeline(const RenderPipelineDescriptor& descriptor)
{
    return convertToBacking(descriptor, m_convertToBackingContext, [backing = m_backing.copyRef(), &convertToBackingContext = m_convertToBackingContext.get()](const WGPURenderPipelineDescriptor& backingDescriptor) {
        return RenderPipelineImpl::create(adoptWebGPU(wgpuDeviceCreateRenderPipeline(backing.get(), &backingDescriptor)), convertToBackingContext);
    });
}

static void createComputePipelineAsyncCallback(WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, String&& message, void* userdata)
{
    auto block = reinterpret_cast<void(^)(WGPUCreatePipelineAsyncStatus, WGPUComputePipeline, String&&)>(userdata);
    block(status, pipeline, WTFMove(message));
    Block_release(block); // Block_release is matched with Block_copy below in DeviceImpl::createComputePipelineAsync().
}

void DeviceImpl::createComputePipelineAsync(const ComputePipelineDescriptor& descriptor, CompletionHandler<void(RefPtr<ComputePipeline>&&, String&&)>&& callback)
{
    convertToBacking(descriptor, m_convertToBackingContext, [backing = m_backing.copyRef(), &convertToBackingContext = m_convertToBackingContext.get(), callback = WTFMove(callback)](const WGPUComputePipelineDescriptor& backingDescriptor) mutable {
        auto blockPtr = makeBlockPtr([convertToBackingContext = Ref { convertToBackingContext }, callback = WTFMove(callback)](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, String&& message) mutable {
            if (status == WGPUCreatePipelineAsyncStatus_Success)
                callback(ComputePipelineImpl::create(adoptWebGPU(pipeline), convertToBackingContext), ""_s);
            else
                callback(nullptr, WTFMove(message));
        });
        wgpuDeviceCreateComputePipelineAsync(backing.get(), &backingDescriptor, &createComputePipelineAsyncCallback, Block_copy(blockPtr.get())); // Block_copy is matched with Block_release above in createComputePipelineAsyncCallback().
    });
}

static void createRenderPipelineAsyncCallback(WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, String&& message, void* userdata)
{
    auto block = reinterpret_cast<void(^)(WGPUCreatePipelineAsyncStatus, WGPURenderPipeline, String&&)>(userdata);
    block(status, pipeline, WTFMove(message));
    Block_release(block); // Block_release is matched with Block_copy below in DeviceImpl::createRenderPipelineAsync().
}

void DeviceImpl::createRenderPipelineAsync(const RenderPipelineDescriptor& descriptor, CompletionHandler<void(RefPtr<RenderPipeline>&&, String&&)>&& callback)
{
    convertToBacking(descriptor, m_convertToBackingContext, [backing = m_backing.copyRef(), convertToBackingContext = m_convertToBackingContext.copyRef(), callback = WTFMove(callback)](const WGPURenderPipelineDescriptor& backingDescriptor) mutable {
        auto blockPtr = makeBlockPtr([convertToBackingContext = convertToBackingContext.copyRef(), callback = WTFMove(callback)](WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, String&& message) mutable {
            if (status == WGPUCreatePipelineAsyncStatus_Success)
                callback(RenderPipelineImpl::create(adoptWebGPU(pipeline), convertToBackingContext), ""_s);
            else
                callback(nullptr, WTFMove(message));
        });
        wgpuDeviceCreateRenderPipelineAsync(backing.get(), &backingDescriptor, &createRenderPipelineAsyncCallback, Block_copy(blockPtr.get())); // Block_copy is matched with Block_release above in createRenderPipelineAsyncCallback().
    });
}

RefPtr<CommandEncoder> DeviceImpl::createCommandEncoder(const std::optional<CommandEncoderDescriptor>& descriptor)
{
    CString label = descriptor ? descriptor->label.utf8() : CString("");

    WGPUCommandEncoderDescriptor backingDescriptor {
        nullptr,
        label.data(),
    };

    return CommandEncoderImpl::create(adoptWebGPU(wgpuDeviceCreateCommandEncoder(m_backing.get(), &backingDescriptor)), m_convertToBackingContext);
}

RefPtr<RenderBundleEncoder> DeviceImpl::createRenderBundleEncoder(const RenderBundleEncoderDescriptor& descriptor)
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

RefPtr<QuerySet> DeviceImpl::createQuerySet(const QuerySetDescriptor& descriptor)
{
    auto label = descriptor.label.utf8();

    WGPUQuerySetDescriptor backingDescriptor {
        nullptr,
        label.data(),
        m_convertToBackingContext->convertToBacking(descriptor.type),
        descriptor.count,
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

static void setUncapturedScopeCallback(WGPUErrorType type, const char* message, void* userdata)
{
    auto block = reinterpret_cast<void(^)(WGPUErrorType, const char*)>(userdata);
    block(type, message);
    Block_release(block);
}

void DeviceImpl::popErrorScope(CompletionHandler<void(bool, std::optional<Error>&&)>&& callback)
{
    auto blockPtr = makeBlockPtr([callback = WTFMove(callback)](WGPUErrorType errorType, const char* message) mutable {
        std::optional<Error> error;
        bool succeeded = false;
        switch (errorType) {
        case WGPUErrorType_NoError:
            succeeded = true;
            break;
        case WGPUErrorType_Force32:
            break;
        case WGPUErrorType_Internal:
            error = { { InternalError::create(String::fromLatin1(message)) } };
            break;
        case WGPUErrorType_Validation:
            error = { { ValidationError::create(String::fromLatin1(message)) } };
            break;
        case WGPUErrorType_OutOfMemory:
            error = { { OutOfMemoryError::create() } };
            break;
        case WGPUErrorType_Unknown:
            break;
        case WGPUErrorType_DeviceLost:
            break;
        }

        callback(succeeded, WTFMove(error));
    });
    wgpuDevicePopErrorScope(m_backing.get(), &popErrorScopeCallback, Block_copy(blockPtr.get())); // Block_copy is matched with Block_release above in popErrorScopeCallback().
}

void DeviceImpl::resolveUncapturedErrorEvent(CompletionHandler<void(bool, std::optional<Error>&&)>&& callback)
{
    auto blockPtr = makeBlockPtr([callback = WTFMove(callback)](WGPUErrorType errorType, const char* message) mutable {
        std::optional<Error> error;
        bool hasUncapturedError = true;
        switch (errorType) {
        case WGPUErrorType_NoError:
            hasUncapturedError = false;
            break;
        case WGPUErrorType_Force32:
            break;
        case WGPUErrorType_Internal:
            error = { { InternalError::create(String::fromLatin1(message)) } };
            break;
        case WGPUErrorType_Validation:
            error = { { ValidationError::create(String::fromLatin1(message)) } };
            break;
        case WGPUErrorType_OutOfMemory:
            error = { { OutOfMemoryError::create() } };
            break;
        case WGPUErrorType_Unknown:
            break;
        case WGPUErrorType_DeviceLost:
            break;
        }

        callback(hasUncapturedError, WTFMove(error));
    });
    wgpuDeviceSetUncapturedErrorCallback(m_backing.get(), &setUncapturedScopeCallback, Block_copy(blockPtr.get()));
}

void DeviceImpl::resolveDeviceLostPromise(CompletionHandler<void(WebCore::WebGPU::DeviceLostReason)>&& callback)
{
    wgpuDeviceSetDeviceLostCallbackWithBlock(m_backing.get(), makeBlockPtr([callback = WTFMove(callback)](WGPUDeviceLostReason reason, const char*) mutable {
        switch (reason) {
        case WGPUDeviceLostReason_Undefined:
        case WGPUDeviceLostReason_Force32:
            callback(WebCore::WebGPU::DeviceLostReason::Unknown);
            return;
        case WGPUDeviceLostReason_Destroyed:
            callback(WebCore::WebGPU::DeviceLostReason::Destroyed);
            return;
        }
    }).get());
}

void DeviceImpl::setLabelInternal(const String& label)
{
    wgpuDeviceSetLabel(m_backing.get(), label.utf8().data());
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
