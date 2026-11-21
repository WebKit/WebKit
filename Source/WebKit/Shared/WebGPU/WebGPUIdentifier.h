/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(GPU_PROCESS)

#include <wtf/ObjectIdentifier.h>
#include <wtf/Variant.h>

namespace WebKit {

struct RemoteGPUIdentifierType;
using RemoteGPUIdentifier = AtomicObjectIdentifier<RemoteGPUIdentifierType>;

// FIXME: should not be used. Figure out if still needed.
// Untyped: the value of an identifier of any of the types below. Used only by RemoteGPU::IsValid.
struct WebGPUIdentifierType;
using WebGPUIdentifier = AtomicObjectIdentifier<WebGPUIdentifierType>;

struct WebGPUAdapterIdentifierType;
using WebGPUAdapterIdentifier = AtomicObjectIdentifier<WebGPUAdapterIdentifierType>;

struct WebGPUBindGroupIdentifierType;
using WebGPUBindGroupIdentifier = AtomicObjectIdentifier<WebGPUBindGroupIdentifierType>;

struct WebGPUBindGroupLayoutIdentifierType;
using WebGPUBindGroupLayoutIdentifier = AtomicObjectIdentifier<WebGPUBindGroupLayoutIdentifierType>;

struct WebGPUBufferIdentifierType;
using WebGPUBufferIdentifier = AtomicObjectIdentifier<WebGPUBufferIdentifierType>;

struct WebGPUCommandBufferIdentifierType;
using WebGPUCommandBufferIdentifier = AtomicObjectIdentifier<WebGPUCommandBufferIdentifierType>;

struct WebGPUCommandEncoderIdentifierType;
using WebGPUCommandEncoderIdentifier = AtomicObjectIdentifier<WebGPUCommandEncoderIdentifierType>;

struct WebGPUCompositorIntegrationIdentifierType;
using WebGPUCompositorIntegrationIdentifier = AtomicObjectIdentifier<WebGPUCompositorIntegrationIdentifierType>;

struct WebGPUComputePassEncoderIdentifierType;
using WebGPUComputePassEncoderIdentifier = AtomicObjectIdentifier<WebGPUComputePassEncoderIdentifierType>;

struct WebGPUComputePipelineIdentifierType;
using WebGPUComputePipelineIdentifier = AtomicObjectIdentifier<WebGPUComputePipelineIdentifierType>;

struct WebGPUDeviceIdentifierType;
using WebGPUDeviceIdentifier = AtomicObjectIdentifier<WebGPUDeviceIdentifierType>;

struct WebGPUExternalTextureIdentifierType;
using WebGPUExternalTextureIdentifier = AtomicObjectIdentifier<WebGPUExternalTextureIdentifierType>;

struct WebGPUPipelineLayoutIdentifierType;
using WebGPUPipelineLayoutIdentifier = AtomicObjectIdentifier<WebGPUPipelineLayoutIdentifierType>;

struct WebGPUPresentationContextIdentifierType;
using WebGPUPresentationContextIdentifier = AtomicObjectIdentifier<WebGPUPresentationContextIdentifierType>;

struct WebGPUQuerySetIdentifierType;
using WebGPUQuerySetIdentifier = AtomicObjectIdentifier<WebGPUQuerySetIdentifierType>;

struct WebGPUQueueIdentifierType;
using WebGPUQueueIdentifier = AtomicObjectIdentifier<WebGPUQueueIdentifierType>;

struct WebGPURenderBundleIdentifierType;
using WebGPURenderBundleIdentifier = AtomicObjectIdentifier<WebGPURenderBundleIdentifierType>;

struct WebGPURenderBundleEncoderIdentifierType;
using WebGPURenderBundleEncoderIdentifier = AtomicObjectIdentifier<WebGPURenderBundleEncoderIdentifierType>;

struct WebGPURenderPassEncoderIdentifierType;
using WebGPURenderPassEncoderIdentifier = AtomicObjectIdentifier<WebGPURenderPassEncoderIdentifierType>;

struct WebGPURenderPipelineIdentifierType;
using WebGPURenderPipelineIdentifier = AtomicObjectIdentifier<WebGPURenderPipelineIdentifierType>;

struct WebGPUSamplerIdentifierType;
using WebGPUSamplerIdentifier = AtomicObjectIdentifier<WebGPUSamplerIdentifierType>;

struct WebGPUShaderModuleIdentifierType;
using WebGPUShaderModuleIdentifier = AtomicObjectIdentifier<WebGPUShaderModuleIdentifierType>;

struct WebGPUTextureIdentifierType;
using WebGPUTextureIdentifier = AtomicObjectIdentifier<WebGPUTextureIdentifierType>;

struct WebGPUTextureViewIdentifierType;
using WebGPUTextureViewIdentifier = AtomicObjectIdentifier<WebGPUTextureViewIdentifierType>;

struct WebGPUXRBindingIdentifierType;
using WebGPUXRBindingIdentifier = AtomicObjectIdentifier<WebGPUXRBindingIdentifierType>;

struct WebGPUXRProjectionLayerIdentifierType;
using WebGPUXRProjectionLayerIdentifier = AtomicObjectIdentifier<WebGPUXRProjectionLayerIdentifierType>;

struct WebGPUXRSubImageIdentifierType;
using WebGPUXRSubImageIdentifier = AtomicObjectIdentifier<WebGPUXRSubImageIdentifierType>;

struct WebGPUXRViewIdentifierType;
using WebGPUXRViewIdentifier = AtomicObjectIdentifier<WebGPUXRViewIdentifierType>;

// A render pass attachment view is either a texture view or a texture, see ::WebGPU::RenderPassAttachmentView.
using WebGPURenderPassAttachmentViewIdentifier = Variant<WebGPUTextureViewIdentifier, WebGPUTextureIdentifier>;

} // namespace WebKit

#endif
