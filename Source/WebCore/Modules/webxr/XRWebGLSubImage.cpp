/*
 * Copyright (C) 2024 Apple, Inc. All rights reserved.
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
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

#include "config.h"
#include "XRWebGLSubImage.h"
#include "XRLayerBacking.h"

#if ENABLE(WEBXR_LAYERS)

#include "WebGLOpaqueTexture.h"
#include "WebXRSession.h"
#include "WebXRViewport.h"
#include "XRCompositionLayer.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(XRWebGLSubImage);

XRWebGLSubImage::~XRWebGLSubImage() = default;

XRWebGLSubImage::XRWebGLSubImage(Ref<WebXRViewport>&& viewport, WebGLTexture& colorTexture, IntSize colorTextureSize, WebGLTexture* depthStencilTexture, std::optional<IntSize> depthStencilTextureSize)
    : m_viewport(WTF::move(viewport))
    , m_colorTexture(colorTexture)
    , m_colorTextureSize(colorTextureSize)
    , m_depthStencilTexture(depthStencilTexture)
    , m_depthStencilTextureSize(WTF::move(depthStencilTextureSize))
{
}

ExceptionOr<Ref<XRWebGLSubImage>> XRWebGLSubImage::create(Ref<WebXRViewport>&& viewport, XRCompositionLayer& layer)
{
    auto colorTexture = layer.colorTextures()[0].get();
    if (!colorTexture || !colorTexture->isUsable())
        return Exception { ExceptionCode::InvalidStateError, "Cannot get a usable texture for the subimage."_s };

    IntSize colorTextureSize { static_cast<int>(layer.backing().colorTextureWidth()), static_cast<int>(layer.backing().colorTextureHeight()) };

    std::optional<IntSize> depthTextureSize;
    WebGLOpaqueTexture* depthTexture = nullptr;
    if (!layer.depthStencilTextures().isEmpty()) {
        depthTexture = layer.depthStencilTextures()[0].get();
        if (!depthTexture || !depthTexture->isUsable())
            depthTexture = nullptr;
        else
            depthTextureSize = IntSize { static_cast<int>(layer.backing().depthTextureWidth().value()), static_cast<int>(layer.backing().depthTextureHeight().value()) };
    }

    return adoptRef(*new XRWebGLSubImage(WTF::move(viewport), *colorTexture, colorTextureSize, depthTexture, depthTextureSize));
}

const WebGLTexture& XRWebGLSubImage::colorTexture() const
{
    return m_colorTexture.get();
}

RefPtr<WebGLTexture> XRWebGLSubImage::depthStencilTexture() const
{
    return m_depthStencilTexture;
}

std::optional<uint32_t> XRWebGLSubImage::depthStencilTextureWidth() const
{
    return !m_depthStencilTexture ? std::nullopt : std::make_optional<uint32_t>(m_viewport->width());
}

std::optional<uint32_t> XRWebGLSubImage::depthStencilTextureHeight() const
{
    return !m_depthStencilTexture ? std::nullopt : std::make_optional<uint32_t>(m_viewport->height());
}


} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
