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
#include "XRLayerLayout.h"
#include "XRLayerQuality.h"

#include <wtf/TZoneMalloc.h>

namespace WebCore {

class XRLayerBacking;

class XRCompositionLayer : public WebXRLayer {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(XRCompositionLayer);
public:
    virtual ~XRCompositionLayer();

    XRLayerLayout layout() const { return XRLayerLayout::Stereo; }

    bool blendTextureSourceAlpha() const { return false; }
    void setBlendTextureSourceAlpha(bool) { }

    bool forceMonoPresentation() const { return false; }
    void setForceMonoPresentation(bool) { }

    float opacity() const { return 1.f; }
    void setOpacity(float) { }

    uint32_t mipLevels() const { return 1; }

    XRLayerQuality quality() const { return XRLayerQuality::Default; }
    void setQuality(XRLayerQuality) { }

    bool needsRedraw() const { return true; }

    XRLayerBacking& backing();

    void destroy() { }
protected:
    explicit XRCompositionLayer(ScriptExecutionContext*, Ref<XRLayerBacking>&&);
    const Ref<XRLayerBacking> m_backing;

private:
    bool isXRCompositionLayer() const final { return true; }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::XRCompositionLayer)
    static bool isType(const WebCore::WebXRLayer& layer) { return layer.isXRCompositionLayer(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBXR_LAYERS)
