/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "WebGLCompressedTextureASTC.h"

#if ENABLE(WEBGL)

#include "ExtensionsGL.h"
#include "RuntimeEnabledFeatures.h"
#include "WebGLRenderingContextBase.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebGLCompressedTextureASTC);

WebGLCompressedTextureASTC::WebGLCompressedTextureASTC(WebGLRenderingContextBase& context)
    : WebGLExtension(context)
    , m_isHDRSupported(context.graphicsContextGL()->getExtensions().supports("GL_KHR_texture_compression_astc_hdr"_s))
    , m_isLDRSupported(context.graphicsContextGL()->getExtensions().supports("GL_KHR_texture_compression_astc_ldr"_s))
{
    context.graphicsContextGL()->getExtensions().ensureEnabled("GL_KHR_texture_compression_astc_hdr"_s);
    context.graphicsContextGL()->getExtensions().ensureEnabled("GL_KHR_texture_compression_astc_ldr"_s);

    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_4x4_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_5x4_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_5x5_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_6x5_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_6x6_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_8x5_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_8x6_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_8x8_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_10x5_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_10x6_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_10x8_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_10x10_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_12x10_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_RGBA_ASTC_12x12_KHR);
    
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR);
    context.addCompressedTextureFormat(ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR);
}

WebGLCompressedTextureASTC::~WebGLCompressedTextureASTC() = default;

WebGLExtension::ExtensionName WebGLCompressedTextureASTC::getName() const
{
    return WebGLCompressedTextureASTCName;
}
    
Vector<String> WebGLCompressedTextureASTC::getSupportedProfiles()
{
    Vector<String> result;
    
    if (m_isHDRSupported)
        result.append("hdr"_s);
    if (m_isLDRSupported)
        result.append("ldr"_s);
    
    return result;
}

bool WebGLCompressedTextureASTC::supported(const WebGLRenderingContextBase& context)
{
    return context.graphicsContextGL()->getExtensions().supports("GL_KHR_texture_compression_astc_hdr"_s)
        || context.graphicsContextGL()->getExtensions().supports("GL_KHR_texture_compression_astc_ldr"_s);
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
