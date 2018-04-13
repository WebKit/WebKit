/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
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
#include "VRDisplay.h"

#include "CanvasRenderingContext.h"
#include "Chrome.h"
#include "DOMException.h"
#include "Page.h"
#include "ScriptedAnimationController.h"
#include "UserGestureIndicator.h"
#include "VRDisplayCapabilities.h"
#include "VREyeParameters.h"
#include "VRFrameData.h"
#include "VRLayerInit.h"
#include "VRPlatformDisplay.h"
#include "VRPose.h"
#include "VRStageParameters.h"

namespace WebCore {

Ref<VRDisplay> VRDisplay::create(ScriptExecutionContext& context, WeakPtr<VRPlatformDisplay>&& platformDisplay)
{
    auto display = adoptRef(*new VRDisplay(context, WTFMove(platformDisplay)));
    display->suspendIfNeeded();
    return display;
}

VRDisplay::VRDisplay(ScriptExecutionContext& context, WeakPtr<VRPlatformDisplay>&& platformDisplay)
    : ActiveDOMObject(&context)
    , m_display(WTFMove(platformDisplay))
{
    auto displayInfo = m_display->getDisplayInfo();
    m_capabilities = VRDisplayCapabilities::create(displayInfo.capabilityFlags());
    m_leftEyeParameters = VREyeParameters::create(displayInfo.eyeTranslation(VRPlatformDisplayInfo::EyeLeft), displayInfo.eyeFieldOfView(VRPlatformDisplayInfo::EyeLeft), displayInfo.renderSize());
    m_rightEyeParameters = VREyeParameters::create(displayInfo.eyeTranslation(VRPlatformDisplayInfo::EyeRight), displayInfo.eyeFieldOfView(VRPlatformDisplayInfo::EyeRight), displayInfo.renderSize());
    m_displayId = displayInfo.displayIdentifier();
    m_displayName = displayInfo.displayName();
}

VRDisplay::~VRDisplay() = default;

bool VRDisplay::isConnected() const
{
    if (!m_display)
        return false;

    return m_display->getDisplayInfo().isConnected();
}

const VRDisplayCapabilities& VRDisplay::capabilities() const
{
    return *m_capabilities;
}

RefPtr<VRStageParameters> VRDisplay::stageParameters() const
{
    auto displayInfo = m_display->getDisplayInfo();
    return VRStageParameters::create(displayInfo.sittingToStandingTransform(), displayInfo.playAreaBounds());
}

const VREyeParameters& VRDisplay::getEyeParameters(VREye eye) const
{
    return eye == VREye::Left ? *m_leftEyeParameters : *m_rightEyeParameters;
}

bool VRDisplay::getFrameData(VRFrameData& frameData) const
{
    if (!m_capabilities->hasPosition() || !m_capabilities->hasOrientation())
        return false;

    // FIXME: ensure that this is only called inside WebVR's rAF.
    frameData.update(m_display->getTrackingInfo(), getEyeParameters(VREye::Left), getEyeParameters(VREye::Right), m_depthNear, m_depthFar);
    return true;
}

Ref<VRPose> VRDisplay::getPose() const
{
    return VRPose::create(m_display->getTrackingInfo());
}

void VRDisplay::resetPose()
{
}

uint32_t VRDisplay::requestAnimationFrame(Ref<RequestAnimationFrameCallback>&& callback)
{
    if (!m_scriptedAnimationController) {
        auto* document = downcast<Document>(scriptExecutionContext());
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
        // FIXME: Get the display id of the HMD as it should use the HMD native refresh rate.
        PlatformDisplayID displayID = document->page() ? document->page()->chrome().displayID() : 0;
        m_scriptedAnimationController = ScriptedAnimationController::create(*document, displayID);
#else
        m_scriptedAnimationController = ScriptedAnimationController::create(*document, 0);
#endif
    }

    return m_scriptedAnimationController->registerCallback(WTFMove(callback));
}

void VRDisplay::cancelAnimationFrame(uint32_t id)
{
    if (!m_scriptedAnimationController)
        return;

    m_scriptedAnimationController->cancelCallback(id);
}

void VRDisplay::requestPresent(const Vector<VRLayerInit>& layers, Ref<DeferredPromise>&& promise)
{
    auto rejectRequestAndStopPresenting = [this, &promise] (ExceptionCode exceptionCode, ASCIILiteral message) {
        promise->reject(Exception { exceptionCode, message });
        if (m_presentingLayer)
            stopPresenting();
    };

    if (!m_capabilities->canPresent()) {
        rejectRequestAndStopPresenting(NotSupportedError, ASCIILiteral("VRDisplay cannot present"));
        return;
    }

    if (!layers.size() || layers.size() > m_capabilities->maxLayers()) {
        rejectRequestAndStopPresenting(InvalidStateError, ASCIILiteral(layers.size() ? "Too many layers" : "Not enough layers"));
        return;
    }

    if (!m_presentingLayer && !UserGestureIndicator::processingUserGesture()) {
        rejectRequestAndStopPresenting(InvalidAccessError, ASCIILiteral("Must request presentation from a user gesture handler."));
        return;
    }

    RELEASE_ASSERT(layers.size() == 1);
    auto layer = layers[0];

    if (!layer.source) {
        rejectRequestAndStopPresenting(InvalidStateError, ASCIILiteral("Layer does not contain any source"));
        return;
    }

    auto* canvasContext = layer.source->getContext("webgl");
    if (!canvasContext || !canvasContext->isWebGL()) {
        rejectRequestAndStopPresenting(NotSupportedError, ASCIILiteral("WebVR requires VRLayerInit with WebGL context."));
        return;
    }

    if ((layer.leftBounds.size() && layer.leftBounds.size() != 4)
        || (layer.rightBounds.size() && layer.rightBounds.size() != 4)) {
        rejectRequestAndStopPresenting(InvalidStateError, ASCIILiteral("Layer bounds must be either 0 or 4"));
        return;
    }

    m_presentingLayer = layer;
    promise->resolve();
}

void VRDisplay::stopPresenting()
{
    m_presentingLayer = std::nullopt;
}

void VRDisplay::exitPresent(Ref<DeferredPromise>&& promise)
{
    if (!m_presentingLayer) {
        promise->reject(Exception { InvalidStateError, ASCIILiteral("VRDisplay is not presenting") });
        return;
    }

    stopPresenting();
}

Vector<VRLayerInit> VRDisplay::getLayers() const
{
    Vector<VRLayerInit> layers;
    if (m_presentingLayer)
        layers.append(m_presentingLayer.value());
    return layers;
}

void VRDisplay::submitFrame()
{
}

bool VRDisplay::hasPendingActivity() const
{
    return false;
}

const char* VRDisplay::activeDOMObjectName() const
{
    return "VRDisplay";
}

bool VRDisplay::canSuspendForDocumentSuspension() const
{
    return false;
}

void VRDisplay::stop()
{
}

} // namespace WebCore
