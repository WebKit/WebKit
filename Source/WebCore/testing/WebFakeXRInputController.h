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

#include "FakeXRButtonStateInit.h"
#include "FakeXRInputSourceInit.h"
#include "FakeXRRigidTransformInit.h"
#include "PlatformXR.h"
#include "XRHandedness.h"
#include "XRTargetRayMode.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class WebFakeXRInputController final : public RefCounted<WebFakeXRInputController> {
public:
    static Ref<WebFakeXRInputController> create(PlatformXR::InputSourceHandle, const FakeXRInputSourceInit&);

    void setHandedness(XRHandedness handeness) { m_handeness = handeness; }
    void setTargetRayMode(XRTargetRayMode mode) { m_targetRayMode = mode; }
    void setProfiles(Vector<String>&& profiles) { m_profiles = WTFMove(profiles); }
    void setGripOrigin(FakeXRRigidTransformInit gripOrigin, bool emulatedPosition = false);
    void clearGripOrigin() { m_gripOrigin = std::nullopt; }
    void setPointerOrigin(FakeXRRigidTransformInit pointerOrigin, bool emulatedPosition = false);
    void disconnect();
    void reconnect();
    void startSelection() { m_primarySelected = true; }
    void endSelection() { m_primarySelected = false; }
    void simulateSelect() { m_simulateSelect = true; }
    void setSupportedButtons(const Vector<FakeXRButtonStateInit>&);
    void updateButtonState(const FakeXRButtonStateInit&);
    bool isConnected() const { return m_connected; }

    PlatformXR::Device::FrameData::InputSource getFrameData();

private:
    WebFakeXRInputController(PlatformXR::InputSourceHandle, const FakeXRInputSourceInit&);

    struct ButtonOrPlaceholder {
        std::optional<PlatformXR::Device::FrameData::InputSourceButton> button;
        std::optional<Vector<float>> axes;
    };
    ButtonOrPlaceholder getButtonOrPlaceholder(FakeXRButtonStateInit::Type) const;

    PlatformXR::InputSourceHandle m_handle { 0 };
    XRHandedness m_handeness { XRHandedness::None };
    XRTargetRayMode m_targetRayMode { XRTargetRayMode::Gaze };
    Vector<String> m_profiles;
    PlatformXR::Device::FrameData::InputSourcePose m_pointerOrigin;
    std::optional<PlatformXR::Device::FrameData::InputSourcePose> m_gripOrigin;
    HashMap<FakeXRButtonStateInit::Type, FakeXRButtonStateInit, WTF::IntHash<FakeXRButtonStateInit::Type>, WTF::StrongEnumHashTraits<FakeXRButtonStateInit::Type>> m_buttons;
    bool m_connected { true };
    bool m_primarySelected { false };
    bool m_simulateSelect { false };
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
