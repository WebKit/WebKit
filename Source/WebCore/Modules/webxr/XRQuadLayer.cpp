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
#include "XRQuadLayer.h"

#if ENABLE(WEBXR_LAYERS)

#include "Logging.h"
#include "WebXRRigidTransform.h"
#include "WebXRSession.h"
#include "XRLayerBacking.h"
#include "XRWebGLBinding.h"
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(XRQuadLayer);

XRQuadLayer::XRQuadLayer(ScriptExecutionContext& scriptExecutionContext, WebXRSession& session, Ref<XRLayerBacking>&& backing, const XRQuadLayerInit& init)
    : XRCompositionLayer(&scriptExecutionContext, session, WTF::move(backing), init)
    , m_space(init.space)
    , m_transform((init.transform) ? init.transform : WebXRRigidTransform::create())
    , m_worldSize(FloatSize { init.width, init.height })
{
    setIsStatic(init.isStatic);
}

XRQuadLayer::~XRQuadLayer() = default;

const WebXRSpace& XRQuadLayer::space() const
{
    ASSERT(m_space);
    return *m_space;
}

void XRQuadLayer::setSpace(WebXRSpace& space)
{
    if (m_space == &space)
        return;

    m_space = space;
    setNeedsRedraw(true);
}

const WebXRRigidTransform& XRQuadLayer::transform() const
{
    ASSERT(m_transform);
    return *m_transform;
}

void XRQuadLayer::setTransform(WebXRRigidTransform& transform)
{
    if (m_transform == &transform)
        return;

    m_transform = transform;
    setNeedsRedraw(true);
}

void XRQuadLayer::startFrame(PlatformXR::FrameData& frameData)
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    auto it = frameData.layers.find(m_backing->handle());
    if (it == frameData.layers.end())
        return;

    if (needsRedraw())
        m_backing->startFrame(frameData);
#else
    UNUSED_PARAM(frameData);
#endif
}

void XRQuadLayer::recomputePose()
{
    auto scopeExit = makeScopeExit([&]() {
        RELEASE_LOG_ERROR(XR, "Failed to invert space transform, using identity transform for layer pose");
        m_poseInLocalSpace = PlatformXR::FrameData::Pose { .position = { 0, 0, 0 }, .orientation = { 0, 0, 0, 1 } };
    });

    std::optional<TransformationMatrix> spaceTransform;
    if (m_space)
        spaceTransform = m_space->nativeOrigin();
    if (!spaceTransform)
        spaceTransform = TransformationMatrix();
    else if (!spaceTransform->isInvertible())
        return;

    auto transformInLocalSpace = m_transform ? *spaceTransform->inverse() * m_transform->rawTransform() : *spaceTransform->inverse();
    TransformationMatrix::Decomposed4Type decomposed;
    if (!transformInLocalSpace.decompose4(decomposed))
        return;

    scopeExit.release();
    m_poseInLocalSpace.position = { static_cast<float>(decomposed.translateX), static_cast<float>(decomposed.translateY), static_cast<float>(decomposed.translateZ) };
    m_poseInLocalSpace.orientation = { static_cast<float>(decomposed.quaternion.x), static_cast<float>(decomposed.quaternion.y), static_cast<float>(decomposed.quaternion.z), static_cast<float>(decomposed.quaternion.w) };
}

PlatformXR::DeviceLayer XRQuadLayer::endFrame()
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    PlatformXR::DeviceLayer layerData;
    if (needsRedraw())
        m_backing->endFrame(layerData);

    layerData.handle = m_backing->handle();
    layerData.visible = true;

    if (needsRedraw())
        recomputePose();

    fillInCommonDeviceLayerData(layerData);

    layerData.quadLayerData = {
        .worldSize = m_worldSize,
        .poseInLocalSpace = m_poseInLocalSpace,
    };
    return layerData;
#else
    return PlatformXR::DeviceLayer { };
#endif
}

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
