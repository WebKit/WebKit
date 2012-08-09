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

#include "cc/CCResourceProvider.h"

#include "Extensions3DChromium.h"
#include "IntRect.h"
#include "LayerRendererChromium.h" // For the GLC() macro
#include "LayerTextureSubImage.h"
#include "cc/CCProxy.h"
#include <public/WebGraphicsContext3D.h>

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
    return !!it->second.lockForReadCount;
}

CCResourceProvider::ResourceId CCResourceProvider::createResource(int pool, const IntSize& size, GC3Denum format, TextureUsageHint hint)
{
    ASSERT(CCProxy::isImplThread());
    unsigned textureId = 0;
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d) {
        // FIXME: Implement this path for software compositing.
        return 0;
    }
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
    Resource resource = {textureId, pool, 0, false, false, size, format};
    m_resources.add(id, resource);
    return id;
}

CCResourceProvider::ResourceId CCResourceProvider::createResourceFromExternalTexture(unsigned textureId)
{
    ASSERT(CCProxy::isImplThread());
    ResourceId id = m_nextId++;
    Resource resource = {textureId, 0, 0, false, true, IntSize(), 0};
    m_resources.add(id, resource);
    return id;
}

void CCResourceProvider::deleteResource(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d) {
        // FIXME: Implement this path for software compositing.
        return;
    }
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end() && !it->second.lockedForWrite && !it->second.lockForReadCount);
    if (!it->second.external)
        GLC(context3d, context3d->deleteTexture(it->second.glId));
    m_resources.remove(it);
}

void CCResourceProvider::deleteOwnedResources(int pool)
{
    ASSERT(CCProxy::isImplThread());
    Vector<ResourceId> toDelete;
    for (ResourceMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
        if (it->second.pool == pool && !it->second.external)
            toDelete.append(it->first);
    }
    for (Vector<ResourceId>::iterator it = toDelete.begin(); it != toDelete.end(); ++it)
        deleteResource(*it);
}

void CCResourceProvider::upload(ResourceId id, const uint8_t* image, const IntRect& imageRect, const IntRect& sourceRect, const IntRect& destRect)
{
    ASSERT(CCProxy::isImplThread());
    ASSERT(m_texSubImage.get());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d) {
        // FIXME: Implement this path for software compositing.
        return;
    }
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end() && !it->second.lockedForWrite && !it->second.lockForReadCount && !it->second.external);

    context3d->bindTexture(GraphicsContext3D::TEXTURE_2D, it->second.glId);
    m_texSubImage->upload(image, imageRect, sourceRect, destRect, it->second.format, context3d);
}

unsigned CCResourceProvider::lockForWrite(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end() && !it->second.lockedForWrite && !it->second.lockForReadCount && !it->second.external);
    it->second.lockedForWrite = true;
    return it->second.glId;
}

void CCResourceProvider::unlockForWrite(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end() && it->second.lockedForWrite && !it->second.external);
    it->second.lockedForWrite = false;
}

void CCResourceProvider::flush()
{
    ASSERT(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d) {
        // FIXME: Implement this path for software compositing.
        return;
    }
    context3d->flush();
}

bool CCResourceProvider::shallowFlushIfSupported()
{
    ASSERT(CCProxy::isImplThread());
    WebGraphicsContext3D* context3d = m_context->context3D();
    if (!context3d) {
        // FIXME: Implement this path for software compositing.
        return false;
    }

    if (m_useShallowFlush)
        context3d->shallowFlushCHROMIUM();
    return m_useShallowFlush;
}

unsigned CCResourceProvider::lockForRead(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end() && !it->second.lockedForWrite);
    ++(it->second.lockForReadCount);
    return it->second.glId;
}

void CCResourceProvider::unlockForRead(ResourceId id)
{
    ASSERT(CCProxy::isImplThread());
    ResourceMap::iterator it = m_resources.find(id);
    ASSERT(it != m_resources.end() && it->second.lockForReadCount > 0);
    --(it->second.lockForReadCount);
}

CCResourceProvider::CCResourceProvider(CCGraphicsContext* context)
    : m_context(context)
    , m_nextId(1)
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
    if (!context3d || !context3d->makeContextCurrent()) {
        // FIXME: Implement this path for software compositing.
        return false;
    }
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

}
