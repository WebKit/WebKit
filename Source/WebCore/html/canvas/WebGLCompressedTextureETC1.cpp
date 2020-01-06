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
#include "WebGLCompressedTextureETC1.h"

#include "ExtensionsGL.h"
#include "WebGLRenderingContextBase.h"

namespace WebCore {

WebGLCompressedTextureETC1::WebGLCompressedTextureETC1(WebGLRenderingContextBase& context)
    : WebGLExtension(context)
{
    context.graphicsContextGL()->getExtensions().ensureEnabled("GL_OES_compressed_ETC1_RGB8_texture");

    context.addCompressedTextureFormat(ExtensionsGL::ETC1_RGB8_OES);
}

WebGLCompressedTextureETC1::~WebGLCompressedTextureETC1() = default;

WebGLExtension::ExtensionName WebGLCompressedTextureETC1::getName() const
{
    return WebGLCompressedTextureETC1Name;
}

bool WebGLCompressedTextureETC1::supported(WebGLRenderingContextBase& context)
{
    return context.graphicsContextGL()->getExtensions().supports("GL_OES_compressed_ETC1_RGB8_texture");
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
