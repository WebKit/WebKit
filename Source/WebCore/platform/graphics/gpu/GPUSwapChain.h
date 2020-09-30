/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGPU)

#include "GPUTexture.h"
#include "PlatformLayer.h"
#include <wtf/OptionSet.h>
#include <wtf/RefPtr.h>

// PlatformLayer implementation needed otherwise compiling derived sources will fail.
#if USE(NICOSIA)
#include "NicosiaPlatformLayer.h"
#elif USE(COORDINATED_GRAPHICS)
#include "TextureMapperPlatformLayerProxyProvider.h"
#elif USE(TEXTURE_MAPPER)
#include "TextureMapperPlatformLayer.h"
#endif

#if USE(METAL)
#include <wtf/RetainPtr.h>
#endif

#if USE(METAL)
OBJC_CLASS CAMetalDrawable;
OBJC_CLASS WebGPULayer;
#endif

namespace WebCore {

class GPUDevice;

struct GPUSwapChainDescriptor;

enum class GPUTextureFormat;

#if USE(METAL)
using PlatformDrawableSmartPtr = RetainPtr<CAMetalDrawable>;
using PlatformSwapLayerSmartPtr = RetainPtr<WebGPULayer>;
#endif

class GPUSwapChain : public RefCounted<GPUSwapChain> {
public:
    static RefPtr<GPUSwapChain> tryCreate(const GPUSwapChainDescriptor&, int width, int height);

    RefPtr<GPUTexture> tryGetCurrentTexture();

#if USE(METAL)
    RetainPtr<CAMetalDrawable> takeDrawable();
#endif

    // For GPUCanvasContext.
    PlatformLayer* platformLayer() const;

    void present();
    void reshape(int width, int height);
    void destroy() { m_currentDrawable = nullptr; }

private:
    GPUSwapChain(PlatformSwapLayerSmartPtr&&, OptionSet<GPUTextureUsage::Flags>);

    PlatformSwapLayerSmartPtr m_platformSwapLayer;
    PlatformDrawableSmartPtr m_currentDrawable;
    OptionSet<GPUTextureUsage::Flags> m_usage;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)
