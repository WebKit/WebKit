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
#include "WebGLMultiDrawInstancedBaseVertexBaseInstance.h"

#include "InspectorInstrumentation.h"
#include "WebGLUtilities.h"
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebGLMultiDrawInstancedBaseVertexBaseInstance);

WebGLMultiDrawInstancedBaseVertexBaseInstance::WebGLMultiDrawInstancedBaseVertexBaseInstance(WebGLRenderingContextBase& context)
    : WebGLExtension(context, WebGLExtensionName::WebGLMultiDrawInstancedBaseVertexBaseInstance)
{
    context.protectedGraphicsContextGL()->ensureExtensionEnabled("GL_ANGLE_base_vertex_base_instance"_s);

    // Spec requires WEBGL_multi_draw to be turned on implicitly here.
    // Enable it both in the backend and in WebKit.
    context.getExtension("WEBGL_multi_draw"_s);
}

WebGLMultiDrawInstancedBaseVertexBaseInstance::~WebGLMultiDrawInstancedBaseVertexBaseInstance() = default;

bool WebGLMultiDrawInstancedBaseVertexBaseInstance::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_ANGLE_base_vertex_base_instance"_s)
        && context.supportsExtension("GL_ANGLE_multi_draw"_s);
}

void WebGLMultiDrawInstancedBaseVertexBaseInstance::multiDrawArraysInstancedBaseInstanceWEBGL(GCGLenum mode, Int32List&& firstsList, GCGLuint firstsOffset, Int32List&& countsList, GCGLuint countsOffset, Int32List&& instanceCountsList, GCGLuint instanceCountsOffset, Uint32List&& baseInstancesList, GCGLuint baseInstancesOffset, GCGLsizei drawcount)
{
    if (isContextLost())
        return;
    auto& context = this->context();

    if (!validateDrawcount(context, "multiDrawArraysInstancedBaseInstanceWEBGL"_s, drawcount)
        || !validateOffset(context, "multiDrawArraysInstancedBaseInstanceWEBGL"_s, "firstsOffset out of bounds"_s, firstsList.length(), firstsOffset, drawcount)
        || !validateOffset(context, "multiDrawArraysInstancedBaseInstanceWEBGL"_s, "countsOffset out of bounds"_s, countsList.length(), countsOffset, drawcount)
        || !validateOffset(context, "multiDrawArraysInstancedBaseInstanceWEBGL"_s, "instanceCountsOffset out of bounds"_s, instanceCountsList.length(), instanceCountsOffset, drawcount)
        || !validateOffset(context, "multiDrawArraysInstancedBaseInstanceWEBGL"_s, "baseInstancesOffset out of bounds"_s, baseInstancesList.length(), baseInstancesOffset, drawcount)) {
        return;
    }

    if (!context.validateVertexArrayObject("multiDrawArraysInstancedBaseInstanceWEBGL"_s))
        return;

    if (context.m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(context, *context.m_currentProgram))
        return;

    context.clearIfComposited(WebGLRenderingContextBase::CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { context };

        context.protectedGraphicsContextGL()->multiDrawArraysInstancedBaseInstanceANGLE(mode, GCGLSpanTuple { firstsList.data() +  firstsOffset, countsList.data() + countsOffset, instanceCountsList.data() + instanceCountsOffset, baseInstancesList.data() + baseInstancesOffset, static_cast<size_t>(drawcount) });
    }

    context.markContextChangedAndNotifyCanvasObserver();
}

void WebGLMultiDrawInstancedBaseVertexBaseInstance::multiDrawElementsInstancedBaseVertexBaseInstanceWEBGL(GCGLenum mode, Int32List&& countsList, GCGLuint countsOffset, GCGLenum type, Int32List&& offsetsList, GCGLuint offsetsOffset, Int32List&& instanceCountsList, GCGLuint instanceCountsOffset, Int32List&& baseVerticesList, GCGLuint baseVerticesOffset, Uint32List&& baseInstancesList, GCGLuint baseInstancesOffset, GCGLsizei drawcount)
{
    if (isContextLost())
        return;
    auto& context = this->context();

    if (!validateDrawcount(context, "multiDrawElementsInstancedBaseVertexBaseInstanceWEBGL"_s, drawcount)
        || !validateOffset(context, "multiDrawElementsInstancedBaseVertexBaseInstanceWEBGL"_s, "countsOffset out of bounds"_s, countsList.length(), countsOffset, drawcount)
        || !validateOffset(context, "multiDrawElementsInstancedBaseVertexBaseInstanceWEBGL"_s, "offsetsOffset out of bounds"_s, offsetsList.length(), offsetsOffset, drawcount)
        || !validateOffset(context, "multiDrawElementsInstancedBaseVertexBaseInstanceWEBGL"_s, "instanceCountsOffset out of bounds"_s, instanceCountsList.length(), instanceCountsOffset, drawcount)
        || !validateOffset(context, "multiDrawElementsInstancedBaseVertexBaseInstanceWEBGL"_s, "baseVerticesOffset out of bounds"_s, baseVerticesList.length(), baseVerticesOffset, drawcount)
        || !validateOffset(context, "multiDrawElementsInstancedBaseVertexBaseInstanceWEBGL"_s, "baseInstancesOffset out of bounds"_s, baseInstancesList.length(), baseInstancesOffset, drawcount)) {
        return;
    }

    if (!context.validateVertexArrayObject("multiDrawElementsInstancedBaseVertexBaseInstanceWEBGL"_s))
        return;

    if (context.m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(context, *context.m_currentProgram))
        return;

    context.clearIfComposited(WebGLRenderingContextBase::CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { context };

        context.protectedGraphicsContextGL()->multiDrawElementsInstancedBaseVertexBaseInstanceANGLE(mode, GCGLSpanTuple { countsList.data() + countsOffset, offsetsList.data() + offsetsOffset, instanceCountsList.data() + instanceCountsOffset, baseVerticesList.data() + baseVerticesOffset, baseInstancesList.data() + baseInstancesOffset, static_cast<size_t>(drawcount) }, type);
    }

    context.markContextChangedAndNotifyCanvasObserver();
}

bool WebGLMultiDrawInstancedBaseVertexBaseInstance::validateDrawcount(WebGLRenderingContextBase& context, ASCIILiteral functionName, GCGLsizei drawcount)
{
    if (drawcount < 0) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "negative drawcount"_s);
        return false;
    }

    return true;
}

bool WebGLMultiDrawInstancedBaseVertexBaseInstance::validateOffset(WebGLRenderingContextBase& context, ASCIILiteral functionName, ASCIILiteral outOfBoundsDescription, GCGLsizei size, GCGLuint offset, GCGLsizei drawcount)
{
    if (drawcount > size) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "drawcount out of bounds"_s);
        return false;
    }

    if (offset > static_cast<GCGLuint>(size - drawcount)) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, outOfBoundsDescription);
        return false;
    }

    return true;
}

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBGL)
