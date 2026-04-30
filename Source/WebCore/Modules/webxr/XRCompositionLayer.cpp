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

#include "config.h"
#include "XRCompositionLayer.h"

#if ENABLE(WEBXR_LAYERS)

#include "EventTargetInlines.h"
#include "WebGLOpaqueTexture.h"
#include "WebXRSession.h"
#include "XRLayerBacking.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(XRCompositionLayer);

XRCompositionLayer::XRCompositionLayer(ScriptExecutionContext* scriptExecutionContext, WebXRSession& session, Ref<XRLayerBacking>&& backing, const WebXRLayerInit& init)
    : WebXRLayer(scriptExecutionContext)
    , m_backing(WTF::move(backing))
    , m_init(init)
    , m_session(session)
{
}

XRCompositionLayer::~XRCompositionLayer() = default;

WebXRSession* XRCompositionLayer::session() const
{
    return m_session.get();
}

XRLayerBacking& XRCompositionLayer::backing()
{
    return m_backing;
}

void XRCompositionLayer::setColorTextures(Vector<RefPtr<WebGLOpaqueTexture>>&& colorTextures)
{
    m_colorTextures = WTF::move(colorTextures);
}

void XRCompositionLayer::setDepthStencilTextures(Vector<RefPtr<WebGLOpaqueTexture>>&& depthStencilTextures)
{
    m_depthStencilTextures = WTF::move(depthStencilTextures);
}

void XRCompositionLayer::fillInCommonDeviceLayerData(PlatformXR::DeviceLayer& data) const
{
    data.blendTextureSourceAlpha = m_blendTextureSourceAlpha;
    data.forceMonoPresentation = m_forceMonoPresentation;
}

} // namespace WebCore

#endif
