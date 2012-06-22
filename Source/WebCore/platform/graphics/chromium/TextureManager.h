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

#ifndef TextureManager_h
#define TextureManager_h

#include "GraphicsContext3D.h"
#include "IntRect.h"
#include "IntSize.h"

#include <wtf/FastAllocBase.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class ManagedTexture;
typedef int TextureToken;

class TextureAllocator {
public:
    virtual unsigned createTexture(const IntSize&, GC3Denum format) = 0;
    virtual void deleteTexture(unsigned texture, const IntSize&, GC3Denum) = 0;

protected:
    virtual ~TextureAllocator() { }
};

class TextureManager {
    WTF_MAKE_NONCOPYABLE(TextureManager);
public:
    static PassOwnPtr<TextureManager> create(size_t maxMemoryLimitBytes, size_t preferredMemoryLimitBytes, int maxTextureSize)
    {
        return adoptPtr(new TextureManager(maxMemoryLimitBytes, preferredMemoryLimitBytes, maxTextureSize));
    }
    ~TextureManager();

    // Absolute maximum limit for texture allocations for this instance.
    static size_t highLimitBytes(const IntSize& viewportSize);
    // Preferred texture size limit given the viewport size.
    static size_t reclaimLimitBytes(const IntSize& viewportSize);

    static size_t memoryUseBytes(const IntSize&, GC3Denum format);

    void setMemoryAllocationLimitBytes(size_t);
    void setMaxMemoryLimitBytes(size_t);
    size_t maxMemoryLimitBytes() { return m_maxMemoryLimitBytes; }
    void setPreferredMemoryLimitBytes(size_t);
    size_t preferredMemoryLimitBytes() { return m_preferredMemoryLimitBytes; }

    void registerTexture(ManagedTexture*);
    void unregisterTexture(ManagedTexture*);

    TextureToken getToken();
    void releaseToken(TextureToken);
    bool hasTexture(TextureToken);

    bool requestTexture(TextureToken, IntSize, GC3Denum textureFormat);

    void protectTexture(TextureToken);
    void unprotectTexture(TextureToken);
    void unprotectAllTextures();
    bool isProtected(TextureToken);

    unsigned allocateTexture(TextureAllocator*, TextureToken);
    void deleteEvictedTextures(TextureAllocator*);

    void evictAndDeleteAllTextures(TextureAllocator*);

    void reduceMemoryToLimit(size_t);
    size_t currentMemoryUseBytes() const { return m_memoryUseBytes; }

private:
    TextureManager(size_t maxMemoryLimitBytes, size_t preferredMemoryLimitBytes, int maxTextureSize);

    struct TextureInfo {
        IntSize size;
        GC3Denum format;
        unsigned textureId;
        bool isProtected;
#ifndef NDEBUG
        TextureAllocator* allocator;
#endif
    };

    void addTexture(TextureToken, TextureInfo);
    void removeTexture(TextureToken, TextureInfo);
    void evictTexture(TextureToken, TextureInfo);

    HashSet<ManagedTexture*> m_registeredTextures;

    typedef HashMap<TextureToken, TextureInfo> TextureMap;
    TextureMap m_textures;
    ListHashSet<TextureToken> m_textureLRUSet;

    size_t m_maxMemoryLimitBytes;
    size_t m_preferredMemoryLimitBytes;
    size_t m_memoryUseBytes;
    int m_maxTextureSize;
    TextureToken m_nextToken;

    struct EvictionEntry {
        unsigned textureId;
        IntSize size;
        GC3Denum format;
#ifndef NDEBUG
        TextureAllocator* allocator;
#endif
    };

    Vector<EvictionEntry> m_evictedTextures;
};

}

#endif
