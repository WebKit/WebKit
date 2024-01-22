/*
 * Copyright (C) 2013 Motorola Mobility LLC. All rights reserved.
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
#include "OESTextureHalfFloat.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(OESTextureHalfFloat);

OESTextureHalfFloat::OESTextureHalfFloat(WebGLRenderingContextBase& context)
    : WebGLExtension(context, WebGLExtensionName::OESTextureHalfFloat)
{
    context.protectedGraphicsContextGL()->ensureExtensionEnabled("GL_OES_texture_half_float"_s);

    // Spec requires EXT_color_buffer_half_float to be turned on implicitly here.
    // Enable it both in the backend and in WebKit.
    context.getExtension("EXT_color_buffer_half_float"_s);
}

OESTextureHalfFloat::~OESTextureHalfFloat() = default;

bool OESTextureHalfFloat::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_OES_texture_half_float"_s);
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
