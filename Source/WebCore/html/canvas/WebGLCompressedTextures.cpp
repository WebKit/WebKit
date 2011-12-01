/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "WebGLCompressedTextures.h"

#include "Extensions3D.h"
#include "WebGLRenderingContext.h"

#include <wtf/Int32Array.h>
#include <wtf/OwnArrayPtr.h>

namespace WebCore {

WebGLCompressedTextures::WebGLCompressedTextures(WebGLRenderingContext* context)
    : WebGLExtension(context)
    , m_supportsDxt1(false)
    , m_supportsDxt5(false)
    , m_supportsEtc1(false)
    , m_supportsPvrtc(false)
{
    Extensions3D* extensions = context->graphicsContext3D()->getExtensions();
    if (extensions->supports("GL_EXT_texture_compression_dxt1")) {
        extensions->ensureEnabled("GL_EXT_texture_compression_dxt1");
        m_supportsDxt1 = true;
    }
    if (extensions->supports("GL_EXT_texture_compression_s3tc")) {
        extensions->ensureEnabled("GL_EXT_texture_compression_s3tc");
        m_supportsDxt1 = true;
        m_supportsDxt5 = true;
    }
    if (extensions->supports("GL_CHROMIUM_texture_compression_dxt5")) {
        extensions->ensureEnabled("GL_CHROMIUM_texture_compression_dxt5");
        m_supportsDxt5 = true;
    }
    if (extensions->supports("GL_OES_compressed_ETC1_RGB8_texture")) {
        extensions->ensureEnabled("GL_OES_compressed_ETC1_RGB8_texture");
        m_supportsEtc1 = true;
    }
    if (extensions->supports("GL_IMG_texture_compression_pvrtc")) {
        extensions->ensureEnabled("GL_IMG_texture_compression_pvrtc");
        m_supportsPvrtc = true;
    }

    if (m_supportsDxt1) {
        m_formats.append(Extensions3D::COMPRESSED_RGB_S3TC_DXT1_EXT);
        m_formats.append(Extensions3D::COMPRESSED_RGBA_S3TC_DXT1_EXT);
    }

    if (m_supportsDxt5)
        m_formats.append(Extensions3D::COMPRESSED_RGBA_S3TC_DXT5_EXT);

    if (m_supportsEtc1)
        m_formats.append(Extensions3D::ETC1_RGB8_OES);

    if (m_supportsPvrtc) {
        m_formats.append(Extensions3D::COMPRESSED_RGB_PVRTC_4BPPV1_IMG);
        m_formats.append(Extensions3D::COMPRESSED_RGBA_PVRTC_4BPPV1_IMG);
    }
}

WebGLCompressedTextures::~WebGLCompressedTextures()
{
}

WebGLExtension::ExtensionName WebGLCompressedTextures::getName() const
{
    return WebKitWebGLCompressedTexturesName;
}

PassOwnPtr<WebGLCompressedTextures> WebGLCompressedTextures::create(WebGLRenderingContext* context)
{
    return adoptPtr(new WebGLCompressedTextures(context));
}

bool WebGLCompressedTextures::supported(WebGLRenderingContext* context)
{
    Extensions3D* extensions = context->graphicsContext3D()->getExtensions();
    return extensions->supports("GL_EXT_texture_compression_dxt1")
        || extensions->supports("GL_EXT_texture_compression_s3tc")
        || extensions->supports("GL_CHROMIUM_texture_compression_dxt5")
        || extensions->supports("GL_OES_compressed_ETC1_RGB8_texture")
        || extensions->supports("GL_IMG_texture_compression_pvrtc");
}

bool WebGLCompressedTextures::validateCompressedTexFormat(GC3Denum format)
{
    switch (format) {
    case Extensions3D::COMPRESSED_RGB_S3TC_DXT1_EXT:
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return m_supportsDxt1;
    case Extensions3D::ETC1_RGB8_OES:
        return m_supportsEtc1;
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return m_supportsDxt5;
    case Extensions3D::COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case Extensions3D::COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
        return m_supportsPvrtc;
    }
    return false;
}

bool WebGLCompressedTextures::validateCompressedTexFuncData(GC3Dsizei width, GC3Dsizei height,
                                                                        GC3Denum format, ArrayBufferView* pixels)
{
    GraphicsContext3D* context = m_context->graphicsContext3D();
    if (!pixels) {
        context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }
    if (width < 0 || height < 0) {
        context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }

    unsigned int bytesRequired = 0;

    switch (format) {
    case Extensions3D::COMPRESSED_RGB_S3TC_DXT1_EXT:
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case Extensions3D::ETC1_RGB8_OES:
        {
            const int kBlockWidth = 4;
            const int kBlockHeight = 4;
            const int kBlockSize = 8;
            int numBlocksAcross = (width + kBlockWidth - 1) / kBlockWidth;
            int numBlocksDown = (height + kBlockHeight - 1) / kBlockHeight;
            int numBlocks = numBlocksAcross * numBlocksDown;
            bytesRequired = numBlocks * kBlockSize;
        }
        break;
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT5_EXT:
        {
            const int kBlockWidth = 4;
            const int kBlockHeight = 4;
            const int kBlockSize = 16;
            int numBlocksAcross = (width + kBlockWidth - 1) / kBlockWidth;
            int numBlocksDown = (height + kBlockHeight - 1) / kBlockHeight;
            int numBlocks = numBlocksAcross * numBlocksDown;
            bytesRequired = numBlocks * kBlockSize;
        }
        break;
    case Extensions3D::COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case Extensions3D::COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
        {
            bytesRequired = (std::max(width, 8) * std::max(height, 8) + 7) / 8;
        }
        break;
    default:
        context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return false;
    }

    if (pixels->byteLength() != bytesRequired) {
        context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }

    return true;
}

bool WebGLCompressedTextures::validateCompressedTexSubDimensions(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
                                                                             GC3Dsizei width, GC3Dsizei height, GC3Denum format, WebGLTexture* tex)
{
    switch (format) {
    case Extensions3D::COMPRESSED_RGB_S3TC_DXT1_EXT:
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case Extensions3D::COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case Extensions3D::ETC1_RGB8_OES:
        {
            const int kBlockWidth = 4;
            const int kBlockHeight = 4;
            if ((xoffset % kBlockWidth) || (yoffset % kBlockHeight))
                return false;
            if (!xoffset) {
                if (width != tex->getWidth(target, level))
                    return false;
            } else {
                if (width % kBlockWidth)
                    return false;
            }
            if (!yoffset) {
                if (height != tex->getHeight(target, level))
                    return false;
            } else {
                if (height % kBlockHeight)
                    return false;
            }
            return true;
        }
    case Extensions3D::COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case Extensions3D::COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
        {
            if (xoffset || yoffset
                || width != tex->getWidth(target, level) || height != tex->getHeight(target, level)) {
                return false;
            }
            return true;
        }
    }
    return false;
}

void WebGLCompressedTextures::compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width,
                                                               GC3Dsizei height, GC3Dint border, ArrayBufferView* data)
{
    GraphicsContext3D* context = m_context->graphicsContext3D();
    if (m_context->isContextLost())
        return;
    if (!validateCompressedTexFormat(internalformat)) {
        context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }
    if (border) {
        context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    if (!validateCompressedTexFuncData(width, height, internalformat, data))
        return;
    WebGLTexture* tex = m_context->validateTextureBinding(target, true);
    if (!tex)
        return;
    if (!m_context->isGLES2NPOTStrict()) {
        if (level && WebGLTexture::isNPOT(width, height)) {
            context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
            return;
        }
    }
    context->compressedTexImage2D(target, level, internalformat, width, height,
                                  border, data->byteLength(), data->baseAddress());
    tex->setLevelInfo(target, level, internalformat, width, height, GraphicsContext3D::UNSIGNED_BYTE);
    m_context->cleanupAfterGraphicsCall(false);
}

void WebGLCompressedTextures::compressedTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
                                                                  GC3Dsizei width, GC3Dsizei height, GC3Denum format, ArrayBufferView* data)
{
    GraphicsContext3D* context = m_context->graphicsContext3D();
    if (m_context->isContextLost())
        return;
    if (!validateCompressedTexFormat(format)) {
        context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }
    if (!validateCompressedTexFuncData(width, height, format, data))
        return;

    WebGLTexture* tex = m_context->validateTextureBinding(target, true);
    if (!tex)
        return;

    if (format != tex->getInternalFormat(target, level)) {
        context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }

    if (!validateCompressedTexSubDimensions(target, level, xoffset, yoffset, width, height, format, tex)) {
        context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }

    context->compressedTexSubImage2D(target, level, xoffset, yoffset,
                                     width, height, format, data->byteLength(), data->baseAddress());
    m_context->cleanupAfterGraphicsCall(false);
}

WebGLGetInfo WebGLCompressedTextures::getCompressedTextureFormats()
{
    return WebGLGetInfo(Int32Array::create(&m_formats[0], m_formats.size()));
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
