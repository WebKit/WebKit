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

#include "CCGraphicsContext.h"
#include "GraphicsContext3D.h"
#include "IntSize.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace WebCore {

class IntRect;
class LayerTextureSubImage;

// Thread-safety notes: this class is not thread-safe and can only be called
// from the thread it was created on (in practice, the compositor thread).
class CCResourceProvider {
    WTF_MAKE_NONCOPYABLE(CCResourceProvider);
public:
    typedef unsigned ResourceId;
    typedef Vector<ResourceId> ResourceIdArray;
    typedef HashMap<ResourceId, ResourceId> ResourceIdMap;
    enum TextureUsageHint { TextureUsageAny, TextureUsageFramebuffer };
    enum ResourceType {
        GLTexture = 1,
        Bitmap,
    };
    struct Mailbox {
        GC3Dbyte name[64];
    };
    struct TransferableResource {
        unsigned id;
        GC3Denum format;
        IntSize size;
        Mailbox mailbox;
    };
    typedef Vector<TransferableResource> TransferableResourceArray;
    struct TransferableResourceList {
        TransferableResourceArray resources;
        unsigned syncPoint;
    };

    static PassOwnPtr<CCResourceProvider> create(CCGraphicsContext*);

    virtual ~CCResourceProvider();

    WebKit::WebGraphicsContext3D* graphicsContext3D();
    int maxTextureSize() const { return m_maxTextureSize; }
    unsigned numResources() const { return m_resources.size(); }

    // Checks whether a resource is in use by a consumer.
    bool inUseByConsumer(ResourceId);


    // Producer interface.

    void setDefaultResourceType(ResourceType type) { m_defaultResourceType = type; }
    ResourceType defaultResourceType() const { return m_defaultResourceType; }
    ResourceType resourceType(ResourceId);

    // Creates a resource of the default resource type.
    ResourceId createResource(int pool, const IntSize&, GC3Denum format, TextureUsageHint);

    // You can also explicitly create a specific resource type.
    ResourceId createGLTexture(int pool, const IntSize&, GC3Denum format, TextureUsageHint);
    ResourceId createBitmap(int pool, const IntSize&);
    // Wraps an external texture into a GL resource.
    ResourceId createResourceFromExternalTexture(unsigned textureId);

    void deleteResource(ResourceId);

    // Deletes all resources owned by a given pool.
    void deleteOwnedResources(int pool);

    // Upload data from image, copying sourceRect (in image) into destRect (in the resource).
    void upload(ResourceId, const uint8_t* image, const IntRect& imageRect, const IntRect& sourceRect, const IntSize& destOffset);

    // Flush all context operations, kicking uploads and ensuring ordering with
    // respect to other contexts.
    void flush();

    // Only flush the command buffer if supported.
    // Returns true if the shallow flush occurred, false otherwise.
    bool shallowFlushIfSupported();

    // Creates accounting for a child, and associate it with a pool. Resources
    // transfered from that child will go to that pool. Returns a child ID.
    int createChild(int pool);

    // Destroys accounting for the child, deleting all resources from that pool.
    void destroyChild(int child);

    // Gets the child->parent resource ID map.
    const ResourceIdMap& getChildToParentMap(int child) const;

    // Prepares resources to be transfered to the parent, moving them to
    // mailboxes and serializing meta-data into TransferableResources.
    // Resources are not removed from the CCResourceProvider, but are markes as
    // "in use".
    TransferableResourceList prepareSendToParent(const ResourceIdArray&);

    // Prepares resources to be transfered back to the child, moving them to
    // mailboxes and serializing meta-data into TransferableResources.
    // Resources are removed from the CCResourceProvider. Note: the resource IDs
    // passed are in the parent namespace and will be translated to the child
    // namespace when returned.
    TransferableResourceList prepareSendToChild(int child, const ResourceIdArray&);

    // Receives resources from a child, moving them from mailboxes. Resource IDs
    // passed are in the child namespace, and will be translated to the parent
    // namespace, added to the child->parent map.
    // NOTE: if the syncPoint filed in TransferableResourceList is set, this
    // will wait on it.
    void receiveFromChild(int child, const TransferableResourceList&);

    // Receives resources from the parent, moving them from mailboxes. Resource IDs
    // passed are in the child namespace.
    // NOTE: if the syncPoint filed in TransferableResourceList is set, this
    // will wait on it.
    void receiveFromParent(const TransferableResourceList&);

    // Only for testing
    size_t mailboxCount() const { return m_mailboxes.size(); }

    // The following lock classes are part of the CCResourceProvider API and are
    // needed to read and write the resource contents. The user must ensure
    // that they only use GL locks on GL resources, etc, and this is enforced
    // by assertions.
    class ScopedReadLockGL {
        WTF_MAKE_NONCOPYABLE(ScopedReadLockGL);
    public:
        ScopedReadLockGL(CCResourceProvider*, CCResourceProvider::ResourceId);
        ~ScopedReadLockGL();

        unsigned textureId() const { return m_textureId; }

    private:
        CCResourceProvider* m_resourceProvider;
        CCResourceProvider::ResourceId m_resourceId;
        unsigned m_textureId;
    };

    class ScopedWriteLockGL {
        WTF_MAKE_NONCOPYABLE(ScopedWriteLockGL);
    public:
        ScopedWriteLockGL(CCResourceProvider*, CCResourceProvider::ResourceId);
        ~ScopedWriteLockGL();

        unsigned textureId() const { return m_textureId; }

    private:
        CCResourceProvider* m_resourceProvider;
        CCResourceProvider::ResourceId m_resourceId;
        unsigned m_textureId;
    };

    class ScopedReadLockSoftware {
        WTF_MAKE_NONCOPYABLE(ScopedReadLockSoftware);
    public:
        ScopedReadLockSoftware(CCResourceProvider*, CCResourceProvider::ResourceId);
        ~ScopedReadLockSoftware();

        const SkBitmap* skBitmap() const { return &m_skBitmap; }

    private:
        CCResourceProvider* m_resourceProvider;
        CCResourceProvider::ResourceId m_resourceId;
        SkBitmap m_skBitmap;
    };

    class ScopedWriteLockSoftware {
        WTF_MAKE_NONCOPYABLE(ScopedWriteLockSoftware);
    public:
        ScopedWriteLockSoftware(CCResourceProvider*, CCResourceProvider::ResourceId);
        ~ScopedWriteLockSoftware();

        SkCanvas* skCanvas() { return &m_skCanvas; }

    private:
        CCResourceProvider* m_resourceProvider;
        CCResourceProvider::ResourceId m_resourceId;
        SkBitmap m_skBitmap;
        SkCanvas m_skCanvas;
    };

private:
    struct Resource {
        Resource()
            : glId(0)
            , pixels(0)
            , pool(0)
            , lockForReadCount(0)
            , lockedForWrite(false)
            , external(false)
            , exported(false)
            , size()
            , format(0)
            , type(static_cast<ResourceType>(0))
        { }
        Resource(unsigned textureId, int pool, const IntSize& size, GC3Denum format)
            : glId(textureId)
            , pixels(0)
            , pool(pool)
            , lockForReadCount(0)
            , lockedForWrite(false)
            , external(false)
            , exported(false)
            , size(size)
            , format(format)
            , type(GLTexture)
        { }
        Resource(uint8_t* pixels, int pool, const IntSize& size, GC3Denum format)
            : glId(0)
            , pixels(pixels)
            , pool(pool)
            , lockForReadCount(0)
            , lockedForWrite(false)
            , external(false)
            , exported(false)
            , size(size)
            , format(format)
            , type(Bitmap)
        { }
        unsigned glId;
        uint8_t* pixels;
        int pool;
        int lockForReadCount;
        bool lockedForWrite;
        bool external;
        bool exported;
        IntSize size;
        GC3Denum format;
        ResourceType type;
    };
    typedef HashMap<ResourceId, Resource> ResourceMap;
    struct Child {
        int pool;
        ResourceIdMap childToParentMap;
        ResourceIdMap parentToChildMap;
    };
    typedef HashMap<int, Child> ChildMap;

    explicit CCResourceProvider(CCGraphicsContext*);
    bool initialize();

    const Resource* lockForRead(ResourceId);
    void unlockForRead(ResourceId);
    const Resource* lockForWrite(ResourceId);
    void unlockForWrite(ResourceId);
    static void populateSkBitmapWithResource(SkBitmap*, const Resource*);

    bool transferResource(WebKit::WebGraphicsContext3D*, ResourceId, TransferableResource*);
    void trimMailboxDeque();

    CCGraphicsContext* m_context;
    ResourceId m_nextId;
    ResourceMap m_resources;
    int m_nextChild;
    ChildMap m_children;

    Deque<Mailbox> m_mailboxes;

    ResourceType m_defaultResourceType;
    bool m_useTextureStorageExt;
    bool m_useTextureUsageHint;
    bool m_useShallowFlush;
    OwnPtr<LayerTextureSubImage> m_texSubImage;
    int m_maxTextureSize;
};

}

#endif
