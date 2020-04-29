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

#include "WebXRRigidTransform.h"
#include "XREye.h"
#include <JavaScriptCore/Float32Array.h>
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class WebXRRigidTransform;

class WebXRView : public RefCounted<WebXRView> {
    WTF_MAKE_ISO_ALLOCATED(WebXRView);
public:
    static Ref<WebXRView> create();
    ~WebXRView();

    XREye eye() const { return m_eye; }
    const Float32Array& projectionMatrix() const { return *m_projectionMatrix; }
    const WebXRRigidTransform& transform() const { return *m_transform; }

    void setEye(XREye eye) { m_eye = eye; }
    void setProjectionMatrix(const Vector<float>&);
    void setTransform(RefPtr<WebXRRigidTransform>&& viewOffset) { m_transform = WTFMove(viewOffset); }

private:
    WebXRView();

    XREye m_eye;
    RefPtr<Float32Array> m_projectionMatrix;
    RefPtr<WebXRRigidTransform> m_transform;
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
