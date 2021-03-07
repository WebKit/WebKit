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

#include "config.h"
#include "WebXRView.h"

#if ENABLE(WEBXR)

#include "WebXRFrame.h"
#include "WebXRRigidTransform.h"
#include <JavaScriptCore/TypedArrayInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

// Arbitrary value for minimum viewport scaling.
// Below this threshold the resulting viewport would be too pixelated.
static constexpr double kMinViewportScale = 0.1;

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRView);

Ref<WebXRView> WebXRView::create(Ref<WebXRFrame>&& frame, XREye eye, Ref<WebXRRigidTransform>&& transform, Ref<Float32Array>&& projection)
{
    return adoptRef(*new WebXRView(WTFMove(frame), eye, WTFMove(transform), WTFMove(projection)));
}

WebXRView::WebXRView(Ref<WebXRFrame>&& frame, XREye eye, Ref<WebXRRigidTransform>&& transform, Ref<Float32Array>&& projection)
    : m_frame(WTFMove(frame))
    , m_eye(eye)
    , m_transform(WTFMove(transform))
    , m_projection(WTFMove(projection))
{
}

WebXRView::~WebXRView() = default;

// https://immersive-web.github.io/webxr/#dom-xrview-recommendedviewportscale
Optional<double> WebXRView::recommendedViewportScale() const
{
    // Return null if the system does not implement a heuristic or method for determining a recommended scale.
    return WTF::nullopt;
}

// https://immersive-web.github.io/webxr/#dom-xrview-requestviewportscale
void WebXRView::requestViewportScale(Optional<double> value)
{
    if (!value || *value <= 0.0)
        return;
    m_requestedViewportScale = std::clamp(*value, kMinViewportScale, 1.0);
}


} // namespace WebCore

#endif // ENABLE(WEBXR)
