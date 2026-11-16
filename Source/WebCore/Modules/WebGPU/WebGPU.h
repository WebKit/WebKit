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

#pragma once

#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUDevice.h>
#include <optional>
#include <wtf/AbstractRefCounted.h>
#include <wtf/CompletionHandler.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if PLATFORM(COCOA) && ENABLE(VIDEO)
#include <WebCore/MediaPlayerIdentifier.h>
#endif

namespace WebCore {

class GraphicsContext;
class IntSize;
class NativeImage;
class VideoFrame;
class WebGPUCompositorIntegration;

struct WebGPUExternalImageSource;
struct WebGPUExternalTextureDescriptor;
struct WebGPUImageCopyTextureTagged;
struct WebGPUPresentationContextDescriptor;

// The root of WebGPU in WebCore. It creates the adapters and has the commands that take WebCore
// types, which the WebGPU implementations cannot see.
class WebGPUIntegration : public AbstractRefCounted {
public:
    WEBCORE_EXPORT virtual ~WebGPUIntegration() = default;

    virtual void requestAdapter(const ::WebGPU::RequestAdapterOptions&, CompletionHandler<void(RefPtr<::WebGPU::Adapter>&&)>&&) = 0;

    virtual RefPtr<::WebGPU::PresentationContext> createPresentationContext(const WebGPUPresentationContextDescriptor&) = 0;

    virtual RefPtr<WebGPUCompositorIntegration> createCompositorIntegration() = 0;
    virtual void paintToCanvas(NativeImage&, const IntSize&, GraphicsContext&) = 0;

    // The queue commands that take WebCore sources.
    virtual void copyExternalImageToTexture(::WebGPU::Queue&, const WebGPUExternalImageSource&, const WebGPUImageCopyTextureTagged& destination, const ::WebGPU::Extent3D& copySize) = 0;
    virtual RefPtr<NativeImage> nativeImage(::WebGPU::Queue&, VideoFrame&) = 0;
    // The device commands that take WebCore sources.
    virtual RefPtr<::WebGPU::ExternalTexture> importExternalTexture(::WebGPU::Device&, const WebGPUExternalTextureDescriptor&) = 0;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    virtual void updateExternalTexture(::WebGPU::Device&, const ::WebGPU::ExternalTexture&, const MediaPlayerIdentifier&) = 0;
#endif
    // Whether an object is valid where it is implemented, for tests.
    virtual bool isValid(const WebGPUCompositorIntegration&) const = 0;
    virtual bool isValid(const ::WebGPU::Buffer&) const = 0;
    virtual bool isValid(const ::WebGPU::Adapter&) const = 0;
    virtual bool isValid(const ::WebGPU::BindGroup&) const = 0;
    virtual bool isValid(const ::WebGPU::BindGroupLayout&) const = 0;
    virtual bool isValid(const ::WebGPU::CommandBuffer&) const = 0;
    virtual bool isValid(const ::WebGPU::CommandEncoder&) const = 0;
    virtual bool isValid(const ::WebGPU::ComputePassEncoder&) const = 0;
    virtual bool isValid(const ::WebGPU::ComputePipeline&) const = 0;
    virtual bool isValid(const ::WebGPU::Device&) const = 0;
    virtual bool isValid(const ::WebGPU::ExternalTexture&) const = 0;
    virtual bool isValid(const ::WebGPU::PipelineLayout&) const = 0;
    virtual bool isValid(const ::WebGPU::PresentationContext&) const = 0;
    virtual bool isValid(const ::WebGPU::QuerySet&) const = 0;
    virtual bool isValid(const ::WebGPU::Queue&) const = 0;
    virtual bool isValid(const ::WebGPU::RenderBundleEncoder&) const = 0;
    virtual bool isValid(const ::WebGPU::RenderBundle&) const = 0;
    virtual bool isValid(const ::WebGPU::RenderPassEncoder&) const = 0;
    virtual bool isValid(const ::WebGPU::RenderPipeline&) const = 0;
    virtual bool isValid(const ::WebGPU::Sampler&) const = 0;
    virtual bool isValid(const ::WebGPU::ShaderModule&) const = 0;
    virtual bool isValid(const ::WebGPU::Texture&) const = 0;
    virtual bool isValid(const ::WebGPU::TextureView&) const = 0;
    virtual bool isValid(const ::WebGPU::XRBinding&) const = 0;
    virtual bool isValid(const ::WebGPU::XRSubImage&) const = 0;
    virtual bool isValid(const ::WebGPU::XRProjectionLayer&) const = 0;
    virtual bool isValid(const ::WebGPU::XRView&) const = 0;

    virtual bool isRemoteGPUProxy() const { return false; }
    virtual bool isWebGPUIntegrationImpl() const { return false; }

protected:
    WebGPUIntegration() = default;

private:
    WebGPUIntegration(const WebGPUIntegration&) = delete;
    WebGPUIntegration(WebGPUIntegration&&) = delete;
    WebGPUIntegration& operator=(const WebGPUIntegration&) = delete;
    WebGPUIntegration& operator=(WebGPUIntegration&&) = delete;
};

} // namespace WebCore
