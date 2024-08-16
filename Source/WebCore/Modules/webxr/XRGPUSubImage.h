/*
 * Copyright (C) 2024 Apple, Inc. All rights reserved.
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

#if ENABLE(WEBXR_LAYERS)

#include "GPUTexture.h"
#include "GPUTextureViewDescriptor.h"
#include "XRSubImage.h"

#include <wtf/TZoneMalloc.h>

namespace WebCore {

class GPUTexture;

// https://github.com/immersive-web/WebXR-WebGPU-Binding/blob/main/explainer.md
class XRGPUSubImage : public XRSubImage {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(XRGPUSubImage);
public:
    static Ref<XRGPUSubImage> create(Ref<WebGPU::XRSubImage>&& backing)
    {
        return adoptRef(*new XRGPUSubImage(WTFMove(backing)));
    }

    Ref<GPUTexture> colorTexture() const { RELEASE_ASSERT_NOT_REACHED(); }
    RefPtr<GPUTexture> depthStencilTexture() const { RELEASE_ASSERT_NOT_REACHED(); }
    RefPtr<GPUTexture> motionVectorTexture() const { RELEASE_ASSERT_NOT_REACHED(); }

    // const GPUTextureViewDescriptor& viewDescriptor() const { return m_viewDescriptor; }
private:
    XRGPUSubImage(Ref<WebGPU::XRSubImage>&&);

    Ref<WebGPU::XRSubImage> m_backing;
    GPUTextureViewDescriptor m_viewDescriptor;
};

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
