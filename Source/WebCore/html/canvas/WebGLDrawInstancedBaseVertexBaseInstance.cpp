/*
 * Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "WebGLDrawInstancedBaseVertexBaseInstance.h"

#include "InspectorInstrumentation.h"
#include "WebGLUtilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebGLDrawInstancedBaseVertexBaseInstance);

WebGLDrawInstancedBaseVertexBaseInstance::WebGLDrawInstancedBaseVertexBaseInstance(WebGLRenderingContextBase& context)
    : WebGLExtension(context, WebGLExtensionName::WebGLDrawInstancedBaseVertexBaseInstance)
{
    context.protectedGraphicsContextGL()->ensureExtensionEnabled("GL_ANGLE_base_vertex_base_instance"_s);
}

WebGLDrawInstancedBaseVertexBaseInstance::~WebGLDrawInstancedBaseVertexBaseInstance() = default;

bool WebGLDrawInstancedBaseVertexBaseInstance::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_ANGLE_base_vertex_base_instance"_s);
}

void WebGLDrawInstancedBaseVertexBaseInstance::drawArraysInstancedBaseInstanceWEBGL(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei instanceCount, GCGLuint baseInstance)
{
    if (isContextLost())
        return;
    auto& context = this->context();

    if (!context.validateVertexArrayObject("drawArraysInstancedBaseInstanceWEBGL"))
        return;

    if (context.m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(context, *context.m_currentProgram))
        return;

    context.clearIfComposited(WebGLRenderingContextBase::CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { context };

        context.protectedGraphicsContextGL()->drawArraysInstancedBaseInstanceANGLE(mode, first, count, instanceCount, baseInstance);
    }

    context.markContextChangedAndNotifyCanvasObserver();
}

void WebGLDrawInstancedBaseVertexBaseInstance::drawElementsInstancedBaseVertexBaseInstanceWEBGL(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset, GCGLsizei instanceCount, GCGLint baseVertex, GCGLuint baseInstance)
{
    if (isContextLost())
        return;
    auto& context = this->context();

    if (!context.validateVertexArrayObject("drawElementsInstancedBaseVertexBaseInstanceWEBGL"))
        return;

    if (context.m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(context, *context.m_currentProgram))
        return;

    context.clearIfComposited(WebGLRenderingContextBase::CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { context };

        context.protectedGraphicsContextGL()->drawElementsInstancedBaseVertexBaseInstanceANGLE(mode, count, type, offset, instanceCount, baseVertex, baseInstance);
    }

    context.markContextChangedAndNotifyCanvasObserver();
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
