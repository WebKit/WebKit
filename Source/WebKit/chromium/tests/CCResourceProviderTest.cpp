/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "CCGraphicsContext.h"
#include "CCSingleThreadProxy.h" // For DebugScopedSetImplThread
#include "CompositorFakeWebGraphicsContext3D.h"
#include "Extensions3DChromium.h"
#include "FakeWebCompositorOutputSurface.h"
#include <gtest/gtest.h>
#include <public/WebGraphicsContext3D.h>
#include <wtf/HashMap.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/OwnPtr.h>

using namespace WebCore;
using namespace WebKit;

namespace {

size_t textureSize(const IntSize& size, WGC3Denum format)
{
    unsigned int componentsPerPixel = 4;
    unsigned int bytesPerComponent = 1;
    GraphicsContext3D::computeFormatAndTypeParameters(format, GraphicsContext3D::UNSIGNED_BYTE, &componentsPerPixel, &bytesPerComponent);
    return size.width() * size.height() * componentsPerPixel * bytesPerComponent;
}

struct Texture {
    Texture(const IntSize& size, WGC3Denum format)
        : size(size)
        , format(format)
        , data(adoptArrayPtr(new uint8_t[textureSize(size, format)]))
    {
    }

    IntSize size;
    WGC3Denum format;
    OwnArrayPtr<uint8_t> data;
};

// Shared data between multiple ResourceProviderContext. This contains mailbox
// contents as well as information about sync points.
class ContextSharedData {
public:
    static PassOwnPtr<ContextSharedData> create() { return adoptPtr(new ContextSharedData()); }

    unsigned insertSyncPoint() { return m_nextSyncPoint++; }

    void genMailbox(WGC3Dbyte* mailbox)
    {
        memset(mailbox, 0, sizeof(WGC3Dbyte[64]));
        memcpy(mailbox, &m_nextMailBox, sizeof(m_nextMailBox));
        ++m_nextMailBox;
    }

    void produceTexture(const WGC3Dbyte* mailboxName, unsigned syncPoint, PassOwnPtr<Texture> texture)
    {
        unsigned mailbox = 0;
        memcpy(&mailbox, mailboxName, sizeof(mailbox));
        ASSERT(mailbox && mailbox < m_nextMailBox);
        m_textures.set(mailbox, texture);
        ASSERT(m_syncPointForMailbox.get(mailbox) < syncPoint);
        m_syncPointForMailbox.set(mailbox, syncPoint);
    }

    PassOwnPtr<Texture> consumeTexture(const WGC3Dbyte* mailboxName, unsigned syncPoint)
    {
        unsigned mailbox = 0;
        memcpy(&mailbox, mailboxName, sizeof(mailbox));
        ASSERT(mailbox && mailbox < m_nextMailBox);

        // If the latest sync point the context has waited on is before the sync
        // point for when the mailbox was set, pretend we never saw that
        // produceTexture.
        if (m_syncPointForMailbox.get(mailbox) < syncPoint)
            return nullptr;
        return m_textures.take(mailbox);
    }

private:
    ContextSharedData()
        : m_nextSyncPoint(1)
        , m_nextMailBox(1)
    { }

    unsigned m_nextSyncPoint;
    unsigned m_nextMailBox;
    typedef HashMap<unsigned, OwnPtr<Texture> > TextureMap;
    TextureMap m_textures;
    HashMap<unsigned, unsigned> m_syncPointForMailbox;
};

class ResourceProviderContext : public CompositorFakeWebGraphicsContext3D {
public:
    static PassOwnPtr<ResourceProviderContext> create(ContextSharedData* sharedData) { return adoptPtr(new ResourceProviderContext(Attributes(), sharedData)); }

    virtual unsigned insertSyncPoint()
    {
        unsigned syncPoint = m_sharedData->insertSyncPoint();
        // Commit the produceTextureCHROMIUM calls at this point, so that
        // they're associated with the sync point.
        for (PendingProduceTextureList::iterator it = m_pendingProduceTextures.begin(); it != m_pendingProduceTextures.end(); ++it)
            m_sharedData->produceTexture((*it)->mailbox, syncPoint, (*it)->texture.release());
        m_pendingProduceTextures.clear();
        return syncPoint;
    }

    virtual void waitSyncPoint(unsigned syncPoint)
    {
        m_lastWaitedSyncPoint = std::max(syncPoint, m_lastWaitedSyncPoint);
    }

    virtual void bindTexture(WGC3Denum target, WebGLId texture)
    {
      ASSERT(target == GraphicsContext3D::TEXTURE_2D);
      ASSERT(!texture || m_textures.find(texture) != m_textures.end());
      m_currentTexture = texture;
    }

    virtual WebGLId createTexture()
    {
        WebGLId id = CompositorFakeWebGraphicsContext3D::createTexture();
        m_textures.add(id, nullptr);
        return id;
    }

    virtual void deleteTexture(WebGLId id)
    {
        TextureMap::iterator it = m_textures.find(id);
        ASSERT(it != m_textures.end());
        m_textures.remove(it);
        if (m_currentTexture == id)
            m_currentTexture = 0;
    }

    virtual void texStorage2DEXT(WGC3Denum target, WGC3Dint levels, WGC3Duint internalformat,
                                 WGC3Dint width, WGC3Dint height)
    {
        ASSERT(m_currentTexture);
        ASSERT(target == GraphicsContext3D::TEXTURE_2D);
        ASSERT(levels == 1);
        WGC3Denum format = GraphicsContext3D::RGBA;
        switch (internalformat) {
        case Extensions3D::RGBA8_OES:
            break;
        case Extensions3DChromium::BGRA8_EXT:
            format = Extensions3D::BGRA_EXT;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        allocateTexture(IntSize(width, height), format);
    }

    virtual void texImage2D(WGC3Denum target, WGC3Dint level, WGC3Denum internalformat, WGC3Dsizei width, WGC3Dsizei height, WGC3Dint border, WGC3Denum format, WGC3Denum type, const void* pixels)
    {
        ASSERT(m_currentTexture);
        ASSERT(target == GraphicsContext3D::TEXTURE_2D);
        ASSERT(!level);
        ASSERT(internalformat == format);
        ASSERT(!border);
        ASSERT(type == GraphicsContext3D::UNSIGNED_BYTE);
        allocateTexture(IntSize(width, height), format);
        if (pixels)
            setPixels(0, 0, width, height, pixels);
    }

    virtual void texSubImage2D(WGC3Denum target, WGC3Dint level, WGC3Dint xoffset, WGC3Dint yoffset, WGC3Dsizei width, WGC3Dsizei height, WGC3Denum format, WGC3Denum type, const void* pixels)
    {
        ASSERT(m_currentTexture);
        ASSERT(target == GraphicsContext3D::TEXTURE_2D);
        ASSERT(!level);
        ASSERT(m_textures.get(m_currentTexture));
        ASSERT(m_textures.get(m_currentTexture)->format == format);
        ASSERT(type == GraphicsContext3D::UNSIGNED_BYTE);
        ASSERT(pixels);
        setPixels(xoffset, yoffset, width, height, pixels);
    }

    virtual void genMailboxCHROMIUM(WGC3Dbyte* mailbox) { return m_sharedData->genMailbox(mailbox); }
    virtual void produceTextureCHROMIUM(WGC3Denum target, const WGC3Dbyte* mailbox)
    {
        ASSERT(m_currentTexture);
        ASSERT(target == GraphicsContext3D::TEXTURE_2D);

        // Delay movind the texture into the mailbox until the next
        // insertSyncPoint, so that it is not visible to other contexts that
        // haven't waited on that sync point.
        OwnPtr<PendingProduceTexture> pending(adoptPtr(new PendingProduceTexture));
        memcpy(pending->mailbox, mailbox, sizeof(pending->mailbox));
        pending->texture = m_textures.take(m_currentTexture);
        m_textures.set(m_currentTexture, nullptr);
        m_pendingProduceTextures.append(pending.release());
    }

    virtual void consumeTextureCHROMIUM(WGC3Denum target, const WGC3Dbyte* mailbox)
    {
        ASSERT(m_currentTexture);
        ASSERT(target == GraphicsContext3D::TEXTURE_2D);
        m_textures.set(m_currentTexture, m_sharedData->consumeTexture(mailbox, m_lastWaitedSyncPoint));
    }

    void getPixels(const IntSize& size, WGC3Denum format, uint8_t* pixels)
    {
        ASSERT(m_currentTexture);
        Texture* texture = m_textures.get(m_currentTexture);
        ASSERT(texture);
        ASSERT(texture->size == size);
        ASSERT(texture->format == format);
        memcpy(pixels, texture->data.get(), textureSize(size, format));
    }

    int textureCount()
    {
        return m_textures.size();
    }

protected:
    ResourceProviderContext(const Attributes& attrs, ContextSharedData* sharedData)
        : CompositorFakeWebGraphicsContext3D(attrs)
        , m_sharedData(sharedData)
        , m_currentTexture(0)
        , m_lastWaitedSyncPoint(0)
    { }

private:
    void allocateTexture(const IntSize& size, WGC3Denum format)
    {
        ASSERT(m_currentTexture);
        m_textures.set(m_currentTexture, adoptPtr(new Texture(size, format)));
    }

    void setPixels(int xoffset, int yoffset, int width, int height, const void* pixels)
    {
        ASSERT(m_currentTexture);
        Texture* texture = m_textures.get(m_currentTexture);
        ASSERT(texture);
        ASSERT(xoffset >= 0 && xoffset+width <= texture->size.width());
        ASSERT(yoffset >= 0 && yoffset+height <= texture->size.height());
        ASSERT(pixels);
        size_t inPitch = textureSize(IntSize(width, 1), texture->format);
        size_t outPitch = textureSize(IntSize(texture->size.width(), 1), texture->format);
        uint8_t* dest = texture->data.get() + yoffset * outPitch + textureSize(IntSize(xoffset, 1), texture->format);
        const uint8_t* src = static_cast<const uint8_t*>(pixels);
        for (int i = 0; i < height; ++i) {
            memcpy(dest, src, inPitch);
            dest += outPitch;
            src += inPitch;
        }
    }

    typedef HashMap<WebGLId, OwnPtr<Texture> > TextureMap;
    struct PendingProduceTexture {
        WGC3Dbyte mailbox[64];
        OwnPtr<Texture> texture;
    };
    typedef Deque<OwnPtr<PendingProduceTexture> > PendingProduceTextureList;
    ContextSharedData* m_sharedData;
    WebGLId m_currentTexture;
    TextureMap m_textures;
    unsigned m_lastWaitedSyncPoint;
    PendingProduceTextureList m_pendingProduceTextures;
};

class CCResourceProviderTest : public testing::TestWithParam<CCResourceProvider::ResourceType> {
public:
    CCResourceProviderTest()
        : m_sharedData(ContextSharedData::create())
        , m_context(FakeWebCompositorOutputSurface::create(ResourceProviderContext::create(m_sharedData.get())))
        , m_resourceProvider(CCResourceProvider::create(m_context.get()))
    {
        m_resourceProvider->setDefaultResourceType(GetParam());
    }

    ResourceProviderContext* context() { return static_cast<ResourceProviderContext*>(m_context->context3D()); }

    void getResourcePixels(CCResourceProvider::ResourceId id, const IntSize& size, WGC3Denum format, uint8_t* pixels)
    {
        if (GetParam() == CCResourceProvider::GLTexture) {
            CCResourceProvider::ScopedReadLockGL lockGL(m_resourceProvider.get(), id);
            ASSERT_NE(0U, lockGL.textureId());
            context()->bindTexture(GraphicsContext3D::TEXTURE_2D, lockGL.textureId());
            context()->getPixels(size, format, pixels);
        } else if (GetParam() == CCResourceProvider::Bitmap) {
            CCResourceProvider::ScopedReadLockSoftware lockSoftware(m_resourceProvider.get(), id);
            memcpy(pixels, lockSoftware.skBitmap()->getPixels(), lockSoftware.skBitmap()->getSize());
        }
    }

    void expectNumResources(int count)
    {
        EXPECT_EQ(count, static_cast<int>(m_resourceProvider->numResources()));
        if (GetParam() == CCResourceProvider::GLTexture)
            EXPECT_EQ(count, context()->textureCount());
    }

protected:
    DebugScopedSetImplThread implThread;
    OwnPtr<ContextSharedData> m_sharedData;
    OwnPtr<CCGraphicsContext> m_context;
    OwnPtr<CCResourceProvider> m_resourceProvider;
};

TEST_P(CCResourceProviderTest, Basic)
{
    IntSize size(1, 1);
    WGC3Denum format = GraphicsContext3D::RGBA;
    int pool = 1;
    size_t pixelSize = textureSize(size, format);
    ASSERT_EQ(4U, pixelSize);

    CCResourceProvider::ResourceId id = m_resourceProvider->createResource(pool, size, format, CCResourceProvider::TextureUsageAny);
    expectNumResources(1);

    uint8_t data[4] = {1, 2, 3, 4};
    IntRect rect(IntPoint(), size);
    m_resourceProvider->upload(id, data, rect, rect, IntSize());

    uint8_t result[4] = {0};
    getResourcePixels(id, size, format, result);
    EXPECT_EQ(0, memcmp(data, result, pixelSize));

    m_resourceProvider->deleteResource(id);
    expectNumResources(0);
}

TEST_P(CCResourceProviderTest, DeleteOwnedResources)
{
    IntSize size(1, 1);
    WGC3Denum format = GraphicsContext3D::RGBA;
    int pool = 1;

    const int count = 3;
    for (int i = 0; i < count; ++i)
        m_resourceProvider->createResource(pool, size, format, CCResourceProvider::TextureUsageAny);
    expectNumResources(3);

    m_resourceProvider->deleteOwnedResources(pool+1);
    expectNumResources(3);

    m_resourceProvider->deleteOwnedResources(pool);
    expectNumResources(0);
}

TEST_P(CCResourceProviderTest, Upload)
{
    IntSize size(2, 2);
    WGC3Denum format = GraphicsContext3D::RGBA;
    int pool = 1;
    size_t pixelSize = textureSize(size, format);
    ASSERT_EQ(16U, pixelSize);

    CCResourceProvider::ResourceId id = m_resourceProvider->createResource(pool, size, format, CCResourceProvider::TextureUsageAny);

    uint8_t image[16] = {0};
    IntRect imageRect(IntPoint(), size);
    m_resourceProvider->upload(id, image, imageRect, imageRect, IntSize());

    for (uint8_t i = 0 ; i < pixelSize; ++i)
        image[i] = i;

    uint8_t result[16] = {0};
    {
        IntRect sourceRect(0, 0, 1, 1);
        IntSize destOffset(0, 0);
        m_resourceProvider->upload(id, image, imageRect, sourceRect, destOffset);

        uint8_t expected[16] = {0, 1, 2, 3,   0, 0, 0, 0,
                                0, 0, 0, 0,   0, 0, 0, 0};
        getResourcePixels(id, size, format, result);
        EXPECT_EQ(0, memcmp(expected, result, pixelSize));
    }
    {
        IntRect sourceRect(0, 0, 1, 1);
        IntSize destOffset(1, 1);
        m_resourceProvider->upload(id, image, imageRect, sourceRect, destOffset);

        uint8_t expected[16] = {0, 1, 2, 3,   0, 0, 0, 0,
                                0, 0, 0, 0,   0, 1, 2, 3};
        getResourcePixels(id, size, format, result);
        EXPECT_EQ(0, memcmp(expected, result, pixelSize));
    }
    {
        IntRect sourceRect(1, 0, 1, 1);
        IntSize destOffset(0, 1);
        m_resourceProvider->upload(id, image, imageRect, sourceRect, destOffset);

        uint8_t expected[16] = {0, 1, 2, 3,   0, 0, 0, 0,
                                4, 5, 6, 7,   0, 1, 2, 3};
        getResourcePixels(id, size, format, result);
        EXPECT_EQ(0, memcmp(expected, result, pixelSize));
    }
    {
        IntRect offsetImageRect(IntPoint(100, 100), size);
        IntRect sourceRect(100, 100, 1, 1);
        IntSize destOffset(1, 0);
        m_resourceProvider->upload(id, image, offsetImageRect, sourceRect, destOffset);

        uint8_t expected[16] = {0, 1, 2, 3,   0, 1, 2, 3,
                                4, 5, 6, 7,   0, 1, 2, 3};
        getResourcePixels(id, size, format, result);
        EXPECT_EQ(0, memcmp(expected, result, pixelSize));
    }


    m_resourceProvider->deleteResource(id);
}

TEST_P(CCResourceProviderTest, TransferResources)
{
    // Resource transfer is only supported with GL textures for now.
    if (GetParam() != CCResourceProvider::GLTexture)
        return;

    OwnPtr<CCGraphicsContext> childContext(FakeWebCompositorOutputSurface::create(ResourceProviderContext::create(m_sharedData.get())));
    OwnPtr<CCResourceProvider> childResourceProvider(CCResourceProvider::create(childContext.get()));

    IntSize size(1, 1);
    WGC3Denum format = GraphicsContext3D::RGBA;
    int pool = 1;
    size_t pixelSize = textureSize(size, format);
    ASSERT_EQ(4U, pixelSize);

    CCResourceProvider::ResourceId id1 = childResourceProvider->createResource(pool, size, format, CCResourceProvider::TextureUsageAny);
    uint8_t data1[4] = {1, 2, 3, 4};
    IntRect rect(IntPoint(), size);
    childResourceProvider->upload(id1, data1, rect, rect, IntSize());

    CCResourceProvider::ResourceId id2 = childResourceProvider->createResource(pool, size, format, CCResourceProvider::TextureUsageAny);
    uint8_t data2[4] = {5, 5, 5, 5};
    childResourceProvider->upload(id2, data2, rect, rect, IntSize());

    int childPool = 2;
    int childId = m_resourceProvider->createChild(childPool);

    {
        // Transfer some resources to the parent.
        CCResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.append(id1);
        resourceIdsToTransfer.append(id2);
        CCResourceProvider::TransferableResourceList list = childResourceProvider->prepareSendToParent(resourceIdsToTransfer);
        EXPECT_NE(0u, list.syncPoint);
        EXPECT_EQ(2u, list.resources.size());
        EXPECT_TRUE(childResourceProvider->inUseByConsumer(id1));
        EXPECT_TRUE(childResourceProvider->inUseByConsumer(id2));
        m_resourceProvider->receiveFromChild(childId, list);
    }

    EXPECT_EQ(2u, m_resourceProvider->numResources());
    EXPECT_EQ(2u, m_resourceProvider->mailboxCount());
    CCResourceProvider::ResourceIdMap resourceMap = m_resourceProvider->getChildToParentMap(childId);
    CCResourceProvider::ResourceId mappedId1 = resourceMap.get(id1);
    CCResourceProvider::ResourceId mappedId2 = resourceMap.get(id2);
    EXPECT_NE(0u, mappedId1);
    EXPECT_NE(0u, mappedId2);
    EXPECT_FALSE(m_resourceProvider->inUseByConsumer(id1));
    EXPECT_FALSE(m_resourceProvider->inUseByConsumer(id2));

    uint8_t result[4] = {0};
    getResourcePixels(mappedId1, size, format, result);
    EXPECT_EQ(0, memcmp(data1, result, pixelSize));

    getResourcePixels(mappedId2, size, format, result);
    EXPECT_EQ(0, memcmp(data2, result, pixelSize));

    {
        // Check that transfering again the same resource from the child to the
        // parent is a noop.
        CCResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.append(id1);
        CCResourceProvider::TransferableResourceList list = childResourceProvider->prepareSendToParent(resourceIdsToTransfer);
        EXPECT_EQ(0u, list.syncPoint);
        EXPECT_EQ(0u, list.resources.size());
    }

    {
        // Transfer resources back from the parent to the child.
        CCResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.append(mappedId1);
        resourceIdsToTransfer.append(mappedId2);
        CCResourceProvider::TransferableResourceList list = m_resourceProvider->prepareSendToChild(childId, resourceIdsToTransfer);
        EXPECT_NE(0u, list.syncPoint);
        EXPECT_EQ(2u, list.resources.size());
        childResourceProvider->receiveFromParent(list);
    }
    EXPECT_EQ(0u, m_resourceProvider->mailboxCount());
    EXPECT_EQ(2u, childResourceProvider->mailboxCount());
    EXPECT_FALSE(childResourceProvider->inUseByConsumer(id1));
    EXPECT_FALSE(childResourceProvider->inUseByConsumer(id2));

    ResourceProviderContext* childContext3D = static_cast<ResourceProviderContext*>(childContext->context3D());
    {
        CCResourceProvider::ScopedReadLockGL lock(childResourceProvider.get(), id1);
        ASSERT_NE(0U, lock.textureId());
        childContext3D->bindTexture(GraphicsContext3D::TEXTURE_2D, lock.textureId());
        childContext3D->getPixels(size, format, result);
        EXPECT_EQ(0, memcmp(data1, result, pixelSize));
    }
    {
        CCResourceProvider::ScopedReadLockGL lock(childResourceProvider.get(), id2);
        ASSERT_NE(0U, lock.textureId());
        childContext3D->bindTexture(GraphicsContext3D::TEXTURE_2D, lock.textureId());
        childContext3D->getPixels(size, format, result);
        EXPECT_EQ(0, memcmp(data2, result, pixelSize));
    }

    {
        // Transfer resources to the parent again.
        CCResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.append(id1);
        resourceIdsToTransfer.append(id2);
        CCResourceProvider::TransferableResourceList list = childResourceProvider->prepareSendToParent(resourceIdsToTransfer);
        EXPECT_NE(0u, list.syncPoint);
        EXPECT_EQ(2u, list.resources.size());
        EXPECT_TRUE(childResourceProvider->inUseByConsumer(id1));
        EXPECT_TRUE(childResourceProvider->inUseByConsumer(id2));
        m_resourceProvider->receiveFromChild(childId, list);
    }

    EXPECT_EQ(2u, m_resourceProvider->numResources());
    m_resourceProvider->destroyChild(childId);
    EXPECT_EQ(0u, m_resourceProvider->numResources());
    EXPECT_EQ(0u, m_resourceProvider->mailboxCount());
}

INSTANTIATE_TEST_CASE_P(CCResourceProviderTests,
                        CCResourceProviderTest,
                        ::testing::Values(CCResourceProvider::GLTexture,
                                          CCResourceProvider::Bitmap));

} // namespace
