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
#include "GPURenderPipeline.h"

#include "GPUBindGroupLayout.h"
#include "GPUDevice.h"
#include "InspectorInstrumentation.h"
#include "WebGPUBlendFactor.h"
#include "WebGPUBlendOperation.h"
#include "WebGPUBlendState.h"
#include <wtf/Locker.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

Lock GPURenderPipeline::s_instancesLock;

Ref<GPURenderPipeline> GPURenderPipeline::create(Ref<WebGPU::RenderPipeline>&& backing, uint64_t uniqueId, GPUDevice* device, WebGPU::RenderPipelineDescriptor&& descriptor, const WebGPU::ShaderModuleDescriptor& vertexShaderModuleDescriptor, std::optional<WebGPU::ShaderModuleDescriptor>&& fragmentShaderModuleDescriptor, bool sharesVertexFragmentShader)
{
    Ref result = adoptRef(*new GPURenderPipeline(WTF::move(backing), uniqueId, device, WTF::move(descriptor), vertexShaderModuleDescriptor, WTF::move(fragmentShaderModuleDescriptor), sharesVertexFragmentShader));

    if (device)
        InspectorInstrumentation::didCreateWebGPURenderPipeline(*device, result);

    return result;
}

HashMap<GPURenderPipeline*, GPUDevice*>& GPURenderPipeline::instances()
{
    static NeverDestroyed<HashMap<GPURenderPipeline*, GPUDevice*>> instances;
    return instances;
}

Lock& GPURenderPipeline::instancesLock()
{
    return s_instancesLock;
}

void GPURenderPipeline::willDestroyDevice(GPUDevice& device)
{
    Locker locker { instancesLock() };
    for (auto& registeredDevice : instances().values()) {
        if (registeredDevice == &device) {
            // Don't remove any GPURenderPipeline from the instances list, as they may still exist.
            // Only remove the association with a GPUDevice.
            registeredDevice = nullptr;
        }
    }
}

GPURenderPipeline::GPURenderPipeline(Ref<WebGPU::RenderPipeline>&& backing, uint64_t uniqueId, GPUDevice* device, WebGPU::RenderPipelineDescriptor&& descriptor, const WebGPU::ShaderModuleDescriptor& vertexShaderModuleDescriptor, std::optional<WebGPU::ShaderModuleDescriptor>&& fragmentShaderModuleDescriptor, bool sharesVertexFragmentShader)
    : m_backing(WTF::move(backing))
    , m_uniqueId(uniqueId)
    , m_descriptor(WTF::move(descriptor))
    , m_vertexShaderModuleDescriptor(vertexShaderModuleDescriptor)
    , m_fragmentShaderModuleDescriptor(WTF::move(fragmentShaderModuleDescriptor))
    , m_sharesVertexFragmentShader(sharesVertexFragmentShader)
{
    if (device) {
        m_device = *device;

        Locker locker { instancesLock() };
        instances().add(this, device);
    }
}

GPURenderPipeline::~GPURenderPipeline()
{
    InspectorInstrumentation::willDestroyWebGPURenderPipeline(*this);

    Locker locker { instancesLock() };
    instances().remove(this);
}

bool GPURenderPipeline::hasActiveInspectorCanvasCallTracer() const
{
    RefPtr device = m_device;
    return device && device->hasActiveInspectorCanvasCallTracer();
}

GPUDevice* GPURenderPipeline::device() const
{
    return m_device;
}

String GPURenderPipeline::label() const
{
    return m_backing->label();
}

void GPURenderPipeline::setLabel(String&& label)
{
    m_descriptor.label = label;
    protect(backing())->setLabel(WTF::move(label));
}

Ref<GPUBindGroupLayout> GPURenderPipeline::getBindGroupLayout(uint32_t index)
{
    // "A new GPUBindGroupLayout wrapper is returned each time"
    return GPUBindGroupLayout::create(protect(backing())->getBindGroupLayout(index), m_uniqueId, protect(m_device));
}

void GPURenderPipeline::updateVertexShader(const String& source, CompletionHandler<void(bool)>&& completionHandler)
{
    updateShader(source, true, WTF::move(completionHandler));
}

void GPURenderPipeline::updateFragmentShader(const String& source, CompletionHandler<void(bool)>&& completionHandler)
{
    updateShader(source, false, WTF::move(completionHandler));
}

static bool isInspectorHighlightableCanvasFormat(WebGPU::TextureFormat format)
{
    switch (format) {
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
    case WebGPU::TextureFormat::Bgra8unorm:
    case WebGPU::TextureFormat::Bgra8unormSRGB:
    case WebGPU::TextureFormat::Rgba16float:
        return true;
    default:
        return false;
    }
}

static bool usesBlendConstant(const WebGPU::BlendComponent& component)
{
    return component.srcFactor == WebGPU::BlendFactor::Constant
        || component.srcFactor == WebGPU::BlendFactor::OneMinusConstant
        || component.dstFactor == WebGPU::BlendFactor::Constant
        || component.dstFactor == WebGPU::BlendFactor::OneMinusConstant;
}

static bool usesBlendConstant(const std::optional<WebGPU::BlendState>& blendState)
{
    return blendState && (usesBlendConstant(blendState->color) || usesBlendConstant(blendState->alpha));
}

void GPURenderPipeline::createPipelineForInspectorHighlight(unsigned canvasColorAttachmentMask, CompletionHandler<void(RefPtr<WebGPU::RenderPipeline>&&)>&& completionHandler) const
{
    RefPtr device = m_device.get();
    if (!device || !canvasColorAttachmentMask || !m_descriptor.fragment || !m_fragmentShaderModuleDescriptor) {
        completionHandler(nullptr);
        return;
    }

    RefPtr vertexShaderModule = device->backing().createShaderModule(m_vertexShaderModuleDescriptor);
    if (!vertexShaderModule) {
        completionHandler(nullptr);
        return;
    }

    RefPtr<WebGPU::ShaderModule> fragmentShaderModule;
    if (m_sharesVertexFragmentShader)
        fragmentShaderModule = vertexShaderModule;
    else
        fragmentShaderModule = device->backing().createShaderModule(*m_fragmentShaderModuleDescriptor);
    if (!fragmentShaderModule) {
        completionHandler(nullptr);
        return;
    }

    auto descriptor = m_descriptor;
    descriptor.vertex.module = *vertexShaderModule;
    descriptor.fragment->module = *fragmentShaderModule;
    bool changedBlendState = false;
    for (size_t i = 0; i < descriptor.fragment->targets.size() && i < 8; ++i) {
        auto& target = descriptor.fragment->targets[i];
        if (!(canvasColorAttachmentMask & (1 << i))) {
            // The blend constant is shared by every color attachment in a render pass, so changing it for highlighting must not affect a non-canvas target.
            if (target && usesBlendConstant(target->blend)) {
                completionHandler(nullptr);
                return;
            }
            continue;
        }

        if (!target || !isInspectorHighlightableCanvasFormat(target->format)) {
            completionHandler(nullptr);
            return;
        }

        WebGPU::BlendComponent blendComponent {
            WebGPU::BlendOperation::Add,
            WebGPU::BlendFactor::Constant,
            WebGPU::BlendFactor::OneMinusSrcAlpha,
        };
        target->blend = WebGPU::BlendState { blendComponent, blendComponent };
        changedBlendState = true;
    }

    if (!changedBlendState) {
        completionHandler(nullptr);
        return;
    }

    device->backing().createRenderPipelineWithPipelineLayoutFromPipelineAsync(descriptor, m_backing, WTF::move(completionHandler));
}

void GPURenderPipeline::updateShader(const String& source, bool updateVertexShader, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr device = m_device.get();
    if (!device) {
        completionHandler(false);
        return;
    }

    auto vertexShaderModuleDescriptor = m_vertexShaderModuleDescriptor;
    auto fragmentShaderModuleDescriptor = m_fragmentShaderModuleDescriptor;
    if (updateVertexShader)
        vertexShaderModuleDescriptor.code = source;
    else {
        if (!fragmentShaderModuleDescriptor) {
            completionHandler(false);
            return;
        }
        fragmentShaderModuleDescriptor->code = source;
    }

    if (m_sharesVertexFragmentShader) {
        vertexShaderModuleDescriptor.code = source;
        fragmentShaderModuleDescriptor->code = source;
    }

    device->backing().pauseAllErrorReporting(true);
    RefPtr vertexShaderModule = device->backing().createShaderModule(vertexShaderModuleDescriptor);
    if (!vertexShaderModule) {
        device->backing().pauseAllErrorReporting(false);
        completionHandler(false);
        return;
    }

    RefPtr<WebGPU::ShaderModule> fragmentShaderModule;
    if (fragmentShaderModuleDescriptor) {
        if (m_sharesVertexFragmentShader)
            fragmentShaderModule = vertexShaderModule;
        else
            fragmentShaderModule = device->backing().createShaderModule(*fragmentShaderModuleDescriptor);
        if (!fragmentShaderModule) {
            device->backing().pauseAllErrorReporting(false);
            completionHandler(false);
            return;
        }
    }
    device->backing().pauseAllErrorReporting(false);

    auto descriptor = m_descriptor;
    descriptor.vertex.module = *vertexShaderModule;
    if (descriptor.fragment) {
        if (!fragmentShaderModule) {
            completionHandler(false);
            return;
        }
        descriptor.fragment->module = *fragmentShaderModule;
    }

    device->backing().createRenderPipelineWithPipelineLayoutFromPipelineAsync(descriptor, m_backing, [weakThis = WeakPtr { *this }, descriptor, vertexShaderModuleDescriptor = WTF::move(vertexShaderModuleDescriptor), fragmentShaderModuleDescriptor = WTF::move(fragmentShaderModuleDescriptor), completionHandler = WTF::move(completionHandler)](RefPtr<WebGPU::RenderPipeline>&& pipeline) mutable {
        RefPtr protectedThis { weakThis };
        if (!protectedThis) {
            completionHandler(false);
            return;
        }

        if (pipeline)
            protectedThis->m_backing = pipeline.releaseNonNull();
        protectedThis->m_descriptor = WTF::move(descriptor);
        protectedThis->m_vertexShaderModuleDescriptor = WTF::move(vertexShaderModuleDescriptor);
        protectedThis->m_fragmentShaderModuleDescriptor = WTF::move(fragmentShaderModuleDescriptor);
        completionHandler(true);
    });
}

}
