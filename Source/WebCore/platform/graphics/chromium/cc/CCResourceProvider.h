/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef CCResourceProvider_h
#define CCResourceProvider_h

#include "GraphicsContext3D.h"
#include "IntSize.h"
#include "cc/CCGraphicsContext.h"
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace WebCore {

class CCScopedLockResourceForRead;
class CCScopedLockResourceForWrite;
class IntRect;
class LayerTextureSubImage;

// Thread-safety notes: this class is not thread-safe and can only be called
// from the thread it was created on (in practice, the compositor thread).
class CCResourceProvider {
    WTF_MAKE_NONCOPYABLE(CCResourceProvider);
public:
    typedef unsigned ResourceId;
    enum TextureUsageHint { TextureUsageAny, TextureUsageFramebuffer };

    static PassOwnPtr<CCResourceProvider> create(CCGraphicsContext*);

    virtual ~CCResourceProvider();

    WebKit::WebGraphicsContext3D* graphicsContext3D();
    int maxTextureSize() const { return m_maxTextureSize; }
    unsigned numResources() const { return m_resources.size(); }

    // Checks whether a resource is in use by a consumer.
    bool inUseByConsumer(ResourceId);


    // Producer interface.

    // Creates a resource of the given size/format, into the given pool.
    ResourceId createResource(int pool, const IntSize&, GC3Denum format, TextureUsageHint);

    // Wraps an external texture into a resource.
    ResourceId createResourceFromExternalTexture(unsigned textureId);
    void deleteResource(ResourceId);

    // Deletes all resources owned by a given pool.
    void deleteOwnedResources(int pool);

    // Upload data from image, copying sourceRect (in image) into destRect (in the resource).
    void upload(ResourceId, const uint8_t* image, const IntRect& imageRect, const IntRect& sourceRect, const IntRect& destRect);

    // Flush all context operations, kicking uploads and ensuring ordering with
    // respect to other contexts.
    void flush();

    // Only flush the command buffer if supported.
    // Returns true if the shallow flush occurred, false otherwise.
    bool shallowFlushIfSupported();

private:
    friend class CCScopedLockResourceForRead;
    friend class CCScopedLockResourceForWrite;

    struct Resource {
        unsigned glId;
        int pool;
        int lockForReadCount;
        bool lockedForWrite;
        bool external;
        IntSize size;
        GC3Denum format;
    };
    typedef HashMap<ResourceId, Resource> ResourceMap;

    explicit CCResourceProvider(CCGraphicsContext*);
    bool initialize();

    // Gets a GL texture id representing the resource, that can be rendered into.
    unsigned lockForWrite(ResourceId);
    void unlockForWrite(ResourceId);

    // Gets a GL texture id representing the resource, that can be rendered with.
    unsigned lockForRead(ResourceId);
    void unlockForRead(ResourceId);

    CCGraphicsContext* m_context;
    ResourceId m_nextId;
    ResourceMap m_resources;

    bool m_useTextureStorageExt;
    bool m_useTextureUsageHint;
    bool m_useShallowFlush;
    OwnPtr<LayerTextureSubImage> m_texSubImage;
    int m_maxTextureSize;
};

class CCScopedLockResourceForRead {
    WTF_MAKE_NONCOPYABLE(CCScopedLockResourceForRead);
public:
    CCScopedLockResourceForRead(CCResourceProvider* resourceProvider, CCResourceProvider::ResourceId resourceId)
        : m_resourceProvider(resourceProvider)
        , m_resourceId(resourceId)
        , m_textureId(resourceProvider->lockForRead(resourceId)) { }

    ~CCScopedLockResourceForRead()
    {
        m_resourceProvider->unlockForRead(m_resourceId);
    }

    unsigned textureId() const { return m_textureId; }

private:
    CCResourceProvider* m_resourceProvider;
    CCResourceProvider::ResourceId m_resourceId;
    unsigned m_textureId;
};

class CCScopedLockResourceForWrite {
    WTF_MAKE_NONCOPYABLE(CCScopedLockResourceForWrite);
public:
    CCScopedLockResourceForWrite(CCResourceProvider* resourceProvider, CCResourceProvider::ResourceId resourceId)
        : m_resourceProvider(resourceProvider)
        , m_resourceId(resourceId)
        , m_textureId(resourceProvider->lockForWrite(resourceId)) { }

    ~CCScopedLockResourceForWrite()
    {
        m_resourceProvider->unlockForWrite(m_resourceId);
    }

    unsigned textureId() const { return m_textureId; }

private:
    CCResourceProvider* m_resourceProvider;
    CCResourceProvider::ResourceId m_resourceId;
    unsigned m_textureId;
};

}

#endif
