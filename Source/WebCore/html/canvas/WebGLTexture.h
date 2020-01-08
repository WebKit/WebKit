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

#pragma once

#if ENABLE(WEBGL)

#include "WebGLSharedObject.h"
#include <wtf/Vector.h>

namespace WebCore {

class WebGLTexture final : public WebGLSharedObject {
public:

    enum TextureExtensionFlag {
        TextureExtensionsDisabled = 0,
        TextureExtensionFloatLinearEnabled = 1 << 0,
        TextureExtensionHalfFloatLinearEnabled = 2 << 0
    };

    virtual ~WebGLTexture();

    static Ref<WebGLTexture> create(WebGLRenderingContextBase&);

    void setTarget(GCGLenum target, GCGLint maxLevel);
    void setParameteri(GCGLenum pname, GCGLint param);
    void setParameterf(GCGLenum pname, GCGLfloat param);

    GCGLenum getTarget() const { return m_target; }

    int getMinFilter() const { return m_minFilter; }

    void setLevelInfo(GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLenum type);

    bool canGenerateMipmaps();
    // Generate all level information.
    void generateMipmapLevelInfo();

    GCGLenum getInternalFormat(GCGLenum target, GCGLint level) const;
    GCGLenum getType(GCGLenum target, GCGLint level) const;
    GCGLsizei getWidth(GCGLenum target, GCGLint level) const;
    GCGLsizei getHeight(GCGLenum target, GCGLint level) const;
    bool isValid(GCGLenum target, GCGLint level) const;
    void markInvalid(GCGLenum target, GCGLint level);

    // Whether width/height is NotPowerOfTwo.
    static bool isNPOT(GCGLsizei, GCGLsizei);

    bool isNPOT() const;
    // Determine if texture sampling should always return [0, 0, 0, 1] (OpenGL ES 2.0 Sec 3.8.2).
    bool needToUseBlackTexture(TextureExtensionFlag) const;

    bool isCompressed() const;
    void setCompressed();

    bool hasEverBeenBound() const { return object() && m_target; }

    static GCGLint computeLevelCount(GCGLsizei width, GCGLsizei height);

    bool immutable() const { return m_immutable; }
    void setImmutable() { m_immutable = true; }

private:
    WebGLTexture(WebGLRenderingContextBase&);

    void deleteObjectImpl(GraphicsContextGLOpenGL*, PlatformGLObject) override;

    class LevelInfo {
    public:
        LevelInfo()
            : valid(false)
            , internalFormat(0)
            , width(0)
            , height(0)
            , type(0)
        {
        }

        void setInfo(GCGLenum internalFmt, GCGLsizei w, GCGLsizei h, GCGLenum tp)
        {
            valid = true;
            internalFormat = internalFmt;
            width = w;
            height = h;
            type = tp;
        }

        bool valid;
        GCGLenum internalFormat;
        GCGLsizei width;
        GCGLsizei height;
        GCGLenum type;
    };

    bool isTexture() const override { return true; }

    void update();

    int mapTargetToIndex(GCGLenum) const;

    const LevelInfo* getLevelInfo(GCGLenum target, GCGLint level) const;

    GCGLenum m_target;

    GCGLenum m_minFilter;
    GCGLenum m_magFilter;
    GCGLenum m_wrapS;
    GCGLenum m_wrapT;

    Vector<Vector<LevelInfo>> m_info;

    bool m_isNPOT;
    bool m_isComplete;
    bool m_needToUseBlackTexture;
    bool m_isCompressed;
    bool m_isFloatType;
    bool m_isHalfFloatType;
    bool m_isForWebGL1;
    bool m_immutable { false };
};

} // namespace WebCore

#endif
