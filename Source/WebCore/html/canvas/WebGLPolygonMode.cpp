/*
 * Copyright 2023 The Chromium Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBGL)
#include "WebGLPolygonMode.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebGLPolygonMode);

WebGLPolygonMode::WebGLPolygonMode(WebGLRenderingContextBase& context)
    : WebGLExtension(context, WebGLExtensionName::WebGLPolygonMode)
{
    context.protectedGraphicsContextGL()->ensureExtensionEnabled("GL_ANGLE_polygon_mode"_s);
    context.printToConsole(MessageLevel::Warning, "WebGL: non-portable extension enabled: WEBGL_polygon_mode"_s);
}

WebGLPolygonMode::~WebGLPolygonMode() = default;

bool WebGLPolygonMode::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_ANGLE_polygon_mode"_s);
}

void WebGLPolygonMode::polygonModeWEBGL(GCGLenum face, GCGLenum mode)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    context.protectedGraphicsContextGL()->polygonModeANGLE(face, mode);
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
