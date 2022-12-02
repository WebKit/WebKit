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

#pragma once

#import "Adapter.h"
#import "HardwareCapabilities.h"
#import <IOSurface/IOSurfaceRef.h>
#import "Queue.h"
#import <wtf/CompletionHandler.h>
#import <wtf/FastMalloc.h>
#import <wtf/Function.h>
#import <wtf/Ref.h>
#import <wtf/ThreadSafeWeakPtr.h>
#import <wtf/Vector.h>
#import <wtf/WeakPtr.h>
#import <wtf/text/WTFString.h>

struct WGPUDeviceImpl {
};

namespace WebGPU {

class BindGroup;
class BindGroupLayout;
class Buffer;
class CommandEncoder;
class ComputePipeline;
class Instance;
class PipelineLayout;
class QuerySet;
class RenderBundleEncoder;
class RenderPipeline;
class Sampler;
class ShaderModule;
class Surface;
class SwapChain;
class Texture;

// https://gpuweb.github.io/gpuweb/#gpudevice
class Device : public WGPUDeviceImpl, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Device> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<Device> create(id<MTLDevice>, String&& deviceLabel, HardwareCapabilities&&, Adapter&);
    static Ref<Device> createInvalid(Adapter& adapter)
    {
        return adoptRef(*new Device(adapter));
    }

    ~Device();

    Ref<BindGroup> createBindGroup(const WGPUBindGroupDescriptor&);
    Ref<BindGroupLayout> createBindGroupLayout(const WGPUBindGroupLayoutDescriptor&);
    Ref<Buffer> createBuffer(const WGPUBufferDescriptor&);
    Ref<CommandEncoder> createCommandEncoder(const WGPUCommandEncoderDescriptor&);
    Ref<ComputePipeline> createComputePipeline(const WGPUComputePipelineDescriptor&);
    void createComputePipelineAsync(const WGPUComputePipelineDescriptor&, CompletionHandler<void(WGPUCreatePipelineAsyncStatus, Ref<ComputePipeline>&&, String&& message)>&& callback);
    Ref<PipelineLayout> createPipelineLayout(const WGPUPipelineLayoutDescriptor&);
    Ref<QuerySet> createQuerySet(const WGPUQuerySetDescriptor&);
    Ref<RenderBundleEncoder> createRenderBundleEncoder(const WGPURenderBundleEncoderDescriptor&);
    Ref<RenderPipeline> createRenderPipeline(const WGPURenderPipelineDescriptor&);
    void createRenderPipelineAsync(const WGPURenderPipelineDescriptor&, CompletionHandler<void(WGPUCreatePipelineAsyncStatus, Ref<RenderPipeline>&&, String&& message)>&& callback);
    Ref<Sampler> createSampler(const WGPUSamplerDescriptor&);
    Ref<ShaderModule> createShaderModule(const WGPUShaderModuleDescriptor&);
    Ref<Surface> createSurface(const WGPUSurfaceDescriptor&);
    Ref<SwapChain> createSwapChain(WGPUSurface, const WGPUSwapChainDescriptor&);
    Ref<Texture> createTexture(const WGPUTextureDescriptor&);
    void destroy();
    size_t enumerateFeatures(WGPUFeatureName* features);
    bool getLimits(WGPUSupportedLimits&);
    Queue& getQueue();
    bool hasFeature(WGPUFeatureName);
    bool popErrorScope(CompletionHandler<void(WGPUErrorType, String&&)>&& callback);
    void pushErrorScope(WGPUErrorFilter);
    void setDeviceLostCallback(Function<void(WGPUDeviceLostReason, String&&)>&&);
    void setUncapturedErrorCallback(Function<void(WGPUErrorType, String&&)>&&);
    void setLabel(String&&);

    bool isValid() const { return m_device; }
    bool isLost() const { return m_isLost; }
    const WGPULimits& limits() const { return m_capabilities.limits; }
    const Vector<WGPUFeatureName>& features() const { return m_capabilities.features; }
    const HardwareCapabilities::BaseCapabilities& baseCapabilities() const { return m_capabilities.baseCapabilities; }

    id<MTLDevice> device() const { return m_device; }

    void generateAValidationError(String&& message);

    Instance& instance() const { return m_adapter->instance(); }
    bool hasUnifiedMemory() const { return m_device.hasUnifiedMemory; }

private:
    Device(id<MTLDevice>, id<MTLCommandQueue> defaultQueue, HardwareCapabilities&&, Adapter&);
    Device(Adapter&);

    struct ErrorScope;
    ErrorScope* currentErrorScope(WGPUErrorFilter);
    bool validatePopErrorScope() const;
    id<MTLBuffer> safeCreateBuffer(NSUInteger length, MTLStorageMode, MTLCPUCacheMode = MTLCPUCacheModeDefaultCache, MTLHazardTrackingMode = MTLHazardTrackingModeDefault) const;
    bool validateCreateTexture(const WGPUTextureDescriptor&, const Vector<WGPUTextureFormat>& viewFormats);
    bool validateCreateIOSurfaceBackedTexture(const WGPUTextureDescriptor&, const Vector<WGPUTextureFormat>& viewFormats, IOSurfaceRef backing);

    bool validateRenderPipeline(const WGPURenderPipelineDescriptor&);

    void makeInvalid() { m_device = nil; }

    void loseTheDevice(WGPUDeviceLostReason);

    struct Error {
        WGPUErrorType type;
        String message;
    };
    struct ErrorScope {
        std::optional<Error> error;
        const WGPUErrorFilter filter;
    };

    id<MTLDevice> m_device { nil };
    const Ref<Queue> m_defaultQueue;

    Function<void(WGPUErrorType, String&&)> m_uncapturedErrorCallback;
    Vector<ErrorScope> m_errorScopeStack;

    Function<void(WGPUDeviceLostReason, String&&)> m_deviceLostCallback;
    bool m_isLost { false };
    id<NSObject> m_deviceObserver { nil };

    HardwareCapabilities m_capabilities { };

    const Ref<Adapter> m_adapter;
};

} // namespace WebGPU
