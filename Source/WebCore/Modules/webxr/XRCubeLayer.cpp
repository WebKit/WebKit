
/*
 * Copyright (C) 2025 Apple, Inc. All rights reserved.
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
#include "XRCubeLayer.h"

#if ENABLE(WEBXR_LAYERS)

#include "WebXRRigidTransform.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(XRCubeLayer);

XRCubeLayer::XRCubeLayer(ScriptExecutionContext& scriptExecutionContext, WebXRSession& session, Ref<XRLayerBacking>&& backing, const XRCubeLayerInit& init)
    : XRCompositionLayer(&scriptExecutionContext, session, WTF::move(backing), init, init.space, nullptr)
    , m_orientation(init.orientation ? Ref<DOMPointReadOnly>(*init.orientation) : DOMPointReadOnly::create(0, 0, 0, 1))
{
    setIsStatic(init.isStatic);
}

XRCubeLayer::~XRCubeLayer() = default;

void XRCubeLayer::setOrientation(DOMPointReadOnly& orientation)
{
    m_orientation = orientation;
    setNeedsRedraw(true);
}

void XRCubeLayer::fillInTypeSpecificDeviceLayerData(PlatformXR::DeviceLayer& layerData) const
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    layerData.cubeLayerData = {
        .orientation = {
            static_cast<float>(m_orientation->x()),
            static_cast<float>(m_orientation->y()),
            static_cast<float>(m_orientation->z()),
            static_cast<float>(m_orientation->w()),
        },
    };
#else
    UNUSED_PARAM(layerData);
#endif
}

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
