/*
 * Copyright (C) 2019 Google Inc. All rights reserved.
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
#include "WebGLCompressedTextureETC.h"

#include "ExtensionsGL.h"
#include "WebGLRenderingContextBase.h"

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebGLCompressedTextureETC);

WebGLCompressedTextureETC::WebGLCompressedTextureETC(WebGLRenderingContextBase& context)
    : WebGLExtension(context)
{
    context.graphicsContextGL()->getExtensions().ensureEnabled("GL_ANGLE_compressed_texture_etc");

    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_R11_EAC);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SIGNED_R11_EAC);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RG11_EAC);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SIGNED_RG11_EAC);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGB8_ETC2);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ETC2);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA8_ETC2_EAC);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);
}

WebGLCompressedTextureETC::~WebGLCompressedTextureETC() = default;

WebGLExtension::ExtensionName WebGLCompressedTextureETC::getName() const
{
    return WebGLCompressedTextureETCName;
}

bool WebGLCompressedTextureETC::supported(WebGLRenderingContextBase& context)
{
    return context.graphicsContextGL()->getExtensions().supports("GL_ANGLE_compressed_texture_etc");
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
