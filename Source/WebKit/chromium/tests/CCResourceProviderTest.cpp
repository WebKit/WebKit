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

#include "cc/CCResourceProvider.h"

#include "CompositorFakeWebGraphicsContext3D.h"
#include "Extensions3DChromium.h"
#include "FakeWebCompositorOutputSurface.h"
#include "cc/CCGraphicsContext.h"
#include "cc/CCSingleThreadProxy.h" // For DebugScopedSetImplThread
#include <gtest/gtest.h>
#include <public/WebGraphicsContext3D.h>
#include <wtf/HashMap.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/OwnPtr.h>

using namespace WebCore;
using namespace WebKit;

namespace {

class ResourceProviderContext : public CompositorFakeWebGraphicsContext3D {
public:
    static PassOwnPtr<ResourceProviderContext> create() { return adoptPtr(new ResourceProviderContext(Attributes())); }

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

    static size_t textureSize(const IntSize& size, WGC3Denum format)
    {
        unsigned int componentsPerPixel = 4;
        unsigned int bytesPerComponent = 1;
        GraphicsContext3D::computeFormatAndTypeParameters(format, GraphicsContext3D::UNSIGNED_BYTE, &componentsPerPixel, &bytesPerComponent);
        return size.width() * size.height() * componentsPerPixel * bytesPerComponent;
    }

protected:
    explicit ResourceProviderContext(const Attributes& attrs)
        : CompositorFakeWebGraphicsContext3D(attrs)
        , m_currentTexture(0)
    { }

private:
    struct Texture {
        Texture(const IntSize& size_, WGC3Denum format_)
            : size(size_)
            , format(format_)
            , data(adoptArrayPtr(new uint8_t[textureSize(size, format)]))
        {
        }

        IntSize size;
        WGC3Denum format;
        OwnArrayPtr<uint8_t> data;
    };

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
    WebGLId m_currentTexture;
    TextureMap m_textures;
};

class CCResourceProviderTest : public testing::Test {
public:
    CCResourceProviderTest()
        : m_context(FakeWebCompositorOutputSurface::create(ResourceProviderContext::create()))
        , m_resourceProvider(CCResourceProvider::create(m_context.get()))
    {
    }

    ResourceProviderContext* context() { return static_cast<ResourceProviderContext*>(m_context->context3D()); }

    void getResourcePixels(CCResourceProvider::ResourceId id, const IntSize& size, WGC3Denum format, uint8_t* pixels)
    {
        CCScopedLockResourceForRead lock(m_resourceProvider.get(), id);
        ASSERT_NE(0U, lock.textureId());
        context()->bindTexture(GraphicsContext3D::TEXTURE_2D, lock.textureId());
        context()->getPixels(size, format, pixels);
    }

protected:
    DebugScopedSetImplThread implThread;
    OwnPtr<CCGraphicsContext> m_context;
    OwnPtr<CCResourceProvider> m_resourceProvider;
};

TEST_F(CCResourceProviderTest, Basic)
{
    IntSize size(1, 1);
    WGC3Denum format = GraphicsContext3D::RGBA;
    int pool = 1;
    size_t pixelSize = ResourceProviderContext::textureSize(size, format);
    ASSERT_EQ(4U, pixelSize);

    CCResourceProvider::ResourceId id = m_resourceProvider->createResource(pool, size, format, CCResourceProvider::TextureUsageAny);
    EXPECT_EQ(1, context()->textureCount());

    uint8_t data[4] = {1, 2, 3, 4};
    IntRect rect(IntPoint(), size);
    m_resourceProvider->upload(id, data, rect, rect, IntSize());

    uint8_t result[4] = {0};
    getResourcePixels(id, size, format, result);
    EXPECT_EQ(0, memcmp(data, result, pixelSize));

    m_resourceProvider->deleteResource(id);
    EXPECT_EQ(0, context()->textureCount());
}

TEST_F(CCResourceProviderTest, DeleteOwnedResources)
{
    IntSize size(1, 1);
    WGC3Denum format = GraphicsContext3D::RGBA;
    int pool = 1;

    const int count = 3;
    for (int i = 0; i < count; ++i)
        m_resourceProvider->createResource(pool, size, format, CCResourceProvider::TextureUsageAny);
    EXPECT_EQ(3, context()->textureCount());

    m_resourceProvider->deleteOwnedResources(pool+1);
    EXPECT_EQ(3, context()->textureCount());

    m_resourceProvider->deleteOwnedResources(pool);
    EXPECT_EQ(0, context()->textureCount());
}

TEST_F(CCResourceProviderTest, Upload)
{
    IntSize size(2, 2);
    WGC3Denum format = GraphicsContext3D::RGBA;
    int pool = 1;
    size_t pixelSize = ResourceProviderContext::textureSize(size, format);
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

    m_resourceProvider->deleteResource(id);
}

} // namespace
