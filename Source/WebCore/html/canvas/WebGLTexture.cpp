/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(WEBGL)

#include "WebGLTexture.h"

#include "WebGLContextGroup.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderingContextBase.h"

namespace WebCore {

Ref<WebGLTexture> WebGLTexture::create(WebGLRenderingContextBase& ctx)
{
    return adoptRef(*new WebGLTexture(ctx));
}

WebGLTexture::WebGLTexture(WebGLRenderingContextBase& ctx)
    : WebGLSharedObject(ctx)
    , m_target(0)
    , m_minFilter(GraphicsContextGL::NEAREST_MIPMAP_LINEAR)
    , m_magFilter(GraphicsContextGL::LINEAR)
    , m_wrapS(GraphicsContextGL::REPEAT)
    , m_wrapT(GraphicsContextGL::REPEAT)
    , m_isNPOT(false)
    , m_isComplete(false)
    , m_needToUseBlackTexture(false)
    , m_isCompressed(false)
    , m_isFloatType(false)
    , m_isHalfFloatType(false)
    , m_isForWebGL1(ctx.isWebGL1())
{
    setObject(ctx.graphicsContextGL()->createTexture());
}

WebGLTexture::~WebGLTexture()
{
    deleteObject(0);
}

void WebGLTexture::setTarget(GCGLenum target, GCGLint maxLevel)
{
    if (!object())
        return;
    // Target is finalized the first time bindTexture() is called.
    if (m_target)
        return;
    switch (target) {
    case GraphicsContextGL::TEXTURE_2D:
        m_target = target;
        m_info.resize(1);
        m_info[0].resize(maxLevel);
        break;
    case GraphicsContextGL::TEXTURE_CUBE_MAP:
        m_target = target;
        m_info.resize(6);
        for (int ii = 0; ii < 6; ++ii)
            m_info[ii].resize(maxLevel);
        break;
    }
}

void WebGLTexture::setParameteri(GCGLenum pname, GCGLint param)
{
    if (!object() || !m_target)
        return;
    switch (pname) {
    case GraphicsContextGL::TEXTURE_MIN_FILTER:
        switch (param) {
        case GraphicsContextGL::NEAREST:
        case GraphicsContextGL::LINEAR:
        case GraphicsContextGL::NEAREST_MIPMAP_NEAREST:
        case GraphicsContextGL::LINEAR_MIPMAP_NEAREST:
        case GraphicsContextGL::NEAREST_MIPMAP_LINEAR:
        case GraphicsContextGL::LINEAR_MIPMAP_LINEAR:
            m_minFilter = param;
            break;
        }
        break;
    case GraphicsContextGL::TEXTURE_MAG_FILTER:
        switch (param) {
        case GraphicsContextGL::NEAREST:
        case GraphicsContextGL::LINEAR:
            m_magFilter = param;
            break;
        }
        break;
    case GraphicsContextGL::TEXTURE_WRAP_S:
        switch (param) {
        case GraphicsContextGL::CLAMP_TO_EDGE:
        case GraphicsContextGL::MIRRORED_REPEAT:
        case GraphicsContextGL::REPEAT:
            m_wrapS = param;
            break;
        }
        break;
    case GraphicsContextGL::TEXTURE_WRAP_T:
        switch (param) {
        case GraphicsContextGL::CLAMP_TO_EDGE:
        case GraphicsContextGL::MIRRORED_REPEAT:
        case GraphicsContextGL::REPEAT:
            m_wrapT = param;
            break;
        }
        break;
    default:
        return;
    }
    update();
}

void WebGLTexture::setParameterf(GCGLenum pname, GCGLfloat param)
{
    if (!object() || !m_target)
        return;
    GCGLint iparam = static_cast<GCGLint>(param);
    setParameteri(pname, iparam);
}

void WebGLTexture::setLevelInfo(GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLenum type)
{
    if (!object() || !m_target)
        return;
    // We assume level, internalFormat, width, height, and type have all been
    // validated already.
    int index = mapTargetToIndex(target);
    if (index < 0)
        return;
    m_info[index][level].setInfo(internalFormat, width, height, type);
    update();
}

void WebGLTexture::generateMipmapLevelInfo()
{
    if (!object() || !m_target)
        return;
    if (!canGenerateMipmaps())
        return;
    if (!m_isComplete) {
        for (size_t ii = 0; ii < m_info.size(); ++ii) {
            const LevelInfo& info0 = m_info[ii][0];
            GCGLsizei width = info0.width;
            GCGLsizei height = info0.height;
            GCGLint levelCount = computeLevelCount(width, height);
            for (GCGLint level = 1; level < levelCount; ++level) {
                width = std::max(1, width >> 1);
                height = std::max(1, height >> 1);
                LevelInfo& info = m_info[ii][level];
                info.setInfo(info0.internalFormat, width, height, info0.type);
            }
        }
        m_isComplete = true;
    }
    m_needToUseBlackTexture = false;
}

GCGLenum WebGLTexture::getInternalFormat(GCGLenum target, GCGLint level) const
{
    const LevelInfo* info = getLevelInfo(target, level);
    if (!info)
        return 0;
    return info->internalFormat;
}

GCGLenum WebGLTexture::getType(GCGLenum target, GCGLint level) const
{
    ASSERT(m_isForWebGL1);
    const LevelInfo* info = getLevelInfo(target, level);
    if (!info)
        return 0;
    return info->type;
}

GCGLsizei WebGLTexture::getWidth(GCGLenum target, GCGLint level) const
{
    const LevelInfo* info = getLevelInfo(target, level);
    if (!info)
        return 0;
    return info->width;
}

GCGLsizei WebGLTexture::getHeight(GCGLenum target, GCGLint level) const
{
    const LevelInfo* info = getLevelInfo(target, level);
    if (!info)
        return 0;
    return info->height;
}

bool WebGLTexture::isValid(GCGLenum target, GCGLint level) const
{
    const LevelInfo* info = getLevelInfo(target, level);
    if (!info)
        return 0;
    return info->valid;
}

void WebGLTexture::markInvalid(GCGLenum target, GCGLint level)
{
    int index = mapTargetToIndex(target);
    if (index < 0)
        return;
    m_info[index][level].valid = false;
    update();
}

bool WebGLTexture::isNPOT(GCGLsizei width, GCGLsizei height)
{
    ASSERT(width >= 0 && height >= 0);
    if (!width || !height)
        return false;
    if ((width & (width - 1)) || (height & (height - 1)))
        return true;
    return false;
}

bool WebGLTexture::isNPOT() const
{
    if (!object())
        return false;
    return m_isNPOT;
}

bool WebGLTexture::needToUseBlackTexture(TextureExtensionFlag extensions) const
{
    if (!object())
        return false;
    if (m_needToUseBlackTexture)
        return true;
    if (m_magFilter == GraphicsContextGL::NEAREST && (m_minFilter == GraphicsContextGL::NEAREST || m_minFilter == GraphicsContextGL::NEAREST_MIPMAP_NEAREST))
        return false;
    if (m_isForWebGL1 && m_isHalfFloatType && !(extensions & TextureExtensionHalfFloatLinearEnabled))
        return true;
    if (m_isFloatType && !(extensions & TextureExtensionFloatLinearEnabled))
        return true;
    return false;
}

bool WebGLTexture::isCompressed() const
{
    if (!object())
        return false;
    return m_isCompressed;
}

void WebGLTexture::setCompressed()
{
    ASSERT(object());
    m_isCompressed = true;
}

void WebGLTexture::deleteObjectImpl(GraphicsContextGLOpenGL* context3d, PlatformGLObject object)
{
    context3d->deleteTexture(object);
}

int WebGLTexture::mapTargetToIndex(GCGLenum target) const
{
    if (m_target == GraphicsContextGL::TEXTURE_2D) {
        if (target == GraphicsContextGL::TEXTURE_2D)
            return 0;
    } else if (m_target == GraphicsContextGL::TEXTURE_CUBE_MAP) {
        switch (target) {
        case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_X:
            return 0;
        case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_X:
            return 1;
        case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Y:
            return 2;
        case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Y:
            return 3;
        case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Z:
            return 4;
        case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Z:
            return 5;
        }
    }
    return -1;
}

bool WebGLTexture::canGenerateMipmaps()
{
    if (isNPOT())
        return false;
    const LevelInfo& first = m_info[0][0];
    for (size_t ii = 0; ii < m_info.size(); ++ii) {
        const LevelInfo& info = m_info[ii][0];
        if (!info.valid
            || info.width != first.width || info.height != first.height
            || info.internalFormat != first.internalFormat || (m_isForWebGL1 && info.type != first.type))
            return false;
    }
    return true;
}

GCGLint WebGLTexture::computeLevelCount(GCGLsizei width, GCGLsizei height)
{
    // return 1 + log2Floor(std::max(width, height));
    GCGLsizei n = std::max(width, height);
    if (n <= 0)
        return 0;
    GCGLint log = 0;
    GCGLsizei value = n;
    for (int ii = 4; ii >= 0; --ii) {
        int shift = (1 << ii);
        GCGLsizei x = (value >> shift);
        if (x) {
            value = x;
            log += shift;
        }
    }
    ASSERT(value == 1);
    return log + 1;
}

static bool internalFormatIsFloatType(GCGLenum internalFormat)
{
    switch (internalFormat) {
    case GraphicsContextGL::R32F:
    case GraphicsContextGL::RG32F:
    case GraphicsContextGL::RGB32F:
    case GraphicsContextGL::RGBA32F:
    case GraphicsContextGL::DEPTH_COMPONENT32F:
    case GraphicsContextGL::DEPTH32F_STENCIL8:
        return true;
    default:
        return false;
    }
}

static bool internalFormatIsHalfFloatType(GCGLenum internalFormat)
{
    switch (internalFormat) {
    case GraphicsContextGL::R16F:
    case GraphicsContextGL::RG16F:
    case GraphicsContextGL::R11F_G11F_B10F:
    case GraphicsContextGL::RGB9_E5:
    case GraphicsContextGL::RGB16F:
    case GraphicsContextGL::RGBA16F:
        return true;
    default:
        return false;
    }
}

void WebGLTexture::update()
{
    m_isNPOT = false;
    for (size_t ii = 0; ii < m_info.size(); ++ii) {
        if (isNPOT(m_info[ii][0].width, m_info[ii][0].height)) {
            m_isNPOT = true;
            break;
        }
    }
    m_isComplete = true;
    const LevelInfo& first = m_info[0][0];
    GCGLint levelCount = computeLevelCount(first.width, first.height);
    if (levelCount < 1)
        m_isComplete = false;
    else {
        for (size_t ii = 0; ii < m_info.size() && m_isComplete; ++ii) {
            const LevelInfo& info0 = m_info[ii][0];
            if (!info0.valid
                || info0.width != first.width || info0.height != first.height
                || info0.internalFormat != first.internalFormat || (m_isForWebGL1 && info0.type != first.type)) {
                m_isComplete = false;
                break;
            }
            GCGLsizei width = info0.width;
            GCGLsizei height = info0.height;
            for (GCGLint level = 1; level < levelCount; ++level) {
                width = std::max(1, width >> 1);
                height = std::max(1, height >> 1);
                const LevelInfo& info = m_info[ii][level];
                if (!info.valid
                    || info.width != width || info.height != height
                    || info.internalFormat != info0.internalFormat || (m_isForWebGL1 && info.type != info0.type)) {
                    m_isComplete = false;
                    break;
                }

            }
        }
    }

    m_isFloatType = false;
    if (m_isForWebGL1) {
        if (m_isComplete) {
            if (m_isForWebGL1)
                m_isFloatType = m_info[0][0].type == GraphicsContextGL::FLOAT;
            else
                m_isFloatType = internalFormatIsFloatType(m_info[0][0].internalFormat);
        } else {
            for (size_t ii = 0; ii < m_info.size(); ++ii) {
                if ((m_isForWebGL1 && m_info[ii][0].type == GraphicsContextGL::FLOAT)
                    || (!m_isForWebGL1 && internalFormatIsFloatType(m_info[ii][0].internalFormat))) {
                    m_isFloatType = true;
                    break;
                }
            }
        }
    }

    m_isHalfFloatType = false;
    if (m_isForWebGL1) {
        if (m_isComplete) {
            if (m_isForWebGL1)
                m_isHalfFloatType = internalFormatIsHalfFloatType(m_info[0][0].internalFormat);
            else
                m_isHalfFloatType = m_info[0][0].type == GraphicsContextGL::HALF_FLOAT_OES;
        } else {
            for (size_t ii = 0; ii < m_info.size(); ++ii) {
                if ((m_isForWebGL1 && m_info[ii][0].type == GraphicsContextGL::HALF_FLOAT_OES)
                    || (!m_isForWebGL1 && internalFormatIsHalfFloatType(m_info[ii][0].internalFormat))) {
                    m_isHalfFloatType = true;
                    break;
                }
            }
        }
    }

    m_needToUseBlackTexture = false;
    // NPOT
    if (m_isNPOT && ((m_minFilter != GraphicsContextGL::NEAREST && m_minFilter != GraphicsContextGL::LINEAR)
        || m_wrapS != GraphicsContextGL::CLAMP_TO_EDGE || m_wrapT != GraphicsContextGL::CLAMP_TO_EDGE))
        m_needToUseBlackTexture = true;
    // Completeness
    if (!m_isComplete && m_minFilter != GraphicsContextGL::NEAREST && m_minFilter != GraphicsContextGL::LINEAR)
        m_needToUseBlackTexture = true;
}

const WebGLTexture::LevelInfo* WebGLTexture::getLevelInfo(GCGLenum target, GCGLint level) const
{
    if (!object() || !m_target)
        return 0;
    int targetIndex = mapTargetToIndex(target);
    if (targetIndex < 0 || targetIndex >= static_cast<int>(m_info.size()))
        return 0;
    if (level < 0 || level >= static_cast<GCGLint>(m_info[targetIndex].size()))
        return 0;
    return &(m_info[targetIndex][level]);
}

}

#endif // ENABLE(WEBGL)
