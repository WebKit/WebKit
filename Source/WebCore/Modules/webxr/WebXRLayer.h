/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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

#if ENABLE(WEBXR)

#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "EventTargetInterfaces.h"
#include "PlatformXR.h"
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class ScriptExecutionContext;

class WebXRLayer : public RefCounted<WebXRLayer>, public EventTarget, public ContextDestructionObserver {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebXRLayer);
public:
    virtual ~WebXRLayer();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    virtual void startFrame(PlatformXR::FrameData&) = 0;
    virtual PlatformXR::Device::Layer endFrame() = 0;

    virtual bool isWebXRWebGLLayer() const { return false; }
    virtual bool isXRCompositionLayer() const { return false; }
    virtual bool isXRCubeLayer() const { return false; }
    virtual bool isXRCylinderLayer() const { return false; }
    virtual bool isXREquirectLayer() const { return false; }
    virtual bool isXRProjectionLayer() const { return false; }
    virtual bool isXRQuadLayer() const { return false; }

protected:
    explicit WebXRLayer(ScriptExecutionContext*);

    // EventTarget
    ScriptExecutionContext* scriptExecutionContext() const final;

private:
    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::WebXRLayer; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
