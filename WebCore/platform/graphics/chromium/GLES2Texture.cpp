/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(GLES2_RENDERING)

#include "GLES2Texture.h"

#include <GLES2/gl2.h>

#include <wtf/OwnArrayPtr.h>

namespace WebCore {

GLES2Texture::GLES2Texture(unsigned int textureId, Format format, int width, int height)
    : m_textureId(textureId)
    , m_format(format)
    , m_width(width)
    , m_height(height)
{
}

GLES2Texture::~GLES2Texture()
{
    glDeleteTextures(1, &m_textureId);
}

PassRefPtr<GLES2Texture> GLES2Texture::create(Format format, int width, int height)
{
    GLuint textureId;
    glGenTextures(1, &textureId);
    if (!textureId)
        return 0;

    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    int max;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
    if (width > max || height > max)
        ASSERT(!"texture too big");

    return adoptRef(new GLES2Texture(textureId, format, width, height));
}

static void convertFormat(GLES2Texture::Format format, unsigned int* glFormat, unsigned int* glType, bool* swizzle)
{
    *swizzle = false;
    switch (format) {
    case GLES2Texture::RGBA8:
        *glFormat = GL_RGBA;
        *glType = GL_UNSIGNED_BYTE;
        break;
    case GLES2Texture::BGRA8:
// FIXME:  Once we have support for extensions, we should check for EXT_texture_format_BGRA8888,
// and use that if present.
        *glFormat = GL_RGBA;
        *glType = GL_UNSIGNED_BYTE;
        *swizzle = true;
        break;
    default:
        ASSERT(!"bad format");
        break;
    }
}

void GLES2Texture::load(void* pixels)
{
    unsigned int glFormat, glType;
    bool swizzle;
    convertFormat(m_format, &glFormat, &glType, &swizzle);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    if (swizzle) {
        ASSERT(glFormat == GL_RGBA && glType == GL_UNSIGNED_BYTE);
        // FIXME:  This could use PBO's to save doing an extra copy here.
        int size = m_width * m_height;
        unsigned* pixels32 = static_cast<unsigned*>(pixels);
        OwnArrayPtr<unsigned> buf(new unsigned[size]);
        unsigned* bufptr = buf.get();
        for (int i = 0; i < size; ++i) {
            unsigned pixel = pixels32[i];
            bufptr[i] = pixel & 0xFF00FF00 | ((pixel & 0x00FF0000) >> 16) | ((pixel & 0x000000FF) << 16);
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, glFormat, glType, buf.get());
    } else
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, glFormat, glType, pixels);
}

void GLES2Texture::bind()
{
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

}

#endif
