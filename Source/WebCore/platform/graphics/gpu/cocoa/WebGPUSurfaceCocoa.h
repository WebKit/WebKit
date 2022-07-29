/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "IOSurface.h"
#include "WebGPUSurface.h"
#include "pal/graphics/WebGPU/WebGPUCanvasConfiguration.h"

namespace WebCore {

class WebGPUSurfaceCocoa final: public WebGPUSurface {
public:
    static RefPtr<WebGPUSurfaceCocoa> create(IntSize, PAL::WebGPU::CanvasConfiguration);

    ~WebGPUSurfaceCocoa() = default;

    // GraphicsLayerContentsDisplayDelegate overrides:
    void prepareToDelegateDisplay(PlatformCALayer&) override;
    void display(PlatformCALayer&) override;
    GraphicsLayer::CompositingCoordinatesOrientation orientation() const override;

    // WebGPUSurface overrides:
    Ref<PAL::WebGPU::Texture> currentTexture() override;
    PixelFormat pixelFormat() const override;
    DestinationColorSpace colorSpace() const override;
    void present() override;
    static GPUTextureFormat preferredTextureFormat()
    {
        return GPUTextureFormat::Bgra8unorm;
    }

private:
    WebGPUSurfaceCocoa(IntSize, PAL::WebGPU::CanvasConfiguration);

    IntSize m_size;
    PAL::WebGPU::CanvasConfiguration m_configuration;

    // Validate an IOSurface (not null, and its size matched the surface size)
    bool isValidBuffer(const IOSurface*);

    // Ensures that the back buffer can be used, and recreate them if necessary.
    void ensureValidBackBuffer();

    // Buffer that's currently used for presentation. The web process has access to
    // this buffer for presentation.
    std::unique_ptr<IOSurface> m_frontBuffer;

    // Buffer for drawing stuff. When it's time for presentation, this is swapped with
    // m_frontBuffer to become the front buffer.
    std::unique_ptr<IOSurface> m_backBuffer;
    RefPtr<PAL::WebGPU::Texture> m_backBufferTexture;

    // Standby secondary back buffer in case something else is using the primary back buffer
    // and we need a buffer now.
    std::unique_ptr<IOSurface> m_secondaryBackBuffer;
};

} // namespace WebCore
