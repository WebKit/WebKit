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

#include "FakeXRBoundsPoint.h"
#include "FakeXRInputSourceInit.h"
#include "FakeXRViewInit.h"
#include "JSDOMPromiseDeferred.h"
#include "PlatformXR.h"
#include "WebFakeXRInputController.h"
#include "WebXRRigidTransform.h"
#include "WebXRView.h"
#include "XRVisibilityState.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {
class FakeXRView final : public RefCounted<FakeXRView> {
public:
    static Ref<FakeXRView> create(XREye eye) { return adoptRef(*new FakeXRView(eye)); }

    RefPtr<WebXRView> view() { return m_view; }
    void setResolution(FakeXRViewInit::DeviceResolution resolution) { m_resolution = resolution; }
    void setFieldOfView(FakeXRViewInit::FieldOfViewInit);

private:
    FakeXRView(XREye eye)
    {
        m_view = WebXRView::create();
        m_view->setEye(eye);
    }

    RefPtr<WebXRView> m_view;
    FakeXRViewInit::DeviceResolution m_resolution;
    FakeXRViewInit::FieldOfViewInit m_fov;
};

class SimulatedXRDevice final : public PlatformXR::Device {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SimulatedXRDevice() { m_supportsOrientationTracking = true; }
    void setNativeBoundsGeometry(Vector<FakeXRBoundsPoint> geometry) { m_nativeBoundsGeometry = geometry; }
    void setViewerOrigin(RefPtr<WebXRRigidTransform>&& origin) { m_viewerOrigin = WTFMove(origin); }
    void setFloorOrigin(RefPtr<WebXRRigidTransform>&& origin) { m_floorOrigin = WTFMove(origin); }
    void setEmulatedPosition(bool emulated) { m_emulatedPosition = emulated; }
    Vector<Ref<FakeXRView>>& views() { return m_views; }
private:
    Optional<Vector<FakeXRBoundsPoint>> m_nativeBoundsGeometry;
    RefPtr<WebXRRigidTransform> m_viewerOrigin;
    RefPtr<WebXRRigidTransform> m_floorOrigin;
    bool m_emulatedPosition { false };
    Vector<Ref<FakeXRView>> m_views;
};

class WebFakeXRDevice final : public RefCounted<WebFakeXRDevice> {
public:
    static Ref<WebFakeXRDevice> create() { return adoptRef(*new WebFakeXRDevice()); }

    void setViews(const Vector<FakeXRViewInit>&);

    void disconnect(DOMPromiseDeferred<void>&&);

    void setViewerOrigin(FakeXRRigidTransformInit origin, bool emulatedPosition = false);

    void clearViewerOrigin();

    void simulateVisibilityChange(XRVisibilityState);

    void setBoundsGeometry(Vector<FakeXRBoundsPoint> boundsCoordinates);

    void setFloorOrigin(FakeXRRigidTransformInit);

    void clearFloorOrigin();

    void simulateResetPose();

    Ref<WebFakeXRInputController> simulateInputSourceConnection(FakeXRInputSourceInit);

    static ExceptionOr<Ref<FakeXRView>> parseView(const FakeXRViewInit&);

    SimulatedXRDevice& simulatedXRDevice() { return m_device; }

private:
    WebFakeXRDevice();

    static ExceptionOr<Ref<WebXRRigidTransform>> parseRigidTransform(const FakeXRRigidTransformInit&);

    SimulatedXRDevice m_device;
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
