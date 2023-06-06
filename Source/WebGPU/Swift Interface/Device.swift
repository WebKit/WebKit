/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

import Foundation

public class Device {
    internal let device: __WGPUDevice
    private let instance: Instance
    private let ownership: Ownership

    internal init(_ device: __WGPUDevice, instance: Instance, ownership: Ownership = .Owned) {
        self.device = device
        self.instance = instance
        self.ownership = ownership
    }

    deinit {
        if ownership == .Owned {
            __wgpuDeviceRelease(device)
        }
    }

    public func createBindGroup(_ descriptor: WGPUBindGroupDescriptor) -> BindGroup {
        var localDescriptor = descriptor
        return BindGroup(__wgpuDeviceCreateBindGroup(device, &localDescriptor))
    }

    public func createBindGroupLayout(_ descriptor: WGPUBindGroupLayoutDescriptor) -> BindGroupLayout {
        var localDescriptor = descriptor
        return BindGroupLayout(__wgpuDeviceCreateBindGroupLayout(device, &localDescriptor))
    }

    public func createBuffer(_ descriptor: WGPUBufferDescriptor) -> Buffer {
        var localDescriptor = descriptor
        return Buffer(__wgpuDeviceCreateBuffer(device, &localDescriptor), instance: instance)
    }

    public func createCommandEncoder(_ descriptor: WGPUCommandEncoderDescriptor) -> CommandEncoder {
        var localDescriptor = descriptor
        return CommandEncoder(__wgpuDeviceCreateCommandEncoder(device, &localDescriptor))
    }

    public func createComputePipeline(_ descriptor: WGPUComputePipelineDescriptor) -> ComputePipeline {
        var localDescriptor = descriptor
        return ComputePipeline(__wgpuDeviceCreateComputePipeline(device, &localDescriptor))
    }

    public func createComputePipelineAsync(_ descriptor: WGPUComputePipelineDescriptor) async -> (WGPUCreatePipelineAsyncStatus, ComputePipeline, String?) {
        var localDescriptor = descriptor
        return await withCheckedContinuation { continuation in
            instance.scheduleWork { [self] in
                __wgpuDeviceCreateComputePipelineAsyncWithBlock(device, &localDescriptor) { (status, computePipeline, message) in
                    continuation.resume(returning: (status, ComputePipeline(computePipeline!), (message != nil) ? String(cString: message!) : nil))
                }
            }
        }
    }

    public func createPipelineLayout(_ descriptor: WGPUPipelineLayoutDescriptor) -> PipelineLayout {
        var localDescriptor = descriptor
        return PipelineLayout(__wgpuDeviceCreatePipelineLayout(device, &localDescriptor))
    }

    public func createQuerySet(_ descriptor: WGPUQuerySetDescriptor) -> QuerySet {
        var localDescriptor = descriptor
        return QuerySet(__wgpuDeviceCreateQuerySet(device, &localDescriptor))
    }

    public func createRenderBundleEncoder(_ descriptor: WGPURenderBundleEncoderDescriptor) -> RenderBundleEncoder {
        var localDescriptor = descriptor
        return RenderBundleEncoder(__wgpuDeviceCreateRenderBundleEncoder(device, &localDescriptor))
    }

    public func createRenderPipeline(_ descriptor: WGPURenderPipelineDescriptor) -> RenderPipeline {
        var localDescriptor = descriptor
        return RenderPipeline(__wgpuDeviceCreateRenderPipeline(device, &localDescriptor))
    }

    public func createRenderPipelineAsync(_ descriptor: WGPURenderPipelineDescriptor) async -> (WGPUCreatePipelineAsyncStatus, RenderPipeline, String?) {
        var localDescriptor = descriptor
        return await withCheckedContinuation { continuation in
            instance.scheduleWork { [self] in
                __wgpuDeviceCreateRenderPipelineAsyncWithBlock(device, &localDescriptor) { (status, renderPipeline, message) in
                    continuation.resume(returning: (status, RenderPipeline(renderPipeline!), (message != nil) ? String(cString: message!) : nil))
                }
            }
        }
    }

    public func createSampler(_ descriptor: WGPUSamplerDescriptor) -> Sampler {
        var localDescriptor = descriptor
        return Sampler(__wgpuDeviceCreateSampler(device, &localDescriptor))
    }

    public func createShaderModule(_ descriptor: WGPUShaderModuleDescriptor) -> ShaderModule {
        var localDescriptor = descriptor
        return ShaderModule(__wgpuDeviceCreateShaderModule(device, &localDescriptor), instance: instance)
    }

    public func createSwapChain(_ surface: Surface, _ descriptor: WGPUSwapChainDescriptor) -> SwapChain {
        var localDescriptor = descriptor
        return SwapChain(__wgpuDeviceCreateSwapChain(device, surface.surface, &localDescriptor))
    }

    public func createTexture(_ descriptor: WGPUTextureDescriptor) -> Texture {
        var localDescriptor = descriptor
        return Texture(__wgpuDeviceCreateTexture(device, &localDescriptor))
    }

    public func destroy() {
        __wgpuDeviceDestroy(device)
    }

    public func enumerateFeatures() -> [WGPUFeatureName] {
        let count = __wgpuDeviceEnumerateFeatures(device, nil)
        var result = Array(repeating: WGPUFeatureName_Undefined, count: count)
        result.withUnsafeMutableBufferPointer { pointer in
            _ = __wgpuDeviceEnumerateFeatures(device, pointer.baseAddress)
        }
        return result
    }

    public func getLimits() -> WGPULimits {
        var result = WGPUSupportedLimits()
        assert(__wgpuDeviceGetLimits(device, &result) == true)
        return result.limits
    }

    public func getQueue() -> Queue {
        return Queue(__wgpuDeviceGetQueue(device), instance: instance, ownership: .Unowned)
    }

    public func hasFeature(_ featureName: WGPUFeatureName) -> Bool {
        return __wgpuDeviceHasFeature(device, featureName)
    }

    public func popErrorScope() async -> (WGPUErrorType, String?) {
        return await withCheckedContinuation { continuation in
            instance.scheduleWork { [self] in
                __wgpuDevicePopErrorScopeWithBlock(device) { (status, message) in
                    continuation.resume(returning: (status, (message != nil) ? String(cString: message!) : nil))
                }
            }
        }
    }

    public func pushErrorScope(_ filter: WGPUErrorFilter) {
        return __wgpuDevicePushErrorScope(device, filter)
    }

    public func setLabel(_ label: String) {
        __wgpuDeviceSetLabel(device, label)
    }

    // FIXME: Make this more Swifty.
    public func setUncapturedErrorCallback(_ callback: @escaping WGPUErrorBlockCallback) {
        __wgpuDeviceSetUncapturedErrorCallbackWithBlock(device, callback)
    }
}
