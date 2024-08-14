/*
 * Copyright (C) 2024 Apple, Inc. All rights reserved.
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

#if ENABLE(WEBXR_LAYERS)

#include "XREye.h"

#include <ExceptionOr.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class WebGL2RenderingContext;
class WebGLRenderingContext;
class WebXRFrame;
class WebXRSession;
class WebXRView;
class XRCompositionLayer;
class XRCubeLayer;
class XRCylinderLayer;
class XREquirectLayer;
class XRProjectionLayer;
class XRQuadLayer;
class XRWebGLSubImage;

struct XRCubeLayerInit;
struct XRCylinderLayerInit;
struct XREquirectLayerInit;
struct XRProjectionLayerInit;
struct XRQuadLayerInit;

// https://immersive-web.github.io/layers/#XRWebGLBindingtype
class XRWebGLBinding : public RefCounted<XRWebGLBinding> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(XRWebGLBinding);
public:

    using WebXRWebGLRenderingContext = std::variant<
        RefPtr<WebGLRenderingContext>,
        RefPtr<WebGL2RenderingContext>
    >;

    static ExceptionOr<Ref<XRWebGLBinding>> create(Ref<WebXRSession>&&, WebXRWebGLRenderingContext&&);
    ~XRWebGLBinding() = default;

    double nativeProjectionScaleFactor() const { RELEASE_ASSERT_NOT_REACHED(); }
    bool usesDepthValues() const { RELEASE_ASSERT_NOT_REACHED(); }

    ExceptionOr<Ref<XRProjectionLayer>> createProjectionLayer(const XRProjectionLayerInit&) { RELEASE_ASSERT_NOT_REACHED(); }
    ExceptionOr<Ref<XRQuadLayer>> createQuadLayer(const XRQuadLayerInit&) { RELEASE_ASSERT_NOT_REACHED(); }
    ExceptionOr<Ref<XRCylinderLayer>> createCylinderLayer(const XRCylinderLayerInit&) { RELEASE_ASSERT_NOT_REACHED(); }
    ExceptionOr<Ref<XREquirectLayer>> createEquirectLayer(const XREquirectLayerInit&) { RELEASE_ASSERT_NOT_REACHED(); }
    ExceptionOr<Ref<XRCubeLayer>> createCubeLayer(const XRCubeLayerInit&) { RELEASE_ASSERT_NOT_REACHED(); }

    ExceptionOr<Ref<XRWebGLSubImage>> getSubImage(const XRCompositionLayer&, const WebXRFrame&, XREye) { RELEASE_ASSERT_NOT_REACHED(); }
    ExceptionOr<Ref<XRWebGLSubImage>> getViewSubImage(const XRProjectionLayer&, const WebXRView&) { RELEASE_ASSERT_NOT_REACHED(); }
};

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
