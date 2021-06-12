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

#include "JSValueInWrappedObject.h"
#include "WebXRRigidTransform.h"
#include "XREye.h"
#include <JavaScriptCore/Float32Array.h>
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class WebXRFrame;
class WebXRRigidTransform;
class WebXRSession;

class WebXRView : public RefCounted<WebXRView> {
    WTF_MAKE_ISO_ALLOCATED_EXPORT(WebXRView, WEBCORE_EXPORT);
public:
    WEBCORE_EXPORT static Ref<WebXRView> create(Ref<WebXRFrame>&&, XREye, Ref<WebXRRigidTransform>&&, Ref<Float32Array>&&);
    WEBCORE_EXPORT ~WebXRView();

    const WebXRFrame& frame() const { return m_frame.get(); }
    XREye eye() const { return m_eye; }
    const Float32Array& projectionMatrix() const { return m_projection.get(); }
    const WebXRRigidTransform& transform() const { return m_transform.get(); }

    std::optional<double> recommendedViewportScale() const;
    void requestViewportScale(std::optional<double>);

    double requestedViewportScale() const { return m_requestedViewportScale; }
    bool isViewportModifiable() const { return m_viewportModifiable; }
    void setViewportModifiable(bool modifiable) { m_viewportModifiable = modifiable; }

    JSValueInWrappedObject& cachedProjectionMatrix() { return m_cachedProjectionMatrix; }

private:
    WebXRView(Ref<WebXRFrame>&&, XREye, Ref<WebXRRigidTransform>&&, Ref<Float32Array>&&);

    Ref<WebXRFrame> m_frame;
    XREye m_eye;
    Ref<WebXRRigidTransform> m_transform;
    Ref<Float32Array> m_projection;
    bool m_viewportModifiable { false };
    double m_requestedViewportScale { 1.0 };
    JSValueInWrappedObject m_cachedProjectionMatrix;
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
