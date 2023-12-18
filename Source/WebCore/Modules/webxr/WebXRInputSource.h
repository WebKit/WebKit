/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 * Copyright (C) 2021 Apple, Inc. All rights reserved.
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

#include "PlatformXR.h"
#include "WebXRGamepad.h"
#include "WebXRInputSpace.h"
#include "XRHandedness.h"
#include "XRTargetRayMode.h"
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBXR_HANDS)
#include "WebXRHand.h"
#endif

namespace WebCore {

#if ENABLE(GAMEPAD)
class Gamepad;
#endif
class XRInputSourceEvent;
class WebXRHand;
class WebXRInputSpace;

class WebXRInputSource : public RefCounted<WebXRInputSource>, public CanMakeWeakPtr<WebXRInputSource> {
    WTF_MAKE_ISO_ALLOCATED(WebXRInputSource);
public:
    using InputSource = PlatformXR::FrameData::InputSource;
    using InputSourceButton = PlatformXR::FrameData::InputSourceButton;

    static Ref<WebXRInputSource> create(Document&, WebXRSession&, double timestamp, const InputSource&);
    ~WebXRInputSource();

    PlatformXR::InputSourceHandle handle() const { return m_source.handle; }
    XRHandedness handedness() const { return m_source.handeness; }
    XRTargetRayMode targetRayMode() const { return m_source.targetRayMode; };
    const WebXRSpace& targetRaySpace() const {return m_targetRaySpace.get(); };
    WebXRSpace* gripSpace() const { return m_gripSpace.get(); }
    const Vector<String>& profiles() const { return m_source.profiles; };
    double connectTime() const { return m_connectTime; }
#if ENABLE(GAMEPAD)
    const Gamepad* gamepad() const { return m_gamepad.ptr(); }
#endif

#if ENABLE(WEBXR_HANDS)
    const WebXRHand* hand() const { return m_hand.get(); }
#endif

    void update(double timestamp, const InputSource&);
    bool requiresInputSourceChange(const InputSource&);
    void disconnect();

    void pollEvents(Vector<Ref<XRInputSourceEvent>>&);

    // For GC reachablitiy.
    WebXRSession* session();

private:
    WebXRInputSource(Document&, WebXRSession&, double timestamp, const InputSource&);

    WeakPtr<WebXRSession> m_session;
    InputSource m_source;
    Ref<WebXRInputSpace> m_targetRaySpace;
    RefPtr<WebXRInputSpace> m_gripSpace;
    double m_connectTime { 0 };
    bool m_connected { true };
#if ENABLE(GAMEPAD)
    Ref<Gamepad> m_gamepad;
#endif

#if ENABLE(WEBXR_HANDS)
    RefPtr<WebXRHand> m_hand;
#endif

    bool m_selectStarted { false };
    bool m_squeezeStarted { false };
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
