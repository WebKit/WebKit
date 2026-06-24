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

#include "Logging.h"
#include "TransformationMatrix.h"
#include "WebGLOpaqueTexture.h"
#include "WebXRRigidTransform.h"
#include "WebXRSession.h"
#include "WebXRSpace.h"
#include "XRCompositionLayerPose.h"
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

XRCompositionLayer::XRCompositionLayer(ScriptExecutionContext* scriptExecutionContext, WebXRSession& session, Ref<XRLayerBacking>&& backing, const WebXRLayerInit& init, Ref<WebXRSpace> space, RefPtr<WebXRRigidTransform> transform)
    : WebXRLayer(scriptExecutionContext)
    , m_backing(WTF::move(backing))
    , m_init(init)
    , m_session(session)
    , m_space(WTF::move(space))
    , m_transform(transform ? WTF::move(transform) : RefPtr<WebXRRigidTransform> { WebXRRigidTransform::create() })
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

PlatformXR::LayerHandle XRCompositionLayer::layerHandle() const
{
    return m_backing->handle();
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

const WebXRSpace& XRCompositionLayer::space() const
{
    ASSERT(m_space);
    return *m_space;
}

void XRCompositionLayer::setSpace(WebXRSpace& space)
{
    if (m_space == &space)
        return;

    m_space = space;
    setNeedsRedraw(true);
}

const WebXRRigidTransform& XRCompositionLayer::transform() const
{
    ASSERT(m_transform);
    return *m_transform;
}

void XRCompositionLayer::setTransform(WebXRRigidTransform& transform)
{
    if (m_transform == &transform)
        return;

    m_transform = transform;
    setNeedsRedraw(true);
}

void XRCompositionLayer::startFrame(PlatformXR::FrameData& frameData)
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    auto it = frameData.layers.find(m_backing->handle());
    if (it == frameData.layers.end())
        return;

    m_backing->startFrame(frameData);
#else
    UNUSED_PARAM(frameData);
#endif
}

std::optional<PlatformXR::FrameData::Pose> computeXRCompositionLayerPose(const TransformationMatrix& spaceTransform, const TransformationMatrix* layerTransform)
{
    // nativeOrigin() returns the transform that maps from the layer's reference space into the session's local space.
    auto transformInLocalSpace = layerTransform ? spaceTransform * *layerTransform : spaceTransform;
    TransformationMatrix::Decomposed4Type decomposed;
    if (!transformInLocalSpace.decompose4(decomposed))
        return std::nullopt;

    return PlatformXR::FrameData::Pose {
        .position = { static_cast<float>(decomposed.translateX), static_cast<float>(decomposed.translateY), static_cast<float>(decomposed.translateZ) },
        .orientation = { static_cast<float>(decomposed.quaternion.x), static_cast<float>(decomposed.quaternion.y), static_cast<float>(decomposed.quaternion.z), static_cast<float>(decomposed.quaternion.w) },
    };
}

void XRCompositionLayer::recomputePose()
{
    std::optional<TransformationMatrix> spaceTransform;
    if (m_space)
        spaceTransform = m_space->nativeOrigin();
    if (!spaceTransform)
        spaceTransform = TransformationMatrix();

    auto pose = computeXRCompositionLayerPose(*spaceTransform, m_transform ? &m_transform->rawTransform() : nullptr);
    if (!pose) {
        RELEASE_LOG_ERROR(XR, "Failed to decompose space transform, using identity transform for layer pose");
        m_poseInLocalSpace = PlatformXR::FrameData::Pose { .position = { 0, 0, 0 }, .orientation = { 0, 0, 0, 1 } };
        return;
    }
    m_poseInLocalSpace = *pose;
}

PlatformXR::DeviceLayer XRCompositionLayer::endFrame()
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    PlatformXR::DeviceLayer layerData;
    m_backing->endFrame(layerData);

    layerData.handle = m_backing->handle();
    layerData.visible = true;

    recomputePose();

    fillInCommonDeviceLayerData(layerData);
    fillInTypeSpecificDeviceLayerData(layerData);
    return layerData;
#else
    return PlatformXR::DeviceLayer { };
#endif
}

} // namespace WebCore

#endif
