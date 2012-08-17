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

#include "CCScopedTexture.h"

#include "CCRenderer.h"
#include "CCSingleThreadProxy.h" // For DebugScopedSetImplThread
#include "CCTiledLayerTestCommon.h"
#include "FakeCCGraphicsContext.h"
#include "GraphicsContext3D.h"
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKit;
using namespace WebKitTests;

namespace {

TEST(CCScopedTextureTest, NewScopedTexture)
{
    OwnPtr<CCGraphicsContext> context(createFakeCCGraphicsContext());
    DebugScopedSetImplThread implThread;
    OwnPtr<CCResourceProvider> resourceProvider(CCResourceProvider::create(context.get()));
    OwnPtr<CCScopedTexture> texture = CCScopedTexture::create(resourceProvider.get());

    // New scoped textures do not hold a texture yet.
    EXPECT_EQ(0u, texture->id());

    // New scoped textures do not have a size yet.
    EXPECT_EQ(IntSize(), texture->size());
    EXPECT_EQ(0u, texture->bytes());
}

TEST(CCScopedTextureTest, CreateScopedTexture)
{
    OwnPtr<CCGraphicsContext> context(createFakeCCGraphicsContext());
    DebugScopedSetImplThread implThread;
    OwnPtr<CCResourceProvider> resourceProvider(CCResourceProvider::create(context.get()));
    OwnPtr<CCScopedTexture> texture = CCScopedTexture::create(resourceProvider.get());
    texture->allocate(CCRenderer::ImplPool, IntSize(30, 30), GraphicsContext3D::RGBA, CCResourceProvider::TextureUsageAny);

    // The texture has an allocated byte-size now.
    size_t expectedBytes = 30 * 30 * 4;
    EXPECT_EQ(expectedBytes, texture->bytes());

    EXPECT_LT(0u, texture->id());
    EXPECT_EQ(GraphicsContext3D::RGBA, texture->format());
    EXPECT_EQ(IntSize(30, 30), texture->size());
}

TEST(CCScopedTextureTest, ScopedTextureIsDeleted)
{
    OwnPtr<CCGraphicsContext> context(createFakeCCGraphicsContext());
    DebugScopedSetImplThread implThread;
    OwnPtr<CCResourceProvider> resourceProvider(CCResourceProvider::create(context.get()));

    {
        OwnPtr<CCScopedTexture> texture = CCScopedTexture::create(resourceProvider.get());

        EXPECT_EQ(0u, resourceProvider->numResources());
        texture->allocate(CCRenderer::ImplPool, IntSize(30, 30), GraphicsContext3D::RGBA, CCResourceProvider::TextureUsageAny);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());
    }

    EXPECT_EQ(0u, resourceProvider->numResources());

    {
        OwnPtr<CCScopedTexture> texture = CCScopedTexture::create(resourceProvider.get());
        EXPECT_EQ(0u, resourceProvider->numResources());
        texture->allocate(CCRenderer::ImplPool, IntSize(30, 30), GraphicsContext3D::RGBA, CCResourceProvider::TextureUsageAny);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());
        texture->free();
        EXPECT_EQ(0u, resourceProvider->numResources());
    }
}

TEST(CCScopedTextureTest, LeakScopedTexture)
{
    OwnPtr<CCGraphicsContext> context(createFakeCCGraphicsContext());
    DebugScopedSetImplThread implThread;
    OwnPtr<CCResourceProvider> resourceProvider(CCResourceProvider::create(context.get()));

    {
        OwnPtr<CCScopedTexture> texture = CCScopedTexture::create(resourceProvider.get());

        EXPECT_EQ(0u, resourceProvider->numResources());
        texture->allocate(CCRenderer::ImplPool, IntSize(30, 30), GraphicsContext3D::RGBA, CCResourceProvider::TextureUsageAny);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());

        texture->leak();
        EXPECT_EQ(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());

        texture->free();
        EXPECT_EQ(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());
    }

    EXPECT_EQ(1u, resourceProvider->numResources());
}

}
