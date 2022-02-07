/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "XRDeviceIdentifier.h"
#include <WebCore/PlatformXR.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class PlatformXRSystemProxy;

struct XRDeviceInfo;

class XRDeviceProxy final : public PlatformXR::Device {
public:
    static Ref<XRDeviceProxy> create(XRDeviceInfo&&, PlatformXRSystemProxy&);
    XRDeviceIdentifier identifier() const { return m_identifier; }

    void sessionDidEnd();
    void updateSessionVisibilityState(PlatformXR::VisibilityState);

private:
    XRDeviceProxy(XRDeviceInfo&&, PlatformXRSystemProxy&);

    WebCore::IntSize recommendedResolution(PlatformXR::SessionMode) final { return m_recommendedResolution; }
    void initializeTrackingAndRendering(PlatformXR::SessionMode) final;
    void shutDownTrackingAndRendering() final;
    bool supportsSessionShutdownNotification() const final { return true; }
    void initializeReferenceSpace(PlatformXR::ReferenceSpaceType) final { }
    Vector<PlatformXR::Device::ViewData> views(PlatformXR::SessionMode) const final;
    void requestFrame(PlatformXR::Device::RequestFrameCallback&&) final;
    std::optional<PlatformXR::LayerHandle> createLayerProjection(uint32_t, uint32_t, bool) final;
    void deleteLayer(PlatformXR::LayerHandle) override { };
    void submitFrame(Vector<PlatformXR::Device::Layer>&&) final;

    XRDeviceIdentifier m_identifier;
    WeakPtr<PlatformXRSystemProxy> m_xrSystem;
    bool m_supportsStereoRendering { false };
    WebCore::IntSize m_recommendedResolution { 0, 0 };
};

} // namespace WebKit

#endif // ENABLE(WEBXR)
