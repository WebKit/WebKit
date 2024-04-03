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

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ANGLEInstancedArrays);

ANGLEInstancedArrays::ANGLEInstancedArrays(WebGLRenderingContextBase& context)
    : WebGLExtension(context, WebGLExtensionName::ANGLEInstancedArrays)
{
    context.protectedGraphicsContextGL()->ensureExtensionEnabled("GL_ANGLE_instanced_arrays"_s);
}

ANGLEInstancedArrays::~ANGLEInstancedArrays() = default;

bool ANGLEInstancedArrays::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_ANGLE_instanced_arrays"_s);
}

void ANGLEInstancedArrays::drawArraysInstancedANGLE(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    context.drawArraysInstanced(mode, first, count, primcount);
}

void ANGLEInstancedArrays::drawElementsInstancedANGLE(GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset, GCGLsizei primcount)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    context.drawElementsInstanced(mode, count, type, offset, primcount);
}

void ANGLEInstancedArrays::vertexAttribDivisorANGLE(GCGLuint index, GCGLuint divisor)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    context.vertexAttribDivisor(index, divisor);
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
