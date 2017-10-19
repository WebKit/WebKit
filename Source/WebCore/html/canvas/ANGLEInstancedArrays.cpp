/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL)
#include "ANGLEInstancedArrays.h"

#if PLATFORM(GTK)
#include "Extensions3D.h"
#endif

namespace WebCore {

ANGLEInstancedArrays::ANGLEInstancedArrays(WebGLRenderingContextBase& context)
    : WebGLExtension(context)
{
}

ANGLEInstancedArrays::~ANGLEInstancedArrays() = default;

WebGLExtension::ExtensionName ANGLEInstancedArrays::getName() const
{
    return ANGLEInstancedArraysName;
}

bool ANGLEInstancedArrays::supported(WebGLRenderingContextBase& context)
{
#if PLATFORM(COCOA)
    UNUSED_PARAM(context);
    return true;
#elif PLATFORM(GTK)
    return context.graphicsContext3D()->getExtensions().supports("GL_ANGLE_instanced_arrays");
#else
    UNUSED_PARAM(context);
    return false;
#endif
}

void ANGLEInstancedArrays::drawArraysInstancedANGLE(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei primcount)
{
    if (m_context.isContextLost())
        return;
    m_context.drawArraysInstanced(mode, first, count, primcount);
}

void ANGLEInstancedArrays::drawElementsInstancedANGLE(GC3Denum mode, GC3Dsizei count, GC3Denum type, long long offset, GC3Dsizei primcount)
{
    if (m_context.isContextLost())
        return;
    m_context.drawElementsInstanced(mode, count, type, offset, primcount);
}

void ANGLEInstancedArrays::vertexAttribDivisorANGLE(GC3Duint index, GC3Duint divisor)
{
    if (m_context.isContextLost())
        return;
    m_context.vertexAttribDivisor(index, divisor);
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
