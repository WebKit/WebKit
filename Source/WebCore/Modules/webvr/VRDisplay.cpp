/*
 * Copyright (C) 2017-2018 Igalia S.L. All rights reserved.
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
#include "DOMWindow.h"
#include "EventNames.h"
#include "Page.h"
#include "ScriptedAnimationController.h"
#include "UserGestureIndicator.h"
#include "VRDisplayCapabilities.h"
#include "VRDisplayEvent.h"
#include "VREyeParameters.h"
#include "VRFrameData.h"
#include "VRLayerInit.h"
#include "VRPlatformDisplay.h"
#include "VRPose.h"
#include "VRStageParameters.h"

namespace WebCore {

Ref<VRDisplay> VRDisplay::create(ScriptExecutionContext& context, WeakPtr<VRPlatformDisplay>&& platformDisplay)
{
    return adoptRef(*new VRDisplay(context, WTFMove(platformDisplay)));
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

    m_display->setClient(this);
    suspendIfNeeded();
}

VRDisplay::~VRDisplay()
{
    m_display->setClient(nullptr);
}

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
        m_scriptedAnimationController = ScriptedAnimationController::create(*document);
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
        rejectRequestAndStopPresenting(NotSupportedError, "VRDisplay cannot present"_s);
        return;
    }

    if (!layers.size() || layers.size() > m_capabilities->maxLayers()) {
        rejectRequestAndStopPresenting(InvalidStateError, layers.size() ? "Too many layers"_s : "Not enough layers"_s);
        return;
    }

    if (!m_presentingLayer && !UserGestureIndicator::processingUserGesture()) {
        rejectRequestAndStopPresenting(InvalidAccessError, "Must request presentation from a user gesture handler."_s);
        return;
    }

    RELEASE_ASSERT(layers.size() == 1);
    auto layer = layers[0];

    if (!layer.source) {
        rejectRequestAndStopPresenting(InvalidStateError, "Layer does not contain any source"_s);
        return;
    }

    auto* canvasContext = layer.source->getContext("webgl");
    if (!canvasContext || !canvasContext->isWebGL()) {
        rejectRequestAndStopPresenting(NotSupportedError, "WebVR requires VRLayerInit with WebGL context."_s);
        return;
    }

    if ((layer.leftBounds.size() && layer.leftBounds.size() != 4)
        || (layer.rightBounds.size() && layer.rightBounds.size() != 4)) {
        rejectRequestAndStopPresenting(InvalidStateError, "Layer bounds must be either 0 or 4"_s);
        return;
    }

    m_presentingLayer = layer;
    promise->resolve();
}

void VRDisplay::stopPresenting()
{
    m_presentingLayer = WTF::nullopt;
}

void VRDisplay::exitPresent(Ref<DeferredPromise>&& promise)
{
    if (!m_presentingLayer) {
        promise->reject(Exception { InvalidStateError, "VRDisplay is not presenting"_s });
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

void VRDisplay::platformDisplayConnected()
{
    document()->domWindow()->dispatchEvent(VRDisplayEvent::create(eventNames().vrdisplayconnectEvent, makeRefPtr(this), WTF::nullopt));
}

void VRDisplay::platformDisplayDisconnected()
{
    document()->domWindow()->dispatchEvent(VRDisplayEvent::create(eventNames().vrdisplaydisconnectEvent, makeRefPtr(this), WTF::nullopt));
}

void VRDisplay::platformDisplayMounted()
{
    document()->domWindow()->dispatchEvent(VRDisplayEvent::create(eventNames().vrdisplayactivateEvent, makeRefPtr(this), VRDisplayEventReason::Mounted));
}

void VRDisplay::platformDisplayUnmounted()
{
    document()->domWindow()->dispatchEvent(VRDisplayEvent::create(eventNames().vrdisplaydeactivateEvent, makeRefPtr(this), VRDisplayEventReason::Unmounted));
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
