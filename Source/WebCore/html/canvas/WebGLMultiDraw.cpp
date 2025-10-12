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
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebGLMultiDraw);

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
    Ref context = this->context();

    if (!validateDrawcount(context.get(), "multiDrawArraysWEBGL"_s, drawcount)
        || !validateOffset(context.get(), "multiDrawArraysWEBGL"_s, "firstsOffset out of bounds"_s, firstsList.length(), firstsOffset, drawcount)
        || !validateOffset(context.get(), "multiDrawArraysWEBGL"_s, "countsOffset out of bounds"_s, countsList.length(), countsOffset, drawcount)) {
        return;
    }

    if (!context->validateVertexArrayObject("multiDrawArraysWEBGL"_s))
        return;

    if (RefPtr currentProgram = context->m_currentProgram; currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(context.get(), *currentProgram))
        return;

    context->clearIfComposited(WebGLRenderingContextBase::CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { context.get() };

        context->protectedGraphicsContextGL()->multiDrawArraysANGLE(mode, GCGLSpanTuple { firstsList.span().subspan(firstsOffset).data(), countsList.span().subspan(countsOffset).data(), static_cast<size_t>(drawcount) });
    }

    context->markContextChangedAndNotifyCanvasObserver();
}

void WebGLMultiDraw::multiDrawArraysInstancedWEBGL(GCGLenum mode, Int32List&& firstsList, GCGLuint firstsOffset, Int32List&& countsList, GCGLuint countsOffset, Int32List&& instanceCountsList, GCGLuint instanceCountsOffset, GCGLsizei drawcount)
{
    if (isContextLost())
        return;
    Ref context = this->context();

    if (!validateDrawcount(context.get(), "multiDrawArraysInstancedWEBGL"_s, drawcount)
        || !validateOffset(context.get(), "multiDrawArraysInstancedWEBGL"_s, "firstsOffset out of bounds"_s, firstsList.length(), firstsOffset, drawcount)
        || !validateOffset(context.get(), "multiDrawArraysInstancedWEBGL"_s, "countsOffset out of bounds"_s, countsList.length(), countsOffset, drawcount)
        || !validateOffset(context.get(), "multiDrawArraysInstancedWEBGL"_s, "instanceCountsOffset out of bounds"_s, instanceCountsList.length(), instanceCountsOffset, drawcount)) {
        return;
    }

    if (!context->validateVertexArrayObject("multiDrawArraysInstancedWEBGL"_s))
        return;

    if (RefPtr currentProgram = context->m_currentProgram; currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(context.get(), *currentProgram))
        return;

    context->clearIfComposited(WebGLRenderingContextBase::CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { context.get() };

        context->protectedGraphicsContextGL()->multiDrawArraysInstancedANGLE(mode, GCGLSpanTuple { firstsList.span().subspan(firstsOffset).data(), countsList.span().subspan(countsOffset).data(), instanceCountsList.span().subspan(instanceCountsOffset).data(), static_cast<size_t>(drawcount) });
    }

    context->markContextChangedAndNotifyCanvasObserver();
}

void WebGLMultiDraw::multiDrawElementsWEBGL(GCGLenum mode, Int32List&& countsList, GCGLuint countsOffset, GCGLenum type, Int32List&& offsetsList, GCGLuint offsetsOffset, GCGLsizei drawcount)
{
    if (isContextLost())
        return;
    Ref context = this->context();

    if (!validateDrawcount(context.get(), "multiDrawElementsWEBGL"_s, drawcount)
        || !validateOffset(context.get(), "multiDrawElementsWEBGL"_s, "countsOffset out of bounds"_s, countsList.length(), countsOffset, drawcount)
        || !validateOffset(context.get(), "multiDrawElementsWEBGL"_s, "offsetsOffset out of bounds"_s, offsetsList.length(), offsetsOffset, drawcount)) {
        return;
    }

    if (!context->validateVertexArrayObject("multiDrawElementsWEBGL"_s))
        return;

    if (RefPtr currentProgram = context->m_currentProgram; currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(context.get(), *currentProgram))
        return;

    context->clearIfComposited(WebGLRenderingContextBase::CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { context.get() };

        context->protectedGraphicsContextGL()->multiDrawElementsANGLE(mode, GCGLSpanTuple { countsList.span().subspan(countsOffset).data(), offsetsList.span().subspan(offsetsOffset).data(), static_cast<size_t>(drawcount) }, type);
    }

    context->markContextChangedAndNotifyCanvasObserver();
}

void WebGLMultiDraw::multiDrawElementsInstancedWEBGL(GCGLenum mode, Int32List&& countsList, GCGLuint countsOffset, GCGLenum type, Int32List&& offsetsList, GCGLuint offsetsOffset, Int32List&& instanceCountsList, GCGLuint instanceCountsOffset, GCGLsizei drawcount)
{
    if (isContextLost())
        return;
    Ref context = this->context();

    if (!validateDrawcount(context.get(), "multiDrawElementsInstancedWEBGL"_s, drawcount)
        || !validateOffset(context.get(), "multiDrawElementsInstancedWEBGL"_s, "countsOffset out of bounds"_s, countsList.length(), countsOffset, drawcount)
        || !validateOffset(context.get(), "multiDrawElementsInstancedWEBGL"_s, "offsetsOffset out of bounds"_s, offsetsList.length(), offsetsOffset, drawcount)
        || !validateOffset(context.get(), "multiDrawElementsInstancedWEBGL"_s, "instanceCountsOffset out of bounds"_s, instanceCountsList.length(), instanceCountsOffset, drawcount)) {
        return;
    }

    if (!context->validateVertexArrayObject("multiDrawElementsInstancedWEBGL"_s))
        return;

    if (RefPtr currentProgram = context->m_currentProgram; currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(context.get(), *currentProgram))
        return;

    context->clearIfComposited(WebGLRenderingContextBase::CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { context.get() };

        context->protectedGraphicsContextGL()->multiDrawElementsInstancedANGLE(mode, GCGLSpanTuple { countsList.span().subspan(countsOffset).data(), offsetsList.span().subspan(offsetsOffset).data(), instanceCountsList.span().subspan(instanceCountsOffset).data(), static_cast<size_t>(drawcount) }, type);
    }

    context->markContextChangedAndNotifyCanvasObserver();
}

bool WebGLMultiDraw::validateDrawcount(WebGLRenderingContextBase& context, ASCIILiteral functionName, GCGLsizei drawcount)
{
    if (drawcount < 0) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "negative drawcount"_s);
        return false;
    }

    return true;
}

bool WebGLMultiDraw::validateOffset(WebGLRenderingContextBase& context, ASCIILiteral functionName, ASCIILiteral outOfBoundsDescription, GCGLsizei size, GCGLuint offset, GCGLsizei drawcount)
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

#endif // ENABLE(WEBGL)
