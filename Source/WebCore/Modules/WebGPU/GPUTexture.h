/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include "GPUIntegralTypes.h"
#include "GPUTextureAspect.h"
#include "GPUTextureDimension.h"
#include "GPUTextureFormat.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <WebCore/WebGPUCppAPI.h>
#include <atomic>
#include <optional>
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GPUDevice;
class GPUTextureView;

struct GPUTextureDescriptor;
struct GPUTextureViewDescriptor;

template<typename> class ExceptionOr;

class GPUTexture : public RefCountedAndCanMakeWeakPtr<GPUTexture> {
public:
    static Ref<GPUTexture> create(Ref<::WebGPU::Texture>&& backing, const GPUTextureDescriptor& descriptor, GPUDevice& device, bool isCanvasBacking = false)
    {
        return adoptRef(*new GPUTexture(WTF::move(backing), descriptor, device, isCanvasBacking));
    }

    NODELETE String label() const;
    void setLabel(String&&);

    ExceptionOr<Ref<GPUTextureView>> createView(const std::optional<GPUTextureViewDescriptor>&) const;

    void destroy();
    bool isDestroyed() const { return m_isDestroyed; }

    ::WebGPU::Texture& backing() { return m_backing; }
    const ::WebGPU::Texture& backing() const { return m_backing; }
    GPUTextureFormat format() const { return m_format; }
    bool isCanvasBacking() const { return m_isCanvasBacking; }

    NODELETE GPUDevice* device() const;

    NODELETE GPUIntegerCoordinateOut width() const;
    NODELETE GPUIntegerCoordinateOut height() const;
    NODELETE GPUIntegerCoordinateOut depthOrArrayLayers() const;
    NODELETE GPUIntegerCoordinateOut mipLevelCount() const;
    NODELETE GPUSize32Out sampleCount() const;
    NODELETE GPUTextureDimension dimension() const;
    NODELETE GPUFlagsConstant usage() const;
    NODELETE JSC::JSValue textureBindingViewDimension() const;
    NODELETE size_t estimatedMemoryCost() const;

    // The GPU memory to tell the GC about, since it can't otherwise see it. Only a texture the device creates for the page has
    // one; a canvas or XR context owns its textures and makes new ones every frame. May be called from GC threads.
    size_t memoryCost() const { return m_memoryCost.load(std::memory_order_relaxed); }
    void setMemoryCost(size_t memoryCost) { m_memoryCost.store(memoryCost, std::memory_order_relaxed); }

    NODELETE static GPUTextureFormat aspectSpecificFormat(GPUTextureFormat, GPUTextureAspect);
    NODELETE static uint32_t texelBlockSize(GPUTextureFormat);
    NODELETE static uint32_t texelBlockWidth(GPUTextureFormat);
    NODELETE static uint32_t texelBlockHeight(GPUTextureFormat);

    virtual ~GPUTexture();

    NODELETE bool hasActiveInspectorCanvasCallTracer() const;

private:
    GPUTexture(Ref<::WebGPU::Texture>&&, const GPUTextureDescriptor&, GPUDevice&, bool isCanvasBacking);

    GPUTexture(const GPUTexture&) = delete;
    GPUTexture(GPUTexture&&) = delete;
    GPUTexture& operator=(const GPUTexture&) = delete;
    GPUTexture& operator=(GPUTexture&&) = delete;

    const Ref<::WebGPU::Texture> m_backing;
    String m_label;
    const GPUTextureFormat m_format;
    const GPUIntegerCoordinateOut m_width;
    const GPUIntegerCoordinateOut m_height;
    const GPUIntegerCoordinateOut m_depthOrArrayLayers;
    const GPUIntegerCoordinateOut m_mipLevelCount;
    const GPUSize32Out m_sampleCount;
    const GPUTextureDimension m_dimension;
    const GPUFlagsConstant m_usage;
    const Ref<GPUDevice> m_device;
    const bool m_isCanvasBacking;
    bool m_isDestroyed { false };
    std::atomic<size_t> m_memoryCost { 0 }; // Atomic, accessed from arbitrary thread by GC.
};

}
