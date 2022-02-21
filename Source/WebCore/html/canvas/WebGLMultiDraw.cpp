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

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebGLMultiDraw);

static GCGLSpan<const int> makeSpanWithOffset(WebGLMultiDraw::Int32List& list, GCGLuint offset)
{
    return makeGCGLSpan(list.data() + offset, list.length() - offset);
}

WebGLMultiDraw::WebGLMultiDraw(WebGLRenderingContextBase& context)
    : WebGLExtension(context)
{
    context.graphicsContextGL()->ensureExtensionEnabled("GL_ANGLE_multi_draw"_s);
    context.graphicsContextGL()->ensureExtensionEnabled("GL_ANGLE_instanced_arrays"_s);
}

WebGLMultiDraw::~WebGLMultiDraw() = default;

WebGLExtension::ExtensionName WebGLMultiDraw::getName() const
{
    return WebGLMultiDrawName;
}

bool WebGLMultiDraw::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_ANGLE_multi_draw"_s)
        && context.supportsExtension("GL_ANGLE_instanced_arrays"_s);
}

void WebGLMultiDraw::multiDrawArraysWEBGL(GCGLenum mode, Int32List firstsList, GCGLuint firstsOffset, Int32List countsList, GCGLuint countsOffset, GCGLsizei drawcount)
{
    if (!m_context || m_context->isContextLost())
        return;

    if (!validateDrawcount("multiDrawArraysWEBGL", drawcount)
        || !validateOffset("multiDrawArraysWEBGL", "firstsOffset out of bounds", firstsList.length(), firstsOffset, drawcount)
        || !validateOffset("multiDrawArraysWEBGL", "countsOffset out of bounds", countsList.length(), countsOffset, drawcount)) {
        return;
    }

    m_context->graphicsContextGL()->multiDrawArraysANGLE(mode, makeSpanWithOffset(firstsList, firstsOffset), makeSpanWithOffset(countsList, countsOffset), drawcount);
}

void WebGLMultiDraw::multiDrawArraysInstancedWEBGL(GCGLenum mode, Int32List firstsList, GCGLuint firstsOffset, Int32List countsList, GCGLuint countsOffset, Int32List instanceCountsList, GCGLuint instanceCountsOffset, GCGLsizei drawcount)
{
    if (!m_context || m_context->isContextLost())
        return;

    if (!validateDrawcount("multiDrawArraysInstancedWEBGL", drawcount)
        || !validateOffset("multiDrawArraysInstancedWEBGL", "firstsOffset out of bounds", firstsList.length(), firstsOffset, drawcount)
        || !validateOffset("multiDrawArraysInstancedWEBGL", "countsOffset out of bounds", countsList.length(), countsOffset, drawcount)
        || !validateOffset("multiDrawArraysInstancedWEBGL", "instanceCountsOffset out of bounds", instanceCountsList.length(), instanceCountsOffset, drawcount)) {
        return;
    }

    m_context->graphicsContextGL()->multiDrawArraysInstancedANGLE(mode, makeSpanWithOffset(firstsList, firstsOffset), makeSpanWithOffset(countsList, countsOffset), makeSpanWithOffset(instanceCountsList, instanceCountsOffset), drawcount);
}

void WebGLMultiDraw::multiDrawElementsWEBGL(GCGLenum mode, Int32List countsList, GCGLuint countsOffset, GCGLenum type, Int32List offsetsList, GCGLuint offsetsOffset, GCGLsizei drawcount)
{
    if (!m_context || m_context->isContextLost())
        return;

    if (!validateDrawcount("multiDrawElementsWEBGL", drawcount)
        || !validateOffset("multiDrawElementsWEBGL", "countsOffset out of bounds", countsList.length(), countsOffset, drawcount)
        || !validateOffset("multiDrawElementsWEBGL", "offsetsOffset out of bounds", offsetsList.length(), offsetsOffset, drawcount)) {
        return;
    }

    m_context->graphicsContextGL()->multiDrawElementsANGLE(mode, makeSpanWithOffset(countsList, countsOffset), type, makeSpanWithOffset(offsetsList, offsetsOffset), drawcount);
}

void WebGLMultiDraw::multiDrawElementsInstancedWEBGL(GCGLenum mode, Int32List countsList, GCGLuint countsOffset, GCGLenum type, Int32List offsetsList, GCGLuint offsetsOffset, Int32List instanceCountsList, GCGLuint instanceCountsOffset, GCGLsizei drawcount)
{
    if (!m_context || m_context->isContextLost())
        return;

    if (!validateDrawcount("multiDrawElementsWEBGL", drawcount)
        || !validateOffset("multiDrawElementsWEBGL", "countsOffset out of bounds", countsList.length(), countsOffset, drawcount)
        || !validateOffset("multiDrawElementsWEBGL", "offsetsOffset out of bounds", offsetsList.length(), offsetsOffset, drawcount)
        || !validateOffset("multiDrawElementsWEBGL", "countsOffset out of bounds", instanceCountsList.length(), instanceCountsOffset, drawcount)) {
        return;
    }

    m_context->graphicsContextGL()->multiDrawElementsInstancedANGLE(mode, makeSpanWithOffset(countsList, countsOffset), type, makeSpanWithOffset(offsetsList, offsetsOffset), makeSpanWithOffset(instanceCountsList, instanceCountsOffset), drawcount);
}

bool WebGLMultiDraw::validateDrawcount(const char* functionName, GCGLsizei drawcount)
{
    if (drawcount < 0) {
        m_context->synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "negative drawcount");
        return false;
    }

    return true;
}

bool WebGLMultiDraw::validateOffset(const char* functionName, const char* outOfBoundsDescription, GCGLsizei size, GCGLuint offset, GCGLsizei drawcount)
{
    if (drawcount > size) {
        m_context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "drawcount out of bounds");
        return false;
    }

    if (offset >= static_cast<GCGLuint>(size - drawcount)) {
        m_context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, outOfBoundsDescription);
        return false;
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
