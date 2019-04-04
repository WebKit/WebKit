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

#pragma once

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "JSDOMPromiseDeferred.h"
#include "VREye.h"
#include "VRLayerInit.h"
#include "VRPlatformDisplayClient.h"
#include <wtf/RefCounted.h>

namespace WebCore {

enum ExceptionCode;
class RequestAnimationFrameCallback;
class ScriptedAnimationController;
class VRDisplayCapabilities;
class VREyeParameters;
class VRFrameData;
class VRPlatformDisplay;
class VRPose;
class VRStageParameters;

class VRDisplay final : public RefCounted<VRDisplay>, public VRPlatformDisplayClient, public EventTargetWithInlineData, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(VRDisplay);
public:
    static Ref<VRDisplay> create(ScriptExecutionContext&, WeakPtr<VRPlatformDisplay>&&);

    virtual ~VRDisplay();

    using RefCounted<VRDisplay>::ref;
    using RefCounted<VRDisplay>::deref;

    bool isConnected() const;
    bool isPresenting() const { return !!m_presentingLayer; };

    const VRDisplayCapabilities& capabilities() const;
    RefPtr<VRStageParameters> stageParameters() const;

    const VREyeParameters& getEyeParameters(VREye) const;

    const String& displayName() const { return m_displayName; }
    uint32_t displayId() const { return m_displayId; }

    bool getFrameData(VRFrameData&) const;

    Ref<VRPose> getPose() const;
    void resetPose();

    double depthNear() const { return m_depthNear; }
    void setDepthNear(double depthNear) { m_depthNear = depthNear; }
    double depthFar() const { return m_depthFar; }
    void setDepthFar(double depthFar) { m_depthFar = depthFar; }

    uint32_t requestAnimationFrame(Ref<RequestAnimationFrameCallback>&&);
    void cancelAnimationFrame(uint32_t);

    void requestPresent(const Vector<VRLayerInit>&, Ref<DeferredPromise>&&);
    void exitPresent(Ref<DeferredPromise>&&);

    Vector<VRLayerInit> getLayers() const;

    void submitFrame();

    // VRPlatformDisplayClient
    void platformDisplayConnected() override;
    void platformDisplayDisconnected() override;
    void platformDisplayMounted() override;
    void platformDisplayUnmounted() override;

private:
    VRDisplay(ScriptExecutionContext&, WeakPtr<VRPlatformDisplay>&&);

    // EventTarget
    EventTargetInterface eventTargetInterface() const override { return VRDisplayEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    // ActiveDOMObject
    bool hasPendingActivity() const override;
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;
    void stop() override;

    void stopPresenting();

    Document* document() { return downcast<Document>(scriptExecutionContext()); }

    WeakPtr<VRPlatformDisplay> m_display;

    RefPtr<VRDisplayCapabilities> m_capabilities;
    // We could likely store just one of the two following ones as the values should be identical
    // (except the sign of the eye to head transform offset).
    RefPtr<VREyeParameters> m_leftEyeParameters;
    RefPtr<VREyeParameters> m_rightEyeParameters;
    RefPtr<VRStageParameters> m_stageParameters;

    String m_displayName;
    uint32_t m_displayId;

    double m_depthNear { 0.01 }; // Default value from the specs.
    double m_depthFar { 10000 }; // Default value from the specs.

    RefPtr<ScriptedAnimationController> m_scriptedAnimationController;

    Optional<VRLayerInit> m_presentingLayer;
};

} // namespace WebCore
