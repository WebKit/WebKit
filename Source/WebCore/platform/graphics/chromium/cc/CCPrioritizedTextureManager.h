/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#ifndef CCPrioritizedTextureManager_h
#define CCPrioritizedTextureManager_h

#include "CCPrioritizedTexture.h"
#include "CCPriorityCalculator.h"
#include "GraphicsContext3D.h"
#include "IntRect.h"
#include "IntSize.h"
#include "TextureManager.h"

namespace WebCore {

class CCPrioritizedTexture;
class CCPriorityCalculator;

class CCPrioritizedTextureManager {
    WTF_MAKE_NONCOPYABLE(CCPrioritizedTextureManager);
public:
    static PassOwnPtr<CCPrioritizedTextureManager> create(size_t maxMemoryLimitBytes, int maxTextureSize)
    {
        return adoptPtr(new CCPrioritizedTextureManager(maxMemoryLimitBytes, maxTextureSize));
    }
    PassOwnPtr<CCPrioritizedTexture> createTexture(IntSize size, GC3Denum format)
    {
        return adoptPtr(new CCPrioritizedTexture(this, size, format));
    }
    ~CCPrioritizedTextureManager();

    // memoryUseBytes() describes the number of bytes used by existing allocated textures.
    // memoryAboveCutoffBytes() describes the number of bytes that would be used if all
    // textures that are above the cutoff were allocated.
    // memoryUseBytes() <= memoryAboveCutoffBytes() should always be true.
    size_t memoryUseBytes() const { return m_memoryUseBytes; }
    size_t memoryAboveCutoffBytes() const { return m_memoryAboveCutoffBytes; }
    size_t memoryForRenderSurfacesBytes() const { return m_maxMemoryLimitBytes - m_memoryAvailableBytes; }

    void setMaxMemoryLimitBytes(size_t bytes) { m_maxMemoryLimitBytes = bytes; }
    size_t maxMemoryLimitBytes() const { return m_maxMemoryLimitBytes; }

    void prioritizeTextures(size_t renderSurfacesMemoryBytes);
    void clearPriorities();

    bool requestLate(CCPrioritizedTexture*);

    void reduceMemory(TextureAllocator*);
    void clearAllMemory(TextureAllocator*);
    void allBackingTexturesWereDeleted();

    void acquireBackingTextureIfNeeded(CCPrioritizedTexture*, TextureAllocator*);

    void registerTexture(CCPrioritizedTexture*);
    void unregisterTexture(CCPrioritizedTexture*);
    void returnBackingTexture(CCPrioritizedTexture*);

#if !ASSERT_DISABLED
    void assertInvariants();
#endif

private:
    // Compare textures. Highest priority first.
    static inline bool compareTextures(CCPrioritizedTexture* a, CCPrioritizedTexture* b)
    {
        if (a->requestPriority() == b->requestPriority())
            return a < b;
        return CCPriorityCalculator::priorityIsHigher(a->requestPriority(), b->requestPriority());
    }
    // Compare backings. Lowest priority first.
    static inline bool compareBackings(CCPrioritizedTexture::Backing* a, CCPrioritizedTexture::Backing* b)
    {
        int priorityA = a->currentTexture() ? a->currentTexture()->requestPriority() : CCPriorityCalculator::lowestPriority();
        int priorityB = b->currentTexture() ? b->currentTexture()->requestPriority() : CCPriorityCalculator::lowestPriority();
        if (priorityA == priorityB)
            return a < b;
        return CCPriorityCalculator::priorityIsLower(priorityA, priorityB);
    }

    CCPrioritizedTextureManager(size_t maxMemoryLimitBytes, int maxTextureSize);

    void reduceMemory(size_t limit, TextureAllocator*);

    void link(CCPrioritizedTexture*, CCPrioritizedTexture::Backing*);
    void unlink(CCPrioritizedTexture*, CCPrioritizedTexture::Backing*);

    CCPrioritizedTexture::Backing* createBacking(IntSize, GC3Denum format, TextureAllocator*);
    void destroyBacking(CCPrioritizedTexture::Backing*, TextureAllocator*);

    size_t m_maxMemoryLimitBytes;
    unsigned m_priorityCutoff;
    size_t m_memoryUseBytes;
    size_t m_memoryAboveCutoffBytes;
    size_t m_memoryAvailableBytes;

    typedef HashSet<CCPrioritizedTexture*> TextureSet;
    typedef ListHashSet<CCPrioritizedTexture::Backing*> BackingSet;
    typedef Vector<CCPrioritizedTexture*> TextureVector;
    typedef Vector<CCPrioritizedTexture::Backing*> BackingVector;

    TextureSet m_textures;
    BackingSet m_backings;

    TextureVector m_tempTextureVector;
    BackingVector m_tempBackingVector;
};

} // WebCore

#endif
