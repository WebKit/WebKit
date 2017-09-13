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

#pragma once

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "JSDOMPromiseDeferred.h"
#include "VREye.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class RequestAnimationFrameCallback;
class VRDisplayCapabilities;
class VREyeParameters;
class VRFrameData;
class VRPose;
class VRStageParameters;
struct VRLayerInit;

class VRDisplay : public RefCounted<VRDisplay>, public EventTargetWithInlineData, public ActiveDOMObject {
public:
    static Ref<VRDisplay> create(ScriptExecutionContext&);

    virtual ~VRDisplay();

    using RefCounted<VRDisplay>::ref;
    using RefCounted<VRDisplay>::deref;

    bool isConnected() const;
    bool isPresenting() const;

    const VRDisplayCapabilities& capabilities() const;
    VRStageParameters* stageParameters() const;

    const VREyeParameters& getEyeParameters(VREye) const;

    unsigned displayId() const;
    const String& displayName() const;

    bool getFrameData(VRFrameData&) const;

    Ref<VRPose> getPose() const;
    void resetPose();

    double depthNear() const;
    void setDepthNear(double);
    double depthFar() const;
    void setDepthFar(double);

    long requestAnimationFrame(Ref<RequestAnimationFrameCallback>&&);
    void cancelAnimationFrame(unsigned);

    void requestPresent(const Vector<VRLayerInit>&, Ref<DeferredPromise>&&);
    void exitPresent(Ref<DeferredPromise>&&);

    const Vector<VRLayerInit>& getLayers() const;

    void submitFrame();

private:
    VRDisplay(ScriptExecutionContext&);

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

    Ref<VRDisplayCapabilities> m_capabilities;
    Ref<VREyeParameters> m_eyeParameters;
};

} // namespace WebCore
