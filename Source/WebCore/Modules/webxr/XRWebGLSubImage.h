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

#include "ExceptionOr.h"
#include "IntSize.h"
#include "XRSubImage.h"
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class WebGLTexture;
class XRCompositionLayer;

// https://immersive-web.github.io/layers/#xrwebglsubimagetype
class XRWebGLSubImage : public XRSubImage {
    WTF_MAKE_TZONE_ALLOCATED(XRWebGLSubImage);
public:
    static ExceptionOr<Ref<XRWebGLSubImage>> create(Ref<WebXRViewport>&&, XRCompositionLayer&);
    virtual ~XRWebGLSubImage();

    const WebXRViewport& viewport() const final { return m_viewport.get(); }
    const WebGLTexture& colorTexture() const;
    RefPtr<WebGLTexture> depthStencilTexture() const;
    RefPtr<WebGLTexture> motionVectorTexture() const { return nullptr; }

    std::optional<uint32_t> imageIndex() const { return m_imageIndex; }
    void setImageIndex(uint32_t imageIndex) { m_imageIndex = imageIndex; }

    uint32_t colorTextureWidth() const { return m_colorTextureSize.width(); }
    uint32_t colorTextureHeight() const { return m_colorTextureSize.height(); }
    std::optional<uint32_t> depthStencilTextureWidth() const;
    std::optional<uint32_t> depthStencilTextureHeight() const;
    std::optional<uint32_t> motionVectorTextureWidth() const { return std::nullopt; }
    std::optional<uint32_t> motionVectorTextureHeight() const { return std::nullopt; }

private:
    XRWebGLSubImage(Ref<WebXRViewport>&&, WebGLTexture& colorTexture, IntSize colorTextureSize, WebGLTexture* depthStencilTexture, std::optional<IntSize> depthStencilTextureSize);

    bool isXRWebGLSubImage() const final { return true; }

    Ref<WebXRViewport> m_viewport;

    Ref<WebGLTexture> m_colorTexture;
    IntSize m_colorTextureSize;

    RefPtr<WebGLTexture> m_depthStencilTexture;
    std::optional<IntSize> m_depthStencilTextureSize;

    std::optional<uint32_t> m_imageIndex;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::XRWebGLSubImage)
    static bool isType(const WebCore::XRSubImage& image) { return image.isXRWebGLSubImage(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBXR_LAYERS)
