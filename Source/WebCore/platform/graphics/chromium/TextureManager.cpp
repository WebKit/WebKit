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
#include "ManagedTexture.h"

using namespace std;

namespace WebCore {


namespace {
size_t memoryLimitBytes(size_t viewportMultiplier, const IntSize& viewportSize, size_t minMegabytes, size_t maxMegabytes)
{
    if (!viewportMultiplier)
        return maxMegabytes * 1024 * 1024;
    if (viewportSize.isEmpty())
        return minMegabytes * 1024 * 1024;
    return max(minMegabytes * 1024 * 1024, min(maxMegabytes * 1024 * 1024, viewportMultiplier * TextureManager::memoryUseBytes(viewportSize, GraphicsContext3D::RGBA)));
}
}

size_t TextureManager::highLimitBytes(const IntSize& viewportSize)
{
    size_t viewportMultiplier, minMegabytes, maxMegabytes;
#if OS(ANDROID)
    viewportMultiplier = 16;
    minMegabytes = 32;
    maxMegabytes = 64;
#else
    viewportMultiplier = 0;
    minMegabytes = 0;
    maxMegabytes = 128;
#endif
    return memoryLimitBytes(viewportMultiplier, viewportSize, minMegabytes, maxMegabytes);
}

size_t TextureManager::reclaimLimitBytes(const IntSize& viewportSize)
{
    size_t viewportMultiplier, minMegabytes, maxMegabytes;
#if OS(ANDROID)
    viewportMultiplier = 8;
    minMegabytes = 16;
    maxMegabytes = 32;
#else
    viewportMultiplier = 0;
    minMegabytes = 0;
    maxMegabytes = 64;
#endif
    return memoryLimitBytes(viewportMultiplier, viewportSize, minMegabytes, maxMegabytes);
}

size_t TextureManager::memoryUseBytes(const IntSize& size, GC3Denum textureFormat)
{
    // FIXME: This assumes all textures are 1 byte/component.
    const GC3Denum type = GraphicsContext3D::UNSIGNED_BYTE;
    unsigned int componentsPerPixel = 4;
    unsigned int bytesPerComponent = 1;
    if (!GraphicsContext3D::computeFormatAndTypeParameters(textureFormat, type, &componentsPerPixel, &bytesPerComponent))
        ASSERT_NOT_REACHED();

    return size.width() * size.height() * componentsPerPixel * bytesPerComponent;
}


TextureManager::TextureManager(size_t maxMemoryLimitBytes, size_t preferredMemoryLimitBytes, int maxTextureSize)
    : m_maxMemoryLimitBytes(maxMemoryLimitBytes)
    , m_preferredMemoryLimitBytes(preferredMemoryLimitBytes)
    , m_memoryUseBytes(0)
    , m_maxTextureSize(maxTextureSize)
    , m_nextToken(1)
{
}

TextureManager::~TextureManager()
{
    for (HashSet<ManagedTexture*>::iterator it = m_registeredTextures.begin(); it != m_registeredTextures.end(); ++it)
        (*it)->clearManager();
}

void TextureManager::setMemoryAllocationLimitBytes(size_t memoryLimitBytes)
{
    setMaxMemoryLimitBytes(memoryLimitBytes);
#if defined(OS_ANDROID)
    // On android, we are setting the preferred memory limit to half of our
    // maximum allocation, because we would like to stay significantly below
    // the absolute memory limit whenever we can. Specifically, by limitting
    // prepainting only to the halfway memory mark.
    setPreferredMemoryLimitBytes(memoryLimitBytes / 2);
#else
    setPreferredMemoryLimitBytes(memoryLimitBytes);
#endif
}

void TextureManager::setMaxMemoryLimitBytes(size_t memoryLimitBytes)
{
    reduceMemoryToLimit(memoryLimitBytes);
    ASSERT(currentMemoryUseBytes() <= memoryLimitBytes);
    m_maxMemoryLimitBytes = memoryLimitBytes;
}

void TextureManager::setPreferredMemoryLimitBytes(size_t memoryLimitBytes)
{
    m_preferredMemoryLimitBytes = memoryLimitBytes;
}

void TextureManager::registerTexture(ManagedTexture* texture)
{
    ASSERT(texture);
    ASSERT(!m_registeredTextures.contains(texture));

    m_registeredTextures.add(texture);
}

void TextureManager::unregisterTexture(ManagedTexture* texture)
{
    ASSERT(texture);
    ASSERT(m_registeredTextures.contains(texture));

    m_registeredTextures.remove(texture);
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
    return m_textures.contains(token);
}

bool TextureManager::isProtected(TextureToken token)
{
    return token && hasTexture(token) && m_textures.get(token).isProtected;
}

void TextureManager::protectTexture(TextureToken token)
{
    ASSERT(hasTexture(token));
    TextureInfo info = m_textures.take(token);
    info.isProtected = true;
    m_textures.add(token, info);
    // If someone protects a texture, put it at the end of the LRU list.
    m_textureLRUSet.remove(token);
    m_textureLRUSet.add(token);
}

void TextureManager::unprotectTexture(TextureToken token)
{
    TextureMap::iterator it = m_textures.find(token);
    if (it != m_textures.end())
        it->second.isProtected = false;
}

void TextureManager::unprotectAllTextures()
{
    for (TextureMap::iterator it = m_textures.begin(); it != m_textures.end(); ++it)
        it->second.isProtected = false;
}

void TextureManager::evictTexture(TextureToken token, TextureInfo info)
{
    TRACE_EVENT("TextureManager::evictTexture", this, 0);
    removeTexture(token, info);
}

void TextureManager::reduceMemoryToLimit(size_t limit)
{
    while (m_memoryUseBytes > limit) {
        ASSERT(!m_textureLRUSet.isEmpty());
        bool foundCandidate = false;
        for (ListHashSet<TextureToken>::iterator lruIt = m_textureLRUSet.begin(); lruIt != m_textureLRUSet.end(); ++lruIt) {
            TextureToken token = *lruIt;
            TextureInfo info = m_textures.get(token);
            if (info.isProtected)
                continue;
            evictTexture(token, info);
            foundCandidate = true;
            break;
        }
        if (!foundCandidate)
            return;
    }
}

void TextureManager::addTexture(TextureToken token, TextureInfo info)
{
    ASSERT(!m_textureLRUSet.contains(token));
    ASSERT(!m_textures.contains(token));
    m_memoryUseBytes += memoryUseBytes(info.size, info.format);
    m_textures.set(token, info);
    m_textureLRUSet.add(token);
}

void TextureManager::deleteEvictedTextures(TextureAllocator* allocator)
{
    if (allocator) {
        for (size_t i = 0; i < m_evictedTextures.size(); ++i) {
            if (m_evictedTextures[i].textureId) {
#ifndef NDEBUG
                ASSERT(m_evictedTextures[i].allocator == allocator);
#endif
                allocator->deleteTexture(m_evictedTextures[i].textureId, m_evictedTextures[i].size, m_evictedTextures[i].format);
            }
        }
    }
    m_evictedTextures.clear();
}

void TextureManager::evictAndDeleteAllTextures(TextureAllocator* allocator)
{
    unprotectAllTextures();
    reduceMemoryToLimit(0);
    deleteEvictedTextures(allocator);
}

void TextureManager::removeTexture(TextureToken token, TextureInfo info)
{
    ASSERT(m_textureLRUSet.contains(token));
    ASSERT(m_textures.contains(token));
    m_memoryUseBytes -= memoryUseBytes(info.size, info.format);
    m_textures.remove(token);
    ASSERT(m_textureLRUSet.contains(token));
    m_textureLRUSet.remove(token);
    EvictionEntry entry;
    entry.textureId = info.textureId;
    entry.size = info.size;
    entry.format = info.format;
#ifndef NDEBUG
    entry.allocator = info.allocator;
#endif
    m_evictedTextures.append(entry);
}

unsigned TextureManager::allocateTexture(TextureAllocator* allocator, TextureToken token)
{
    TextureMap::iterator it = m_textures.find(token);
    ASSERT(it != m_textures.end());
    TextureInfo* info = &it.get()->second;
    ASSERT(info->isProtected);

    unsigned textureId = allocator->createTexture(info->size, info->format);
    info->textureId = textureId;
#ifndef NDEBUG
    info->allocator = allocator;
#endif
    return textureId;
}

bool TextureManager::requestTexture(TextureToken token, IntSize size, unsigned format)
{
    if (size.width() > m_maxTextureSize || size.height() > m_maxTextureSize)
        return false;

    TextureMap::iterator it = m_textures.find(token);
    if (it != m_textures.end()) {
        ASSERT(it->second.size != size || it->second.format != format);
        removeTexture(token, it->second);
    }

    size_t memoryRequiredBytes = memoryUseBytes(size, format);
    if (memoryRequiredBytes > m_maxMemoryLimitBytes)
        return false;

    reduceMemoryToLimit(m_maxMemoryLimitBytes - memoryRequiredBytes);
    if (m_memoryUseBytes + memoryRequiredBytes > m_maxMemoryLimitBytes)
        return false;

    TextureInfo info;
    info.size = size;
    info.format = format;
    info.textureId = 0;
    info.isProtected = true;
#ifndef NDEBUG
    info.allocator = 0;
#endif
    addTexture(token, info);
    return true;
}

}

#endif // USE(ACCELERATED_COMPOSITING)
