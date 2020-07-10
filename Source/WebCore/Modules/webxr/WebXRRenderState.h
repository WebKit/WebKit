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

#include "XRSessionMode.h"
#include <wtf/IsoMalloc.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class HTMLCanvasElement;
class WebXRWebGLLayer;
struct XRRenderStateInit;

class WebXRRenderState : public RefCounted<WebXRRenderState> {
    WTF_MAKE_ISO_ALLOCATED(WebXRRenderState);
public:
    static Ref<WebXRRenderState> create(XRSessionMode);
    ~WebXRRenderState();

    double depthNear() const;
    double depthFar() const;
    Optional<double> inlineVerticalFieldOfView() const;
    RefPtr<WebXRWebGLLayer> baseLayer() const;
    HTMLCanvasElement* outputCanvas() const;

private:
    explicit WebXRRenderState(Optional<double>&& fieldOfView);
    explicit WebXRRenderState(const XRRenderStateInit&);

    // https://immersive-web.github.io/webxr/#initialize-the-render-state
    struct {
        double near { 0.1 }; // in meters
        double far { 1000 }; // in meters
    } m_depth;
    Optional<double> m_inlineVerticalFieldOfView; // in radians
    RefPtr<WebXRWebGLLayer> m_baseLayer;
    WeakPtr<HTMLCanvasElement> m_outputCanvas;
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
