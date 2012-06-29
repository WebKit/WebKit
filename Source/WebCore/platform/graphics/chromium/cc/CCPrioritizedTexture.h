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

#ifndef CCPrioritizedTexture_h
#define CCPrioritizedTexture_h

#include "CCPriorityCalculator.h"
#include "GraphicsContext3D.h"
#include "IntRect.h"
#include "IntSize.h"
#include "TextureManager.h"

namespace WebCore {

class CCPrioritizedTextureManager;
class CCPriorityCalculator;
class CCGraphicsContext;

class CCPrioritizedTexture {
    WTF_MAKE_NONCOPYABLE(CCPrioritizedTexture);
public:
    static PassOwnPtr<CCPrioritizedTexture> create(CCPrioritizedTextureManager* manager, IntSize size, GC3Denum format)
    {
        return adoptPtr(new CCPrioritizedTexture(manager, size, format));
    }
    static PassOwnPtr<CCPrioritizedTexture> create(CCPrioritizedTextureManager* manager)
    {
        return adoptPtr(new CCPrioritizedTexture(manager, IntSize(), 0));
    }
    ~CCPrioritizedTexture();

    // Texture properties. Changing these causes the backing texture to be lost.
    // Setting these to the same value is a no-op.
    void setTextureManager(CCPrioritizedTextureManager*);
    CCPrioritizedTextureManager* textureManager() { return m_manager; }
    void setDimensions(IntSize, GC3Denum format);
    GC3Denum format() const { return m_format; }
    IntSize size() const { return m_size; }
    size_t memorySizeBytes() const { return m_memorySizeBytes; }

    // Set priority for the requested texture. 
    void setRequestPriority(int priority) { m_priority = priority; }
    int requestPriority() const { return m_priority; }

    // After CCPrioritizedTexture::prioritizeTextures() is called, this returns
    // if the the request succeeded and this texture can be acquired for use.
    bool canAcquireBackingTexture() const { return m_isAbovePriorityCutoff; }

    // This returns whether we still have a backing texture. This can continue
    // to be true even after canAquireBackingTexture() becomes false. In this
    // case the texture can be used but shouldn't be updated since it will get
    // taken away "soon".
    bool haveBackingTexture() const { return !!currentBacking(); }

    // If canAquireBackingTexture() is true acquireBackingTexture() will acquire
    // a backing texture for use. Call this whenever the texture is actually needed.
    void acquireBackingTexture(TextureAllocator*);

    // FIXME: Request late is really a hack for when we are totally out of memory
    //        (all textures are visible) but we can still squeeze into the limit
    //        by not painting occluded textures. In this case the manager
    //        refuses all visible textures and requestLate() will enable
    //        canAquireBackingTexture() on a call-order basis. We might want to
    //        just remove this in the future (carefully) and just make sure we don't
    //        regress OOMs situations.
    bool requestLate();

    // These functions will aquire the texture if possible. If neither haveBackingTexture()
    // nor canAquireBackingTexture() is true, an ID of zero will be used/returned.
    void bindTexture(CCGraphicsContext*, TextureAllocator*);
    void framebufferTexture2D(CCGraphicsContext*, TextureAllocator*);
    unsigned textureId();

private:
    friend class CCPrioritizedTextureManager;

    class Backing {
        WTF_MAKE_NONCOPYABLE(Backing);
    public:
        IntSize size() const { return m_size; }
        GC3Denum format() const { return m_format; }
        size_t memorySizeBytes() const { return m_memorySizeBytes; }
        unsigned textureId() const { return m_textureId; }
        CCPrioritizedTexture* currentTexture() const { return m_currentTexture; }
        void setCurrentTexture(CCPrioritizedTexture* current) { m_currentTexture = current; }

    private:
        friend class CCPrioritizedTextureManager;

        Backing(IntSize size, GC3Denum format, unsigned textureId)
            : m_size(size)
            , m_format(format)
            , m_memorySizeBytes(TextureManager::memoryUseBytes(size, format))
            , m_textureId(textureId)
            , m_priority(CCPriorityCalculator::lowestPriority())
            , m_currentTexture(0) { }
        ~Backing() { ASSERT(!m_currentTexture); }

        IntSize m_size;
        GC3Denum m_format;
        size_t m_memorySizeBytes;
        unsigned m_textureId;
        int m_priority;
        CCPrioritizedTexture* m_currentTexture;
    };

    CCPrioritizedTexture(CCPrioritizedTextureManager*, IntSize, GC3Denum format);

    bool isAbovePriorityCutoff() { return m_isAbovePriorityCutoff; }
    void setAbovePriorityCutoff(bool isAbovePriorityCutoff) { m_isAbovePriorityCutoff = isAbovePriorityCutoff; }
    void setManagerInternal(CCPrioritizedTextureManager* manager) { m_manager = manager; }

    Backing* currentBacking() const { return m_currentBacking; }
    void setCurrentBacking(Backing*);

    IntSize m_size;
    GC3Denum m_format;
    size_t m_memorySizeBytes;

    size_t m_priority;
    bool m_isAbovePriorityCutoff;

    Backing* m_currentBacking;
    CCPrioritizedTextureManager* m_manager;
};

} // WebCore

#endif
