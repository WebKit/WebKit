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
#include "WebXRRenderState.h"

#if ENABLE(WEBXR)

#include "WebXRWebGLLayer.h"
#include "XRRenderStateInit.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRRenderState);

Ref<WebXRRenderState> WebXRRenderState::create(const WebXRSession& session)
{
    // When an XRRenderState object is created for an XRSession session, the user agent MUST initialize the render state by running the following steps:
    //   1. Let state be the newly created XRRenderState object.

    //   2. Initialize state’s depthNear to 0.1.
    //   3. Initialize state’s depthFar to 1000.0.
    // (Default-initialized in the XRRenderState class definition.)

    //   4. If session is an immersive session, initialize state’s inlineVerticalFieldOfView to null.
    //   5. Else initialize state’s inlineVerticalFieldOfView to PI * 0.5.
    // FIXME: "immersive session" support
    UNUSED_PARAM(session);
    Optional<double> inlineVerticalFieldOfView { piOverTwoDouble };

    //   6. Initialize state’s baseLayer to null.
    //   7. Initialize state’s outputContext to null.
    // (Initialized to null by default.)

    return adoptRef(*new WebXRRenderState(WTFMove(inlineVerticalFieldOfView)));
}

WebXRRenderState::WebXRRenderState(Optional<double>&& inlineVerticalFieldOfView)
    : m_inlineVerticalFieldOfView(WTFMove(inlineVerticalFieldOfView))
{
}

WebXRRenderState::WebXRRenderState(const XRRenderStateInit&)
{
}

WebXRRenderState::~WebXRRenderState() = default;

double WebXRRenderState::depthNear() const
{
    return m_depth.near;
}

double WebXRRenderState::depthFar() const
{
    return m_depth.far;
}

Optional<double> WebXRRenderState::inlineVerticalFieldOfView() const
{
    return m_inlineVerticalFieldOfView;
}

RefPtr<WebXRWebGLLayer> WebXRRenderState::baseLayer() const
{
    return m_baseLayer;
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
