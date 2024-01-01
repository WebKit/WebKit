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
#include "GPUDevice.h"

#include "DOMPromiseProxy.h"
#include "GPUBindGroup.h"
#include "GPUBindGroupDescriptor.h"
#include "GPUBindGroupLayout.h"
#include "GPUBindGroupLayoutDescriptor.h"
#include "GPUBuffer.h"
#include "GPUBufferDescriptor.h"
#include "GPUCommandEncoder.h"
#include "GPUCommandEncoderDescriptor.h"
#include "GPUComputePipeline.h"
#include "GPUComputePipelineDescriptor.h"
#include "GPUExternalTexture.h"
#include "GPUExternalTextureDescriptor.h"
#include "GPUPipelineError.h"
#include "GPUPipelineLayout.h"
#include "GPUPipelineLayoutDescriptor.h"
#include "GPUPresentationContext.h"
#include "GPUQuerySet.h"
#include "GPUQuerySetDescriptor.h"
#include "GPURenderBundleEncoder.h"
#include "GPURenderBundleEncoderDescriptor.h"
#include "GPURenderPipeline.h"
#include "GPURenderPipelineDescriptor.h"
#include "GPUSampler.h"
#include "GPUSamplerDescriptor.h"
#include "GPUShaderModule.h"
#include "GPUShaderModuleDescriptor.h"
#include "GPUSupportedFeatures.h"
#include "GPUSupportedLimits.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"
#include "JSDOMPromiseDeferred.h"
#include "JSGPUComputePipeline.h"
#include "JSGPUDeviceLostInfo.h"
#include "JSGPUInternalError.h"
#include "JSGPUOutOfMemoryError.h"
#include "JSGPUPipelineError.h"
#include "JSGPURenderPipeline.h"
#include "JSGPUValidationError.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(GPUDevice);

GPUDevice::GPUDevice(ScriptExecutionContext* scriptExecutionContext, Ref<WebGPU::Device>&& backing)
    : ActiveDOMObject { scriptExecutionContext }
    , m_lostPromise(makeUniqueRef<LostPromise>())
    , m_backing(WTFMove(backing))
    , m_queue(GPUQueue::create(Ref { m_backing->queue() }))
    , m_autoPipelineLayout(createAutoPipelineLayout())
{
}

GPUDevice::~GPUDevice() = default;

String GPUDevice::label() const
{
    return m_backing->label();
}

void GPUDevice::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

Ref<GPUSupportedFeatures> GPUDevice::features() const
{
    return GPUSupportedFeatures::create(m_backing->features());
}

Ref<GPUSupportedLimits> GPUDevice::limits() const
{
    return GPUSupportedLimits::create(m_backing->limits());
}

Ref<GPUQueue> GPUDevice::queue() const
{
    return m_queue;
}

void GPUDevice::addBufferToUnmap(GPUBuffer& buffer)
{
    m_buffersToUnmap.add(&buffer);
}

void GPUDevice::removeBufferToUnmap(GPUBuffer& buffer)
{
    m_buffersToUnmap.remove(&buffer);
}

void GPUDevice::destroy(ScriptExecutionContext& scriptExecutionContext)
{
    for (auto& buffer : m_buffersToUnmap)
        buffer->destroy(scriptExecutionContext);

    m_buffersToUnmap.clear();

    m_backing->destroy();
}

GPUDevice::LostPromise& GPUDevice::lost()
{
    if (m_waitingForDeviceLostPromise)
        return m_lostPromise;

    m_waitingForDeviceLostPromise = true;
    m_backing->resolveDeviceLostPromise([weakThis = WeakPtr { *this }](WebCore::WebGPU::DeviceLostReason reason) {
        if (!weakThis)
            return;

        auto ref = GPUDeviceLostInfo::create(WebCore::WebGPU::DeviceLostInfo::create(reason, ""_s));
        weakThis->m_lostPromise->resolve(WTFMove(ref));
    });

    return m_lostPromise;
}

ExceptionOr<Ref<GPUBuffer>> GPUDevice::createBuffer(const GPUBufferDescriptor& bufferDescriptor)
{
    auto bufferSize = bufferDescriptor.size;
    if (bufferDescriptor.mappedAtCreation && bufferSize > limits()->maxBufferSize())
        return Exception { ExceptionCode::RangeError };

    auto usage = bufferDescriptor.usage;
    auto mappedAtCreation = bufferDescriptor.mappedAtCreation;
    return GPUBuffer::create(m_backing->createBuffer(bufferDescriptor.convertToBacking()), bufferSize, usage, mappedAtCreation, *this);
}

Ref<GPUTexture> GPUDevice::createTexture(const GPUTextureDescriptor& textureDescriptor)
{
    return GPUTexture::create(m_backing->createTexture(textureDescriptor.convertToBacking()), textureDescriptor);
}

static WebGPU::SamplerDescriptor convertToBacking(const std::optional<GPUSamplerDescriptor>& samplerDescriptor)
{
    if (!samplerDescriptor) {
        return {
            { },
            WebGPU::AddressMode::ClampToEdge,
            WebGPU::AddressMode::ClampToEdge,
            WebGPU::AddressMode::ClampToEdge,
            WebGPU::FilterMode::Nearest,
            WebGPU::FilterMode::Nearest,
            WebGPU::MipmapFilterMode::Nearest,
            0,
            32,
            std::nullopt,
            1
        };
    }

    return samplerDescriptor->convertToBacking();
}

Ref<GPUSampler> GPUDevice::createSampler(const std::optional<GPUSamplerDescriptor>& samplerDescriptor)
{
    return GPUSampler::create(m_backing->createSampler(convertToBacking(samplerDescriptor)));
}

Ref<GPUExternalTexture> GPUDevice::importExternalTexture(const GPUExternalTextureDescriptor& externalTextureDescriptor)
{
    return GPUExternalTexture::create(m_backing->importExternalTexture(externalTextureDescriptor.convertToBacking()));
}

Ref<GPUBindGroupLayout> GPUDevice::createBindGroupLayout(const GPUBindGroupLayoutDescriptor& bindGroupLayoutDescriptor)
{
    return GPUBindGroupLayout::create(m_backing->createBindGroupLayout(bindGroupLayoutDescriptor.convertToBacking()));
}

Ref<GPUPipelineLayout> GPUDevice::createAutoPipelineLayout()
{
    return GPUPipelineLayout::create(m_backing->createPipelineLayout(WebGPU::PipelineLayoutDescriptor {
        { "autoLayout"_s, },
        std::nullopt
    }));
}

Ref<GPUPipelineLayout> GPUDevice::createPipelineLayout(const GPUPipelineLayoutDescriptor& pipelineLayoutDescriptor)
{
    return GPUPipelineLayout::create(m_backing->createPipelineLayout(pipelineLayoutDescriptor.convertToBacking()));
}

Ref<GPUBindGroup> GPUDevice::createBindGroup(const GPUBindGroupDescriptor& bindGroupDescriptor)
{
    return GPUBindGroup::create(m_backing->createBindGroup(bindGroupDescriptor.convertToBacking()));
}

Ref<GPUShaderModule> GPUDevice::createShaderModule(const GPUShaderModuleDescriptor& shaderModuleDescriptor)
{
    return GPUShaderModule::create(m_backing->createShaderModule(shaderModuleDescriptor.convertToBacking(m_autoPipelineLayout)));
}

Ref<GPUComputePipeline> GPUDevice::createComputePipeline(const GPUComputePipelineDescriptor& computePipelineDescriptor)
{
    return GPUComputePipeline::create(m_backing->createComputePipeline(computePipelineDescriptor.convertToBacking(m_autoPipelineLayout)));
}

Ref<GPURenderPipeline> GPUDevice::createRenderPipeline(const GPURenderPipelineDescriptor& renderPipelineDescriptor)
{
    return GPURenderPipeline::create(m_backing->createRenderPipeline(renderPipelineDescriptor.convertToBacking(m_autoPipelineLayout)));
}

void GPUDevice::createComputePipelineAsync(const GPUComputePipelineDescriptor& computePipelineDescriptor, CreateComputePipelineAsyncPromise&& promise)
{
    m_backing->createComputePipelineAsync(computePipelineDescriptor.convertToBacking(m_autoPipelineLayout), [promise = WTFMove(promise)](RefPtr<WebGPU::ComputePipeline>&& computePipeline) mutable {
        if (computePipeline.get())
            promise.resolve(GPUComputePipeline::create(computePipeline.releaseNonNull()));
        else
            promise.rejectType<IDLInterface<GPUPipelineError>>(GPUPipelineError::create(""_s, { GPUPipelineErrorReason::Validation }));
    });
}

void GPUDevice::createRenderPipelineAsync(const GPURenderPipelineDescriptor& renderPipelineDescriptor, CreateRenderPipelineAsyncPromise&& promise)
{
    m_backing->createRenderPipelineAsync(renderPipelineDescriptor.convertToBacking(m_autoPipelineLayout), [promise = WTFMove(promise)](RefPtr<WebGPU::RenderPipeline>&& renderPipeline) mutable {
        if (renderPipeline.get())
            promise.resolve(GPURenderPipeline::create(renderPipeline.releaseNonNull()));
        else
            promise.rejectType<IDLInterface<GPUPipelineError>>(GPUPipelineError::create(""_s, { GPUPipelineErrorReason::Validation }));
    });
}

static WebGPU::CommandEncoderDescriptor convertToBacking(const std::optional<GPUCommandEncoderDescriptor>& commandEncoderDescriptor)
{
    if (!commandEncoderDescriptor)
        return { };

    return commandEncoderDescriptor->convertToBacking();
}

Ref<GPUCommandEncoder> GPUDevice::createCommandEncoder(const std::optional<GPUCommandEncoderDescriptor>& commandEncoderDescriptor)
{
    return GPUCommandEncoder::create(m_backing->createCommandEncoder(convertToBacking(commandEncoderDescriptor)));
}

Ref<GPURenderBundleEncoder> GPUDevice::createRenderBundleEncoder(const GPURenderBundleEncoderDescriptor& renderBundleEncoderDescriptor)
{
    return GPURenderBundleEncoder::create(m_backing->createRenderBundleEncoder(renderBundleEncoderDescriptor.convertToBacking()));
}

Ref<GPUQuerySet> GPUDevice::createQuerySet(const GPUQuerySetDescriptor& querySetDescriptor)
{
    return GPUQuerySet::create(m_backing->createQuerySet(querySetDescriptor.convertToBacking()), querySetDescriptor);
}

void GPUDevice::pushErrorScope(GPUErrorFilter errorFilter)
{
    m_backing->pushErrorScope(convertToBacking(errorFilter));
}

void GPUDevice::popErrorScope(ErrorScopePromise&& errorScopePromise)
{
    m_backing->popErrorScope([promise = WTFMove(errorScopePromise)](std::optional<WebGPU::Error>&& error) mutable {
        if (!error) {
            promise.resolve(std::nullopt);
            return;
        }
        WTF::switchOn(WTFMove(*error), [&promise](Ref<WebGPU::OutOfMemoryError>&& outOfMemoryError) {
            GPUError error = RefPtr<GPUOutOfMemoryError>(GPUOutOfMemoryError::create(WTFMove(outOfMemoryError)));
            promise.resolve(error);
        }, [&promise](Ref<WebGPU::ValidationError>&& validationError) {
            GPUError error = RefPtr<GPUValidationError>(GPUValidationError::create(WTFMove(validationError)));
            promise.resolve(error);
        }, [&promise](Ref<WebGPU::InternalError>&& internalError) {
            GPUError error = RefPtr<GPUInternalError>(GPUInternalError::create(WTFMove(internalError)));
            promise.resolve(error);
        });
    });
}

}
