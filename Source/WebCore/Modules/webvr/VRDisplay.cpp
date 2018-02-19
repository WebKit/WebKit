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

#include "VRDisplayCapabilities.h"
#include "VREyeParameters.h"
#include "VRLayerInit.h"
#include "VRPlatformDisplay.h"
#include "VRPose.h"

namespace WebCore {

Ref<VRDisplay> VRDisplay::create(ScriptExecutionContext& context, WeakPtr<VRPlatformDisplay>&& platformDisplay)
{
    auto display = adoptRef(*new VRDisplay(context, WTFMove(platformDisplay)));
    display->suspendIfNeeded();
    return display;
}

VRDisplay::VRDisplay(ScriptExecutionContext& context, WeakPtr<VRPlatformDisplay>&& platformDisplay)
    : ActiveDOMObject(&context)
    , m_capabilities(VRDisplayCapabilities::create(platformDisplay->getDisplayInfo().capabilityFlags))
    , m_eyeParameters(VREyeParameters::create())
    , m_display(WTFMove(platformDisplay))
    , m_displayName(platformDisplay->getDisplayInfo().displayName)
{
}

VRDisplay::~VRDisplay() = default;

bool VRDisplay::isConnected() const
{
    if (!m_display)
        return false;

    return m_display->getDisplayInfo().isConnected;
}

bool VRDisplay::isPresenting() const
{
    return false;
}

const VRDisplayCapabilities& VRDisplay::capabilities() const
{
    return m_capabilities;
}

VRStageParameters* VRDisplay::stageParameters() const
{
    return nullptr;
}

const VREyeParameters& VRDisplay::getEyeParameters(VREye) const
{
    return m_eyeParameters;
}

unsigned VRDisplay::displayId() const
{
    return 0;
}

const String& VRDisplay::displayName() const
{
    return m_displayName;
}

bool VRDisplay::getFrameData(VRFrameData&) const
{
    return false;
}

Ref<VRPose> VRDisplay::getPose() const
{
    return VRPose::create();
}

void VRDisplay::resetPose()
{
}

double VRDisplay::depthNear() const
{
    return 0;
}

void VRDisplay::setDepthNear(double)
{
}

double VRDisplay::depthFar() const
{
    return 0;
}

void VRDisplay::setDepthFar(double)
{
}

long VRDisplay::requestAnimationFrame(Ref<RequestAnimationFrameCallback>&&)
{
    return 0;
}

void VRDisplay::cancelAnimationFrame(unsigned)
{
}

void VRDisplay::requestPresent(const Vector<VRLayerInit>&, Ref<DeferredPromise>&&)
{
}

void VRDisplay::exitPresent(Ref<DeferredPromise>&&)
{
}

const Vector<VRLayerInit>& VRDisplay::getLayers() const
{
    static auto mockLayers = makeNeverDestroyed(Vector<VRLayerInit> { });
    return mockLayers;
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
