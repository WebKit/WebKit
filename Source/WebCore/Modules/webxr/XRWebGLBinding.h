/*
 * Copyright (C) 2024 Apple, Inc. All rights reserved.
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
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

#include "ExceptionOr.h"
#include "XREye.h"
#include "XRLayerInit.h"
#include "XRLayerLayout.h"
#include "XRProjectionLayerInit.h"
#include "XRTextureType.h"

#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class ScriptExecutionContext;
class WebGL2RenderingContext;
class WebGLOpaqueTexture;
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
struct XRQuadLayerInit;

// https://immersive-web.github.io/layers/#XRWebGLBindingtype
class XRWebGLBinding : public RefCounted<XRWebGLBinding> {
    WTF_MAKE_TZONE_ALLOCATED(XRWebGLBinding);
public:
    using WebXRWebGLRenderingContext = Variant<
        Ref<WebGLRenderingContext>,
        Ref<WebGL2RenderingContext>
    >;

    static ExceptionOr<Ref<XRWebGLBinding>> create(Ref<WebXRSession>&&, WebXRWebGLRenderingContext&&);

    double nativeProjectionScaleFactor() const { RELEASE_ASSERT_NOT_REACHED(); }
    bool usesDepthValues() const { RELEASE_ASSERT_NOT_REACHED(); }

    ExceptionOr<Ref<XRProjectionLayer>> createProjectionLayer(ScriptExecutionContext&, const XRProjectionLayerInit&);
    ExceptionOr<Ref<XRQuadLayer>> createQuadLayer(ScriptExecutionContext&, const XRQuadLayerInit&);
    ExceptionOr<Ref<XRCylinderLayer>> createCylinderLayer(const XRCylinderLayerInit&) { RELEASE_ASSERT_NOT_REACHED(); }
    ExceptionOr<Ref<XREquirectLayer>> createEquirectLayer(const XREquirectLayerInit&) { RELEASE_ASSERT_NOT_REACHED(); }
    ExceptionOr<Ref<XRCubeLayer>> createCubeLayer(const XRCubeLayerInit&) { RELEASE_ASSERT_NOT_REACHED(); }

    Ref<WebXRViewport> initializeViewport(IntSize, XRLayerLayout, int offset, int num);

    ExceptionOr<Ref<XRWebGLSubImage>> getSubImage(XRCompositionLayer&, const WebXRFrame&, XREye);
    ExceptionOr<Ref<XRWebGLSubImage>> getViewSubImage(XRProjectionLayer&, const WebXRView&);

private:
    XRWebGLBinding(Ref<WebXRSession>&&, WebXRWebGLRenderingContext&&);

    void initializeCompositionLayer(XRCompositionLayer&);
    ExceptionOr<XRLayerLayout> determineLayout(XRTextureType, XRLayerLayout);
    bool colorFormatIsSupportedForProjectionLayer(GCGLenum) const;
    bool depthFormatIsSupportedForProjectionLayer(GCGLenum) const;
    bool colorFormatIsSupportedForNonProjectionLayer(GCGLenum) const;
    bool depthFormatIsSupportedForNonProjectionLayer(GCGLenum) const;
    ExceptionOr<Vector<RefPtr<WebGLOpaqueTexture>>> allocateColorTexturesForProjectionLayer(XRProjectionLayer&, XRTextureType, GCGLenum textureFormat, double scaleFactor);
    ExceptionOr<Vector<RefPtr<WebGLOpaqueTexture>>> allocateDepthTexturesForProjectionLayer(XRProjectionLayer&, XRTextureType, GCGLenum textureFormat, double scaleFactor);
    ExceptionOr<Vector<RefPtr<WebGLOpaqueTexture>>> allocateColorTexturesForLayer(XRCompositionLayer&, XRTextureType, const XRLayerInit&);
    ExceptionOr<Vector<RefPtr<WebGLOpaqueTexture>>> allocateDepthTexturesForLayer(XRCompositionLayer&, XRTextureType, const XRLayerInit&);
    bool validateXRWebGLSubImageCreation(const XRCompositionLayer&, const WebXRFrame&) const;

    IntRect rectForView(const XRProjectionLayer&, const WebXRView&) const;

    const Ref<WebXRSession> m_session;
    WebXRWebGLRenderingContext m_context;
};

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
