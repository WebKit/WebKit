/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2014 Igalia S.L.
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

#pragma once

#include "ClipStack.h"
#include "FilterOperation.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "IntSize.h"
#include "TextureMapper.h"
#include "TextureMapperContextAttributes.h"
#include "TextureMapperGLHeaders.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class GraphicsLayer;
class NativeImage;

#if OS(WINDOWS)
#define USE_TEXMAP_DEPTH_STENCIL_BUFFER 1
#else
#define USE_TEXMAP_DEPTH_STENCIL_BUFFER 0
#endif

class BitmapTexture final : public RefCounted<BitmapTexture> {
public:
    enum Flag {
        NoFlag = 0,
        SupportsAlpha = 1 << 0,
        DepthBuffer = 1 << 1,
    };

    typedef unsigned Flags;

    static Ref<BitmapTexture> create(const TextureMapperContextAttributes& contextAttributes, const Flags flags = NoFlag, GLint internalFormat = GL_DONT_CARE)
    {
        return adoptRef(*new BitmapTexture(contextAttributes, flags, internalFormat));
    }

    WEBCORE_EXPORT ~BitmapTexture();

    IntSize size() const;
    bool isValid() const;
    Flags flags() const { return m_flags; }
    GLint internalFormat() const { return m_internalFormat; }
    bool isOpaque() const { return !(m_flags & SupportsAlpha); }

    void bindAsSurface();
    void initializeStencil();
    void initializeDepthBuffer();
    uint32_t id() const { return m_id; }
    uint32_t textureTarget() const { return GL_TEXTURE_2D; }
    IntSize textureSize() const { return m_textureSize; }

    void updateContents(NativeImage*, const IntRect&, const IntPoint& offset);
    void updateContents(GraphicsLayer*, const IntRect& target, const IntPoint& offset, float scale = 1);
    void updateContents(const void*, const IntRect& target, const IntPoint& offset, int bytesPerLine);

    void reset(const IntSize& size, Flags flags = 0)
    {
        m_flags = flags;
        m_contentSize = size;
        didReset();
    }
    void didReset();

    const IntSize& contentSize() const { return m_contentSize; }
    int numberOfBytes() const { return size().width() * size().height() * 32 >> 3; }

    RefPtr<BitmapTexture> applyFilters(TextureMapper&, const FilterOperations&, bool defersLastFilterPass);
    struct FilterInfo {
        RefPtr<const FilterOperation> filter;

        FilterInfo(RefPtr<const FilterOperation>&& f = nullptr)
            : filter(WTFMove(f))
            { }
    };
    const FilterInfo* filterInfo() const { return &m_filterInfo; }
    void setFilterInfo(FilterInfo&& filterInfo) { m_filterInfo = WTFMove(filterInfo); }

    ClipStack& clipStack() { return m_clipStack; }

    void copyFromExternalTexture(GLuint textureID);

    TextureMapper::Flags colorConvertFlags() const { return m_colorConvertFlags; }

private:
    BitmapTexture(const TextureMapperContextAttributes&, const Flags, GLint internalFormat);

    void clearIfNeeded();
    void createFboIfNeeded();

    Flags m_flags { 0 };
    IntSize m_contentSize;
    GLuint m_id { 0 };
    IntSize m_textureSize;
    GLuint m_fbo { 0 };
#if !USE(TEXMAP_DEPTH_STENCIL_BUFFER)
    GLuint m_rbo { 0 };
#endif
    GLuint m_depthBufferObject { 0 };
    bool m_shouldClear { true };
    ClipStack m_clipStack;
    TextureMapperContextAttributes m_contextAttributes;
    TextureMapper::Flags m_colorConvertFlags { TextureMapper::NoFlag };
    FilterInfo m_filterInfo;
    GLint m_internalFormat { 0 };
    GLenum m_format { 0 };
    GLenum m_type {
#if OS(DARWIN)
        GL_UNSIGNED_INT_8_8_8_8_REV
#else
        GL_UNSIGNED_BYTE
#endif
    };
};

} // namespace WebCore
