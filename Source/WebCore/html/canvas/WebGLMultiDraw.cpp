/*
 * Copyright (C) 2021 Google Inc. All rights reserved.
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
#include "WebGLMultiDraw.h"

#include "InspectorInstrumentation.h"
#include "WebGLUtilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebGLMultiDraw);

WebGLMultiDraw::WebGLMultiDraw(WebGLRenderingContextBase& context)
    : WebGLExtension(context, WebGLExtensionName::WebGLMultiDraw)
{
    context.protectedGraphicsContextGL()->ensureExtensionEnabled("GL_ANGLE_multi_draw"_s);

    // Spec requires ANGLE_instanced_arrays to be turned on implicitly here.
    // Enable it both in the backend and in WebKit.
    if (context.isWebGL1())
        context.getExtension("ANGLE_instanced_arrays"_s);
}

WebGLMultiDraw::~WebGLMultiDraw() = default;

bool WebGLMultiDraw::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_ANGLE_multi_draw"_s)
        && context.supportsExtension("GL_ANGLE_instanced_arrays"_s);
}

void WebGLMultiDraw::multiDrawArraysWEBGL(GCGLenum mode, Int32List&& firstsList, GCGLuint firstsOffset, Int32List&& countsList, GCGLuint countsOffset, GCGLsizei drawcount)
{
    if (isContextLost())
        return;
    auto& context = this->context();

    if (!validateDrawcount(context, "multiDrawArraysWEBGL", drawcount)
        || !validateOffset(context, "multiDrawArraysWEBGL", "firstsOffset out of bounds", firstsList.length(), firstsOffset, drawcount)
        || !validateOffset(context, "multiDrawArraysWEBGL", "countsOffset out of bounds", countsList.length(), countsOffset, drawcount)) {
        return;
    }

    if (!context.validateVertexArrayObject("multiDrawArraysWEBGL"))
        return;

    if (context.m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(context, *context.m_currentProgram))
        return;

    context.clearIfComposited(WebGLRenderingContextBase::CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { context };

        context.protectedGraphicsContextGL()->multiDrawArraysANGLE(mode, GCGLSpanTuple { firstsList.data() + firstsOffset, countsList.data() + countsOffset, static_cast<size_t>(drawcount) });
    }

    context.markContextChangedAndNotifyCanvasObserver();
}

void WebGLMultiDraw::multiDrawArraysInstancedWEBGL(GCGLenum mode, Int32List&& firstsList, GCGLuint firstsOffset, Int32List&& countsList, GCGLuint countsOffset, Int32List&& instanceCountsList, GCGLuint instanceCountsOffset, GCGLsizei drawcount)
{
    if (isContextLost())
        return;
    auto& context = this->context();

    if (!validateDrawcount(context, "multiDrawArraysInstancedWEBGL", drawcount)
        || !validateOffset(context, "multiDrawArraysInstancedWEBGL", "firstsOffset out of bounds", firstsList.length(), firstsOffset, drawcount)
        || !validateOffset(context, "multiDrawArraysInstancedWEBGL", "countsOffset out of bounds", countsList.length(), countsOffset, drawcount)
        || !validateOffset(context, "multiDrawArraysInstancedWEBGL", "instanceCountsOffset out of bounds", instanceCountsList.length(), instanceCountsOffset, drawcount)) {
        return;
    }

    if (!context.validateVertexArrayObject("multiDrawArraysInstancedWEBGL"))
        return;

    if (context.m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(context, *context.m_currentProgram))
        return;

    context.clearIfComposited(WebGLRenderingContextBase::CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { context };

        context.protectedGraphicsContextGL()->multiDrawArraysInstancedANGLE(mode, GCGLSpanTuple { firstsList.data() +  firstsOffset, countsList.data() + countsOffset, instanceCountsList.data() + instanceCountsOffset, static_cast<size_t>(drawcount) });
    }

    context.markContextChangedAndNotifyCanvasObserver();
}

void WebGLMultiDraw::multiDrawElementsWEBGL(GCGLenum mode, Int32List&& countsList, GCGLuint countsOffset, GCGLenum type, Int32List&& offsetsList, GCGLuint offsetsOffset, GCGLsizei drawcount)
{
    if (isContextLost())
        return;
    auto& context = this->context();

    if (!validateDrawcount(context, "multiDrawElementsWEBGL", drawcount)
        || !validateOffset(context, "multiDrawElementsWEBGL", "countsOffset out of bounds", countsList.length(), countsOffset, drawcount)
        || !validateOffset(context, "multiDrawElementsWEBGL", "offsetsOffset out of bounds", offsetsList.length(), offsetsOffset, drawcount)) {
        return;
    }

    if (!context.validateVertexArrayObject("multiDrawElementsWEBGL"))
        return;

    if (context.m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(context, *context.m_currentProgram))
        return;

    context.clearIfComposited(WebGLRenderingContextBase::CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { context };

        context.protectedGraphicsContextGL()->multiDrawElementsANGLE(mode, GCGLSpanTuple { countsList.data() + countsOffset, offsetsList.data() + offsetsOffset, static_cast<size_t>(drawcount) }, type);
    }

    context.markContextChangedAndNotifyCanvasObserver();
}

void WebGLMultiDraw::multiDrawElementsInstancedWEBGL(GCGLenum mode, Int32List&& countsList, GCGLuint countsOffset, GCGLenum type, Int32List&& offsetsList, GCGLuint offsetsOffset, Int32List&& instanceCountsList, GCGLuint instanceCountsOffset, GCGLsizei drawcount)
{
    if (isContextLost())
        return;
    auto& context = this->context();

    if (!validateDrawcount(context, "multiDrawElementsWEBGL", drawcount)
        || !validateOffset(context, "multiDrawElementsWEBGL", "countsOffset out of bounds", countsList.length(), countsOffset, drawcount)
        || !validateOffset(context, "multiDrawElementsWEBGL", "offsetsOffset out of bounds", offsetsList.length(), offsetsOffset, drawcount)
        || !validateOffset(context, "multiDrawElementsWEBGL", "instanceCountsOffset out of bounds", instanceCountsList.length(), instanceCountsOffset, drawcount)) {
        return;
    }

    if (!context.validateVertexArrayObject("multiDrawElementsInstancedWEBGL"))
        return;

    if (context.m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(context, *context.m_currentProgram))
        return;

    context.clearIfComposited(WebGLRenderingContextBase::CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { context };

        context.protectedGraphicsContextGL()->multiDrawElementsInstancedANGLE(mode, GCGLSpanTuple { countsList.data() + countsOffset, offsetsList.data() + offsetsOffset, instanceCountsList.data() + instanceCountsOffset, static_cast<size_t>(drawcount) }, type);
    }

    context.markContextChangedAndNotifyCanvasObserver();
}

bool WebGLMultiDraw::validateDrawcount(WebGLRenderingContextBase& context, const char* functionName, GCGLsizei drawcount)
{
    if (drawcount < 0) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "negative drawcount");
        return false;
    }

    return true;
}

bool WebGLMultiDraw::validateOffset(WebGLRenderingContextBase& context, const char* functionName, const char* outOfBoundsDescription, GCGLsizei size, GCGLuint offset, GCGLsizei drawcount)
{
    if (drawcount > size) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "drawcount out of bounds");
        return false;
    }

    if (offset > static_cast<GCGLuint>(size - drawcount)) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, outOfBoundsDescription);
        return false;
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
