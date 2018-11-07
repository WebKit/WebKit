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

#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

#if USE(METAL)
OBJC_CLASS CAMetalLayer;
#endif

namespace WebCore {

class GPUDevice;

#if USE(METAL)
using PlatformSwapLayer = CAMetalLayer;
using PlatformSwapLayerSmartPtr = RetainPtr<CAMetalLayer>;
#else
using PlatformSwapLayer = void;
using PlatformSwapLayerSmartPtr = RefPtr<void>;
#endif

class GPUSwapChain : public RefCounted<GPUSwapChain> {
public:
    static RefPtr<GPUSwapChain> create();

    void setDevice(const GPUDevice&);
    void reshape(int width, int height);
    void present();

    PlatformSwapLayer *platformLayer() const { return m_platformSwapLayer.get(); }

private:
    GPUSwapChain(PlatformSwapLayerSmartPtr&&);

    PlatformSwapLayerSmartPtr m_platformSwapLayer;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)
