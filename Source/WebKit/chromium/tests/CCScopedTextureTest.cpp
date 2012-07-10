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

#include "cc/CCScopedTexture.h"

#include "CCTiledLayerTestCommon.h"
#include "GraphicsContext3D.h"

#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKit;
using namespace WebKitTests;

namespace {

TEST(CCScopedTextureTest, NewScopedTexture)
{
    FakeTextureAllocator allocator;
    OwnPtr<CCScopedTexture> texture = CCScopedTexture::create(&allocator);

    // New scoped textures do not hold a texture yet.
    EXPECT_EQ(0u, texture->id());

    // New scoped textures do not have a size yet.
    EXPECT_EQ(IntSize(), texture->size());
    EXPECT_EQ(0u, texture->bytes());
}

TEST(CCScopedTextureTest, CreateScopedTexture)
{
    FakeTextureAllocator allocator;
    OwnPtr<CCScopedTexture> texture = CCScopedTexture::create(&allocator);
    texture->allocate(IntSize(30, 30), GraphicsContext3D::RGBA);

    // The texture has an allocated byte-size now.
    size_t expectedBytes = 30 * 30 * 4;
    EXPECT_EQ(expectedBytes, texture->bytes());

    EXPECT_LT(0u, texture->id());
    EXPECT_EQ(GraphicsContext3D::RGBA, texture->format());
    EXPECT_EQ(IntSize(30, 30), texture->size());
}

// Fake TextureAllocator that tracks the number of textures in use.
class TrackingTextureAllocator : public TextureAllocator {
public:
    TrackingTextureAllocator()
        : m_nextTextureId(1)
        , m_numTextures(0)
    { }

    virtual unsigned createTexture(const WebCore::IntSize&, GC3Denum) OVERRIDE
    {
        unsigned id = m_nextTextureId;
        ++m_nextTextureId;

        m_textures.set(id, true);
        ++m_numTextures;
        return id;
    }

    virtual void deleteTexture(unsigned id, const WebCore::IntSize&, GC3Denum) OVERRIDE
    {
        if (!m_textures.get(id))
            return;

        m_textures.set(id, false);
        --m_numTextures;
    }

    virtual void deleteAllTextures() OVERRIDE
    {
        m_textures.clear();
        m_numTextures = 0;
    }

    unsigned numTextures() const { return m_numTextures; }

private:
    unsigned m_nextTextureId;
    HashMap<unsigned, bool> m_textures;
    unsigned m_numTextures;
};

TEST(CCScopedTextureTest, ScopedTextureIsDeleted)
{
    TrackingTextureAllocator allocator;

    {
        OwnPtr<CCScopedTexture> texture = CCScopedTexture::create(&allocator);

        EXPECT_EQ(0u, allocator.numTextures());
        texture->allocate(IntSize(30, 30), GraphicsContext3D::RGBA);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, allocator.numTextures());
    }

    EXPECT_EQ(0u, allocator.numTextures());

    {
        OwnPtr<CCScopedTexture> texture = CCScopedTexture::create(&allocator);
        EXPECT_EQ(0u, allocator.numTextures());
        texture->allocate(IntSize(30, 30), GraphicsContext3D::RGBA);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, allocator.numTextures());
        texture->free();
        EXPECT_EQ(0u, allocator.numTextures());
    }
}

TEST(CCScopedTextureTest, LeakScopedTexture)
{
    TrackingTextureAllocator allocator;

    {
        OwnPtr<CCScopedTexture> texture = CCScopedTexture::create(&allocator);

        EXPECT_EQ(0u, allocator.numTextures());
        texture->allocate(IntSize(30, 30), GraphicsContext3D::RGBA);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, allocator.numTextures());

        texture->leak();
        EXPECT_EQ(0u, texture->id());
        EXPECT_EQ(1u, allocator.numTextures());

        texture->free();
        EXPECT_EQ(0u, texture->id());
        EXPECT_EQ(1u, allocator.numTextures());
    }

    EXPECT_EQ(1u, allocator.numTextures());
}

}
