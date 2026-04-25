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

#include "WebXRLayer.h"
#include "XRLayerInit.h"
#include "XRLayerLayout.h"
#include "XRLayerQuality.h"
#include "XRProjectionLayerInit.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class WebGLOpaqueTexture;
class WebXRSession;
class WebXRSpace;
class XRLayerBacking;

class XRCompositionLayer : public WebXRLayer {
    WTF_MAKE_TZONE_ALLOCATED(XRCompositionLayer);

public:
    virtual ~XRCompositionLayer();

    using WebXRLayerInit = Variant<
        XRLayerInit,
        XRProjectionLayerInit
    >;
    const WebXRLayerInit& init() const { return m_init; }

    XRLayerLayout layout() const { return m_layout; }
    void setLayout(XRLayerLayout layout) { m_layout = layout; }

    bool blendTextureSourceAlpha() const { return m_blendTextureSourceAlpha; }
    void setBlendTextureSourceAlpha(bool blendTextureSourceAlpha) { m_blendTextureSourceAlpha = blendTextureSourceAlpha; }

    bool forceMonoPresentation() const { return m_forceMonoPresentation; }
    void setForceMonoPresentation(bool forceMonoPresentation) { m_forceMonoPresentation = forceMonoPresentation; }

    float opacity() const { return 1.f; }
    void setOpacity(float) { }

    uint32_t mipLevels() const { return 1; }

    XRLayerQuality quality() const { return XRLayerQuality::Default; }
    void setQuality(XRLayerQuality) { }

    bool needsRedraw() const { return m_needsRedraw; }
    void setNeedsRedraw(bool needsRedraw) { m_needsRedraw = needsRedraw; }

    bool isStatic() const { return m_isStatic; }
    void setIsStatic(bool isStatic) { m_isStatic = isStatic; }

    XRLayerBacking& backing();
    WebXRSession* session() const;

    void destroy() { }

    const Vector<RefPtr<WebGLOpaqueTexture>>& colorTextures() const { return m_colorTextures; }
    void setColorTextures(Vector<RefPtr<WebGLOpaqueTexture>>&&);

    const Vector<RefPtr<WebGLOpaqueTexture>>& depthStencilTextures() const { return m_depthStencilTextures; }
    void setDepthStencilTextures(Vector<RefPtr<WebGLOpaqueTexture>>&&);

protected:
    explicit XRCompositionLayer(ScriptExecutionContext*, WebXRSession&, Ref<XRLayerBacking>&&, const WebXRLayerInit&);

    void fillInCommonDeviceLayerData(PlatformXR::DeviceLayer&) const;

    const Ref<XRLayerBacking> m_backing;
    const WebXRLayerInit m_init;

private:
    bool isXRCompositionLayer() const final { return true; }

    WeakPtr<WebXRSession> m_session;

    bool m_isStatic { false };
    bool m_needsRedraw { true };
    XRLayerLayout m_layout { XRLayerLayout::Stereo };
    bool m_blendTextureSourceAlpha { false };
    bool m_forceMonoPresentation { false };

    Vector<RefPtr<WebGLOpaqueTexture>> m_colorTextures;
    Vector<RefPtr<WebGLOpaqueTexture>> m_depthStencilTextures;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::XRCompositionLayer)
    static bool isType(const WebCore::WebXRLayer& layer) { return layer.isXRCompositionLayer(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBXR_LAYERS)
