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

#include "config.h"

#include "CCResourceProvider.h"

#include "CCProxy.h"
#include "Extensions3DChromium.h"
#include "IntRect.h"
#include "LayerRendererChromium.h" // For the GLC() macro
#include "LayerTextureSubImage.h"
#include <limits.h>
#include <public/WebGraphicsContext3D.h>
#include <wtf/HashSet.h>

using WebKit::WebGraphicsContext3D;

namespace WebCore {

static GC3Denum textureToStorageFormat(GC3Denum textureFormat)
{
    GC3Denum storageFormat = Extensions3D::RGBA8_OES;
    switch (textureFormat) {
    case GraphicsContext3D::RGBA:
        break;
    case Extensions3D::BGRA_EXT:
        storageFormat = Extensions3DChromium::BGRA8_EXT;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return storageFormat;
}

static bool isTextureFormatSupportedForStorage(GC3Denum format)
{
    return (format == GraphicsContext3D::RGBA || format == Extensions3D::BGRA_EXT);
}

PassOwnPtr<CCResourceProvider> CCResourceProvider::create(CCGraphicsContext* context)
{
    OwnPtr<CCResourceProvider> resourceProvider(adoptPtr(new CCResourceProvider(context)));
    if (!resourceProvider->initialize())
        return nullptr;
    return resourceProvider.release();
}

CCResourceProvider::~CCResourceProvider()
{
}

WebGraphicsContext3D* CCResourceProvider::graphicsContext3D()
{
    ASSERT(CCProxy::isImplThread());
    return m_context->context3D();
}

bool CCResourceProvider::inUseByConsumer(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end());
    return !!it->second.lockForReadCount || it->second.exported;
}

CCResourceProvider::ResourceId CCResourceProvider::createResource(int pool, const IntSize& size, GC3Denum format, TextureUsageHint hint)
{
    switch (m_defaultResourceType) {
    case GLTexture:
        return createGLTexture(pool, size, format, hint);
    case Bitmap:
        ASSERT(format == GraphicsContext3D::RGBA);
        return createBitmap(pool, size);
    }

    CRASH();
    return 0;
}

CCResourceProvider::ResourceId CCResourceProvider::createGLTexture(int pool, const IntSize& size, GC3Denum format, TextureUsageHint hint)
{
    ASSERT(CCProxy::isImplThread());
    unsigned textureId = 0;
    WebGraphicsContext3D* context3d = m_context->context3D();
    ASSERT(context3d);
    GLC(context3d, textureId = context3d->createTexture());
    GLC(context3d, context3d->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    GLC(context3d, context3d->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(context3d, context3d->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
    GLC(context3d, context3d->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
    GLC(context3d, context3d->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));

    if (m_useTextureUsageHint && hint == TextureUsageFramebuffer)
        GLC(context3d, context3d->texParameteri(GraphicsContext3D::TEXTURE_2D, Extensions3DChromium::GL_TEXTURE_USAGE_ANGLE, Extensions3DChromium::GL_FRAMEBUFFER_ATTACHMENT_ANGLE));
    if (m_useTextureStorageExt && isTextureFormatSupportedForStorage(format)) {
        GC3Denum storageFormat = textureToStorageFormat(format);
        GLC(context3d, context3d->texStorage2DEXT(GraphicsContext3D::TEXTURE_2D, 1, storageFormat, size.width(), size.height()));
    } else
        GLC(context3d, context3d->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, format, size.width(), size.height(), 0, format, GraphicsContext3D::UNSIGNED_BYTE, 0));
    ResourceId id = m_nextId++;
    Resource resource(textureId, pool, size, format);
    m_resources.add(id, resource);
    return id;
}

CCResourceProvider::ResourceId CCResourceProvider::createBitmap(int pool, const IntSize& size)
{
    ASSERT(CCProxy::isImplThread());

    uint8_t* pixels = new uint8_t[size.width() * size.height() * 4];

    ResourceId id = m_nextId++;
    Resource resource(pixels, pool, size, GraphicsContext3D::RGBA);
    m_resources.add(id, resource);
    return id;
}

CCResourceProvider::ResourceId CCResourceProvider::createResourceFromExternalTexture(unsigned textureId)
{
    ASSERT(CCProxy::isImplThread());
    ASSERT(m_context->context3D());
    ResourceId id = m_nextId++;
    Resource resource(textureId, 0, IntSize(), 0);
    resource.external = true;
    m_resources.add(id, resource);
    return id;
}

void CCResourceProvider::deleteResource(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end() && !it->second.lockedForWrite && !it->second.lockForReadCount);

    if (it->second.glId && !it->second.external) {
        WebGraphicsContext3D* context3d = m_context->context3D();
        ASSERT(context3d);
        GLC(context3d, context3d->deleteTexture(it->second.glId));
    }
    if (it->second.pixels)
        delete it->second.pixels;

    m_resources.remove(it);
}

void CCResourceProvider::deleteOwnedResources(int pool)
{
    ASSERT(CCProxy::isImplThread());
    ResourceIdArray toDelete;
    for (ResourceMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
        if (it->second.pool == pool && !it->second.external)
            toDelete.append(it->first);
    }
    for (ResourceIdArray::iterator it = toDelete.begin(); it != toDelete.end(); ++it)
        deleteResource(*it);
}

CCResourceProvider::ResourceType CCResourceProvider::resourceType(ResourceId id)
{
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end());
    return it->second.type;
}

void CCResourceProvider::upload(ResourceId id, const uint8_t* image, const IntRect& imageRect, const IntRect& sourceRect, const IntSize& destOffset)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end() && !it->second.lockedForWrite && !it->second.lockForReadCount && !it->second.external);

    if (it->second.glId) {
        WebGraphicsContext3D* context3d = m_context->context3D();
        ASSERT(context3d);
        ASSERT(m_texSubImage.get());
        context3d->bindTexture(GraphicsContext3D::TEXTURE_2D, it->second.glId);
        m_texSubImage->upload(image, imageRect, sourceRect, destOffset, it->second.format, context3d);
    }

    if (it->second.pixels) {
        SkBitmap srcFull;
        srcFull.setConfig(SkBitmap::kARGB_8888_Config, imageRect.width(), imageRect.height());
        srcFull.setPixels(const_cast<uint8_t*>(image));
        SkBitmap srcSubset;
        SkIRect skSourceRect = SkIRect::MakeXYWH(sourceRect.x(), sourceRect.y(), sourceRect.width(), sourceRect.height());
        skSourceRect.offset(-imageRect.x(), -imageRect.y());
        srcFull.extractSubset(&srcSubset, skSourceRect);

        ScopedWriteLockSoftware lock(this, id);
        SkCanvas* dest = lock.skCanvas();
        dest->writePixels(srcSubset, destOffset.width(), destOffset.height());
    }
}

void CCResourceProvider::flush()
{
    ASSERT(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (context3d)
        context3d->flush();
}

bool CCResourceProvider::shallowFlushIfSupported()
{
    ASSERT(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !m_useShallowFlush)
        return false;

    context3d->shallowFlushCHROMIUM();
    return true;
}

const CCResourceProvider::Resource* CCResourceProvider::lockForRead(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end() && !it->second.lockedForWrite);
    it->second.lockForReadCount++;
    return &it->second;
}

void CCResourceProvider::unlockForRead(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end() && it->second.lockForReadCount > 0);
    it->second.lockForReadCount--;
}

const CCResourceProvider::Resource* CCResourceProvider::lockForWrite(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end() && !it->second.lockedForWrite && !it->second.lockForReadCount && !it->second.external);
    it->second.lockedForWrite = true;
    return &it->second;
}

void CCResourceProvider::unlockForWrite(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end() && it->second.lockedForWrite && !it->second.external);
    it->second.lockedForWrite = false;
}

CCResourceProvider::ScopedReadLockGL::ScopedReadLockGL(CCResourceProvider* resourceProvider, CCResourceProvider::ResourceId resourceId)
    : m_resourceProvider(resourceProvider)
    , m_resourceId(resourceId)
    , m_textureId(resourceProvider->lockForRead(resourceId)->glId)
{
    ASSERT(m_textureId);
}

CCResourceProvider::ScopedReadLockGL::~ScopedReadLockGL()
{
    m_resourceProvider->unlockForRead(m_resourceId);
}

CCResourceProvider::ScopedWriteLockGL::ScopedWriteLockGL(CCResourceProvider* resourceProvider, CCResourceProvider::ResourceId resourceId)
    : m_resourceProvider(resourceProvider)
    , m_resourceId(resourceId)
    , m_textureId(resourceProvider->lockForWrite(resourceId)->glId)
{
    ASSERT(m_textureId);
}

CCResourceProvider::ScopedWriteLockGL::~ScopedWriteLockGL()
{
    m_resourceProvider->unlockForWrite(m_resourceId);
}

void CCResourceProvider::populateSkBitmapWithResource(SkBitmap* skBitmap, const Resource* resource)
{
    ASSERT(resource->pixels);
    ASSERT(resource->format == GraphicsContext3D::RGBA);
    skBitmap->setConfig(SkBitmap::kARGB_8888_Config, resource->size.width(), resource->size.height());
    skBitmap->setPixels(resource->pixels);
}

CCResourceProvider::ScopedReadLockSoftware::ScopedReadLockSoftware(CCResourceProvider* resourceProvider, CCResourceProvider::ResourceId resourceId)
    : m_resourceProvider(resourceProvider)
    , m_resourceId(resourceId)
{
    CCResourceProvider::populateSkBitmapWithResource(&m_skBitmap, resourceProvider->lockForRead(resourceId));
}

CCResourceProvider::ScopedReadLockSoftware::~ScopedReadLockSoftware()
{
    m_resourceProvider->unlockForRead(m_resourceId);
}

CCResourceProvider::ScopedWriteLockSoftware::ScopedWriteLockSoftware(CCResourceProvider* resourceProvider, CCResourceProvider::ResourceId resourceId)
    : m_resourceProvider(resourceProvider)
    , m_resourceId(resourceId)
{
    CCResourceProvider::populateSkBitmapWithResource(&m_skBitmap, resourceProvider->lockForWrite(resourceId));
    m_skCanvas.setBitmapDevice(m_skBitmap);
}

CCResourceProvider::ScopedWriteLockSoftware::~ScopedWriteLockSoftware()
{
    m_resourceProvider->unlockForWrite(m_resourceId);
}

CCResourceProvider::CCResourceProvider(CCGraphicsContext* context)
    : m_context(context)
    , m_nextId(1)
    , m_nextChild(1)
    , m_defaultResourceType(GLTexture)
    , m_useTextureStorageExt(false)
    , m_useTextureUsageHint(false)
    , m_useShallowFlush(false)
    , m_maxTextureSize(0)
{
}

bool CCResourceProvider::initialize()
{
    ASSERT(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d) {
        m_maxTextureSize = INT_MAX;

        // FIXME: Implement this path for software compositing.
        return false;
    }
    if (!context3d->makeContextCurrent())
        return false;

    WebKit::WebString extensionsWebString = context3d->getString(GraphicsContext3D::EXTENSIONS);
    String extensionsString(extensionsWebString.data(), extensionsWebString.length());
    Vector<String> extensions;
    extensionsString.split(' ', extensions);
    bool useMapSub = false;
    for (size_t i = 0; i < extensions.size(); ++i) {
        if (extensions[i] == "GL_EXT_texture_storage")
            m_useTextureStorageExt = true;
        else if (extensions[i] == "GL_ANGLE_texture_usage")
            m_useTextureUsageHint = true;
        else if (extensions[i] == "GL_CHROMIUM_map_sub")
            useMapSub = true;
        else if (extensions[i] == "GL_CHROMIUM_shallow_flush")
            m_useShallowFlush = true;
    }

    m_texSubImage = adoptPtr(new LayerTextureSubImage(useMapSub));
    GLC(context3d, context3d->getIntegerv(GraphicsContext3D::MAX_TEXTURE_SIZE, &m_maxTextureSize));
    return true;
}

int CCResourceProvider::createChild(int pool)
{
    ASSERT(CCProxy::isImplThread());
    Child childInfo;
    childInfo.pool = pool;
    int child = m_nextChild++;
    m_children.add(child, childInfo);
    return child;
}

void CCResourceProvider::destroyChild(int child)
{
    ASSERT(CCProxy::isImplThread());
    ChildMap::iterator it = m_children.find(child);
    ASSERT(it != m_children.end());
    deleteOwnedResources(it->second.pool);
    m_children.remove(it);
    trimMailboxDeque();
}

const CCResourceProvider::ResourceIdMap& CCResourceProvider::getChildToParentMap(int child) const
{
    ASSERT(CCProxy::isImplThread());
    ChildMap::const_iterator it = m_children.find(child);
    ASSERT(it != m_children.end());
    return it->second.childToParentMap;
}

CCResourceProvider::TransferableResourceList CCResourceProvider::prepareSendToParent(const ResourceIdArray& resources)
{
    ASSERT(CCProxy::isImplThread());
    TransferableResourceList list;
    list.syncPoint = 0;
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !context3d->makeContextCurrent()) {
        // FIXME: Implement this path for software compositing.
        return list;
    }
    for (ResourceIdArray::const_iterator it = resources.begin(); it != resources.end(); ++it) {
        TransferableResource resource;
        if (transferResource(context3d, *it, &resource)) {
            m_resources.find(*it)->second.exported = true;
            list.resources.append(resource);
        }
    }
    if (list.resources.size())
        list.syncPoint = context3d->insertSyncPoint();
    return list;
}

CCResourceProvider::TransferableResourceList CCResourceProvider::prepareSendToChild(int child, const ResourceIdArray& resources)
{
    ASSERT(CCProxy::isImplThread());
    TransferableResourceList list;
    list.syncPoint = 0;
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !context3d->makeContextCurrent()) {
        // FIXME: Implement this path for software compositing.
        return list;
    }
    Child& childInfo = m_children.find(child)->second;
    for (ResourceIdArray::const_iterator it = resources.begin(); it != resources.end(); ++it) {
        TransferableResource resource;
        if (!transferResource(context3d, *it, &resource))
            ASSERT_NOT_REACHED();
        resource.id = childInfo.parentToChildMap.get(*it);
        childInfo.parentToChildMap.remove(*it);
        childInfo.childToParentMap.remove(resource.id);
        list.resources.append(resource);
        deleteResource(*it);
    }
    if (list.resources.size())
        list.syncPoint = context3d->insertSyncPoint();
    return list;
}

void CCResourceProvider::receiveFromChild(int child, const TransferableResourceList& resources)
{
    ASSERT(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !context3d->makeContextCurrent()) {
        // FIXME: Implement this path for software compositing.
        return;
    }
    if (resources.syncPoint) {
        // NOTE: If the parent is a browser and the child a renderer, the parent
        // is not supposed to have its context wait, because that could induce
        // deadlocks and/or security issues. The caller is responsible for
        // waiting asynchronously, and resetting syncPoint before calling this.
        // However if the parent is a renderer (e.g. browser tag), it may be ok
        // (and is simpler) to wait.
        GLC(context3d, context3d->waitSyncPoint(resources.syncPoint));
    }
    Child& childInfo = m_children.find(child)->second;
    for (Vector<TransferableResource>::const_iterator it = resources.resources.begin(); it != resources.resources.end(); ++it) {
        unsigned textureId;
        GLC(context3d, textureId = context3d->createTexture());
        GLC(context3d, context3d->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
        GLC(context3d, context3d->consumeTextureCHROMIUM(GraphicsContext3D::TEXTURE_2D, it->mailbox.name));
        ResourceId id = m_nextId++;
        Resource resource(textureId, childInfo.pool, it->size, it->format);
        m_resources.add(id, resource);
        m_mailboxes.append(it->mailbox);
        childInfo.parentToChildMap.add(id, it->id);
        childInfo.childToParentMap.add(it->id, id);
    }
}

void CCResourceProvider::receiveFromParent(const TransferableResourceList& resources)
{
    ASSERT(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d || !context3d->makeContextCurrent()) {
        // FIXME: Implement this path for software compositing.
        return;
    }
    if (resources.syncPoint)
        GLC(context3d, context3d->waitSyncPoint(resources.syncPoint));
    for (Vector<TransferableResource>::const_iterator it = resources.resources.begin(); it != resources.resources.end(); ++it) {
        Resource& resource = m_resources.find(it->id)->second;
        ASSERT(resource.exported);
        resource.exported = false;
        GLC(context3d, context3d->bindTexture(GraphicsContext3D::TEXTURE_2D, resource.glId));
        GLC(context3d, context3d->consumeTextureCHROMIUM(GraphicsContext3D::TEXTURE_2D, it->mailbox.name));
        m_mailboxes.append(it->mailbox);
    }
}

bool CCResourceProvider::transferResource(WebGraphicsContext3D* context, ResourceId id, TransferableResource* resource)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::const_iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end() && !it->second.lockedForWrite && !it->second.lockForReadCount && !it->second.external);
    if (it->second.exported)
        return false;
    resource->id = id;
    resource->format = it->second.format;
    resource->size = it->second.size;
    if (!m_mailboxes.isEmpty())
        resource->mailbox = m_mailboxes.takeFirst();
    else
        GLC(context, context->genMailboxCHROMIUM(resource->mailbox.name));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, it->second.glId));
    GLC(context, context->produceTextureCHROMIUM(GraphicsContext3D::TEXTURE_2D, resource->mailbox.name));
    return true;
}

void CCResourceProvider::trimMailboxDeque()
{
    // Trim the mailbox deque to the maximum number of resources we may need to
    // send.
    // If we have a parent, any non-external resource not already transfered is
    // eligible to be sent to the parent. Otherwise, all resources belonging to
    // a child might need to be sent back to the child.
    size_t maxMailboxCount = 0;
    if (m_context->capabilities().hasParentCompositor) {
        for (ResourceMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
            if (!it->second.exported && !it->second.external)
                ++maxMailboxCount;
        }
    } else {
        HashSet<int> childPoolSet;
        for (ChildMap::iterator it = m_children.begin(); it != m_children.end(); ++it)
            childPoolSet.add(it->second.pool);
        for (ResourceMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
            if (childPoolSet.contains(it->second.pool))
                ++maxMailboxCount;
        }
    }
    while (m_mailboxes.size() > maxMailboxCount)
        m_mailboxes.removeFirst();
}

}
