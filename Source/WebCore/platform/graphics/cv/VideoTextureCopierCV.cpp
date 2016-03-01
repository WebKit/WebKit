/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "VideoTextureCopierCV.h"

#include "Logging.h"
#include <wtf/NeverDestroyed.h>

#if PLATFORM(IOS)
#include <OpenGLES/ES3/glext.h>
#endif

#include "CoreVideoSoftLink.h"

namespace WebCore {

VideoTextureCopierCV::VideoTextureCopierCV(GraphicsContext3D& context)
    : m_context(context)
    , m_readFramebuffer(context.createFramebuffer())
{
}

VideoTextureCopierCV::~VideoTextureCopierCV()
{
    m_context->deleteFramebuffer(m_readFramebuffer);
}

#if !LOG_DISABLED

#define STRINGIFY_PAIR(e) e, #e
static std::map<uint32_t, const char*>& enumToStringMap()
{
    static NeverDestroyed<std::map<uint32_t, const char*>> map;
    if (map.get().empty()) {
        std::map<uint32_t, const char*> stringMap;
        map.get().emplace(STRINGIFY_PAIR(GL_RGB));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA));
        map.get().emplace(STRINGIFY_PAIR(GL_LUMINANCE_ALPHA));
        map.get().emplace(STRINGIFY_PAIR(GL_LUMINANCE));
        map.get().emplace(STRINGIFY_PAIR(GL_ALPHA));
        map.get().emplace(STRINGIFY_PAIR(GL_R8));
        map.get().emplace(STRINGIFY_PAIR(GL_R16F));
        map.get().emplace(STRINGIFY_PAIR(GL_R32F));
        map.get().emplace(STRINGIFY_PAIR(GL_R8UI));
        map.get().emplace(STRINGIFY_PAIR(GL_R8I));
        map.get().emplace(STRINGIFY_PAIR(GL_R16UI));
        map.get().emplace(STRINGIFY_PAIR(GL_R16I));
        map.get().emplace(STRINGIFY_PAIR(GL_R32UI));
        map.get().emplace(STRINGIFY_PAIR(GL_R32I));
        map.get().emplace(STRINGIFY_PAIR(GL_RG8));
        map.get().emplace(STRINGIFY_PAIR(GL_RG16F));
        map.get().emplace(STRINGIFY_PAIR(GL_RG32F));
        map.get().emplace(STRINGIFY_PAIR(GL_RG8UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RG8I));
        map.get().emplace(STRINGIFY_PAIR(GL_RG16UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RG16I));
        map.get().emplace(STRINGIFY_PAIR(GL_RG32UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RG32I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB8));
        map.get().emplace(STRINGIFY_PAIR(GL_SRGB8));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA8));
        map.get().emplace(STRINGIFY_PAIR(GL_SRGB8_ALPHA8));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA4));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB10_A2));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_COMPONENT16));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_COMPONENT24));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_COMPONENT32F));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH24_STENCIL8));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH32F_STENCIL8));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA));
        map.get().emplace(STRINGIFY_PAIR(GL_LUMINANCE_ALPHA));
        map.get().emplace(STRINGIFY_PAIR(GL_LUMINANCE));
        map.get().emplace(STRINGIFY_PAIR(GL_ALPHA));
        map.get().emplace(STRINGIFY_PAIR(GL_RED));
        map.get().emplace(STRINGIFY_PAIR(GL_RG_INTEGER));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_STENCIL));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_BYTE));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_SHORT_5_6_5));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_SHORT_4_4_4_4));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_SHORT_5_5_5_1));
        map.get().emplace(STRINGIFY_PAIR(GL_BYTE));
        map.get().emplace(STRINGIFY_PAIR(GL_HALF_FLOAT));
        map.get().emplace(STRINGIFY_PAIR(GL_FLOAT));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_SHORT));
        map.get().emplace(STRINGIFY_PAIR(GL_SHORT));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT));
        map.get().emplace(STRINGIFY_PAIR(GL_INT));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT_2_10_10_10_REV));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT_24_8));
        map.get().emplace(STRINGIFY_PAIR(GL_FLOAT_32_UNSIGNED_INT_24_8_REV));

#if PLATFORM(IOS)
        map.get().emplace(STRINGIFY_PAIR(GL_RED_INTEGER));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB_INTEGER));
        map.get().emplace(STRINGIFY_PAIR(GL_RG8_SNORM));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB565));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB8_SNORM));
        map.get().emplace(STRINGIFY_PAIR(GL_R11F_G11F_B10F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB9_E5));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB16F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB32F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB8UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB8I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB16UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB16I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB32UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB32I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA8_SNORM));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA16F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA32F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA8UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA8I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB10_A2UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA16UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA16I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA32I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA32UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB5_A1));
        map.get().emplace(STRINGIFY_PAIR(GL_RG));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA_INTEGER));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_COMPONENT));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT_10F_11F_11F_REV));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT_5_9_9_9_REV));
#endif
    }
    return map.get();
}

#endif

bool VideoTextureCopierCV::copyVideoTextureToPlatformTexture(TextureType inputTexture, size_t width, size_t height, Platform3DObject outputTexture, GC3Denum outputTarget, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY)
{
    if (flipY || premultiplyAlpha)
        return false;

    if (!inputTexture)
        return false;

#if PLATFORM(IOS)
    Platform3DObject videoTextureName = CVOpenGLESTextureGetName(inputTexture);
    GC3Denum videoTextureTarget = CVOpenGLESTextureGetTarget(inputTexture);
#else
    Platform3DObject videoTextureName = CVOpenGLTextureGetName(inputTexture);
    GC3Denum videoTextureTarget = CVOpenGLTextureGetTarget(inputTexture);
#endif

    LOG(Media, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - internalFormat: %s, format: %s, type: %s", this, enumToStringMap()[internalFormat], enumToStringMap()[format], enumToStringMap()[type]);

    // Save the origial bound texture & framebuffer names so we can re-bind them after copying the video texture.
    GC3Dint boundTexture = 0;
    GC3Dint boundReadFramebuffer = 0;
    m_context->getIntegerv(GraphicsContext3D::TEXTURE_BINDING_2D, &boundTexture);
    m_context->getIntegerv(GraphicsContext3D::READ_FRAMEBUFFER_BINDING, &boundReadFramebuffer);

    m_context->bindTexture(videoTextureTarget, videoTextureName);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameterf(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texParameterf(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);

    // Make that framebuffer the read source from which drawing commands will read voxels.
    m_context->bindFramebuffer(GraphicsContext3D::READ_FRAMEBUFFER, m_readFramebuffer);

    // Allocate uninitialized memory for the output texture.
    m_context->bindTexture(outputTarget, outputTexture);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameterf(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texParameterf(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texImage2DDirect(outputTarget, level, internalFormat, width, height, 0, format, type, nullptr);

    // Attach the video texture to the framebuffer.
    m_context->framebufferTexture2D(GraphicsContext3D::READ_FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, videoTextureTarget, videoTextureName, level);

    GC3Denum status = m_context->checkFramebufferStatus(GraphicsContext3D::READ_FRAMEBUFFER);
    if (status != GraphicsContext3D::FRAMEBUFFER_COMPLETE)
        return false;

    // Copy texture from the read framebuffer (and thus the video texture) to the output texture.
    m_context->copyTexImage2D(outputTarget, level, internalFormat, 0, 0, width, height, 0);

    // Restore the previous texture and framebuffer bindings.
    m_context->bindTexture(outputTarget, boundTexture);
    m_context->bindFramebuffer(GraphicsContext3D::READ_FRAMEBUFFER, boundReadFramebuffer);

    return !m_context->getError();
}


}
