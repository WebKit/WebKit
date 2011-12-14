/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "TextureManager.h"

#include "LayerRendererChromium.h"

namespace WebCore {

static size_t memoryUseBytes(IntSize size, unsigned textureFormat)
{
    // FIXME: This assumes all textures are 4 bytes/pixel, like RGBA.
    return size.width() * size.height() * 4;
}

TextureManager::TextureManager(GraphicsContext3D* context, size_t memoryLimitBytes, int maxTextureSize)
    : m_context(context)
    , m_memoryLimitBytes(memoryLimitBytes)
    , m_memoryUseBytes(0)
    , m_maxTextureSize(maxTextureSize)
    , m_nextToken(1)
{
}

TextureToken TextureManager::getToken()
{
    return m_nextToken++;
}

void TextureManager::releaseToken(TextureToken token)
{
    TextureMap::iterator it = m_textures.find(token);
    if (it != m_textures.end())
        removeTexture(token, it->second);
}

bool TextureManager::hasTexture(TextureToken token)
{
    if (m_textures.contains(token)) {
        // If someone asks about a texture put it at the end of the LRU list.
        m_textureLRUSet.remove(token);
        m_textureLRUSet.add(token);
        return true;
    }
    return false;
}

bool TextureManager::isProtected(TextureToken token)
{
    return token && hasTexture(token) && m_textures.get(token).isProtected;
}

void TextureManager::protectTexture(TextureToken token)
{
    ASSERT(hasTexture(token));
    ASSERT(!m_textures.get(token).isProtected);
    TextureInfo info = m_textures.take(token);
    info.isProtected = true;
    m_textures.add(token, info);
}

void TextureManager::unprotectAllTextures()
{
    for (TextureMap::iterator it = m_textures.begin(); it != m_textures.end(); ++it)
        it->second.isProtected = false;
}

bool TextureManager::reduceMemoryToLimit(size_t limit)
{
    while (m_memoryUseBytes > limit) {
        ASSERT(!m_textureLRUSet.isEmpty());
        bool foundCandidate = false;
        for (ListHashSet<TextureToken>::iterator lruIt = m_textureLRUSet.begin(); lruIt != m_textureLRUSet.end(); ++lruIt) {
            TextureToken token = *lruIt;
            TextureInfo info = m_textures.get(token);
            if (info.isProtected)
                continue;
            removeTexture(token, info);
            foundCandidate = true;
            break;
        }
        if (!foundCandidate)
            return false;
    }
    return true;
}

void TextureManager::addTexture(TextureToken token, TextureInfo info)
{
    ASSERT(!m_textureLRUSet.contains(token));
    ASSERT(!m_textures.contains(token));
    m_memoryUseBytes += memoryUseBytes(info.size, info.format);
    m_textures.set(token, info);
    m_textureLRUSet.add(token);
}

void TextureManager::removeTexture(TextureToken token, TextureInfo info)
{
    ASSERT(m_textureLRUSet.contains(token));
    ASSERT(m_textures.contains(token));
    m_memoryUseBytes -= memoryUseBytes(info.size, info.format);
    m_textures.remove(token);
    ASSERT(m_textureLRUSet.contains(token));
    m_textureLRUSet.remove(token);
    GLC(m_context.get(), m_context->deleteTexture(info.textureId));
}

unsigned TextureManager::requestTexture(TextureToken token, IntSize size, unsigned format, bool* newTexture)
{
    if (size.width() > m_maxTextureSize || size.height() > m_maxTextureSize)
        return 0;

    TextureMap::iterator it = m_textures.find(token);
    if (it != m_textures.end()) {
        ASSERT(it->second.size != size || it->second.format != format);
        removeTexture(token, it->second);
    }

    size_t memoryRequiredBytes = memoryUseBytes(size, format);
    if (memoryRequiredBytes > m_memoryLimitBytes || !reduceMemoryToLimit(m_memoryLimitBytes - memoryRequiredBytes))
        return 0;

    unsigned textureId;
    GLC(m_context.get(), textureId = m_context->createTexture());
    GLC(m_context.get(), m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    // Do basic linear filtering on resize.
    GLC(m_context.get(), m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(m_context.get(), m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
    // NPOT textures in GL ES only work when the wrap mode is set to GraphicsContext3D::CLAMP_TO_EDGE.
    GLC(m_context.get(), m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
    GLC(m_context.get(), m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));
    GLC(m_context.get(), m_context->texImage2DResourceSafe(GraphicsContext3D::TEXTURE_2D, 0, format, size.width(), size.height(), 0, format, GraphicsContext3D::UNSIGNED_BYTE));
    TextureInfo info;
    info.size = size;
    info.format = format;
    info.textureId = textureId;
    info.isProtected = true;
    addTexture(token, info);
    return textureId;
}

}

#endif // USE(ACCELERATED_COMPOSITING)
