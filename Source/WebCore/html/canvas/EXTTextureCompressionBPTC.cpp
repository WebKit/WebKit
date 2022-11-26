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
#include "EXTTextureCompressionBPTC.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(EXTTextureCompressionBPTC);

EXTTextureCompressionBPTC::EXTTextureCompressionBPTC(WebGLRenderingContextBase& context)
    : WebGLExtension(context)
{
    context.graphicsContextGL()->ensureExtensionEnabled("GL_EXT_texture_compression_bptc"_s);

    context.addCompressedTextureFormat(GraphicsContextGL::COMPRESSED_RGBA_BPTC_UNORM_EXT);
    context.addCompressedTextureFormat(GraphicsContextGL::COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT);
    context.addCompressedTextureFormat(GraphicsContextGL::COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT);
    context.addCompressedTextureFormat(GraphicsContextGL::COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT);
}

EXTTextureCompressionBPTC::~EXTTextureCompressionBPTC() = default;

WebGLExtension::ExtensionName EXTTextureCompressionBPTC::getName() const
{
    return EXTTextureCompressionBPTCName;
}

bool EXTTextureCompressionBPTC::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_EXT_texture_compression_bptc"_s);
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
