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
#include "OESDrawBuffersIndexed.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(OESDrawBuffersIndexed);

OESDrawBuffersIndexed::OESDrawBuffersIndexed(WebGLRenderingContextBase& context)
    : WebGLExtension(context, WebGLExtensionName::OESDrawBuffersIndexed)
{
    context.protectedGraphicsContextGL()->ensureExtensionEnabled("GL_OES_draw_buffers_indexed"_s);
}

OESDrawBuffersIndexed::~OESDrawBuffersIndexed() = default;

bool OESDrawBuffersIndexed::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_OES_draw_buffers_indexed"_s);
}

void OESDrawBuffersIndexed::enableiOES(GCGLenum target, GCGLuint index)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    context.protectedGraphicsContextGL()->enableiOES(target, index);
}

void OESDrawBuffersIndexed::disableiOES(GCGLenum target, GCGLuint index)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    context.protectedGraphicsContextGL()->disableiOES(target, index);
}

void OESDrawBuffersIndexed::blendEquationiOES(GCGLuint buf, GCGLenum mode)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    context.protectedGraphicsContextGL()->blendEquationiOES(buf, mode);
}

void OESDrawBuffersIndexed::blendEquationSeparateiOES(GCGLuint buf, GCGLenum modeRGB, GCGLenum modeAlpha)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    context.protectedGraphicsContextGL()->blendEquationSeparateiOES(buf, modeRGB, modeAlpha);
}

void OESDrawBuffersIndexed::blendFunciOES(GCGLuint buf, GCGLenum src, GCGLenum dst)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    context.protectedGraphicsContextGL()->blendFunciOES(buf, src, dst);
}

void OESDrawBuffersIndexed::blendFuncSeparateiOES(GCGLuint buf, GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    context.protectedGraphicsContextGL()->blendFuncSeparateiOES(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void OESDrawBuffersIndexed::colorMaskiOES(GCGLuint buf, GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    // Used in WebGLRenderingContextBase::clearIfComposited
    if (!buf) {
        context.m_colorMask[0] = red;
        context.m_colorMask[1] = green;
        context.m_colorMask[2] = blue;
        context.m_colorMask[3] = alpha;
    }
    context.protectedGraphicsContextGL()->colorMaskiOES(buf, red, green, blue, alpha);
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
