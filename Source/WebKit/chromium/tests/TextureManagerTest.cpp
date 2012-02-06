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

#include "ManagedTexture.h"
#include "TextureManager.h"

#include <gtest/gtest.h>

using namespace WebCore;
using namespace WTF;

namespace {

class FakeTextureAllocator : public TextureAllocator {
public:
    virtual ~FakeTextureAllocator() { }
    virtual unsigned createTexture(const IntSize&, GC3Denum) { return 1; }
    virtual void deleteTexture(unsigned, const IntSize&, GC3Denum) { }
};

class TextureManagerTest : public testing::Test {
public:
    TextureManagerTest()
        : m_textureSize(256, 256)
        , m_textureFormat(GraphicsContext3D::RGBA)
    {
    }

    virtual ~TextureManagerTest()
    {
    }

    size_t texturesMemorySize(size_t textureCount)
    {
        return TextureManager::memoryUseBytes(m_textureSize, m_textureFormat) * textureCount;
    }

    PassOwnPtr<TextureManager> createTextureManager(size_t maxTextures, size_t preferredTextures)
    {
        return TextureManager::create(texturesMemorySize(maxTextures), texturesMemorySize(preferredTextures), 1024);
    }

    bool requestTexture(TextureManager* manager, TextureToken token)
    {
        unsigned textureId;
        bool result = manager->requestTexture(token, m_textureSize, m_textureFormat, textureId);
        if (result)
            manager->allocateTexture(&m_fakeTextureAllocator, token);
        return result;
    }

private:
    FakeTextureAllocator m_fakeTextureAllocator;
    const IntSize m_textureSize;
    const GC3Denum m_textureFormat;
};

TEST_F(TextureManagerTest, requestTextureInPreferredLimit)
{
    const size_t preferredTextures = 8;
    OwnPtr<TextureManager> textureManager = createTextureManager(preferredTextures * 2, preferredTextures);
    TextureToken tokens[preferredTextures];
    for (size_t i = 0; i < preferredTextures; ++i) {
        tokens[i] = textureManager->getToken();
        if (i)
            EXPECT_GT(tokens[i], tokens[i - 1]);
        EXPECT_TRUE(requestTexture(textureManager.get(), tokens[i]));
        EXPECT_TRUE(textureManager->hasTexture(tokens[i]));
        EXPECT_TRUE(textureManager->isProtected(tokens[i]));
    }

    for (size_t i = 0; i < preferredTextures; ++i)
        EXPECT_TRUE(textureManager->hasTexture(tokens[i]));

    EXPECT_EQ(texturesMemorySize(preferredTextures), textureManager->currentMemoryUseBytes());
}

TEST_F(TextureManagerTest, requestTextureExceedingPreferredLimit)
{
    const size_t maxTextures = 8;
    const size_t preferredTextures = 4;
    OwnPtr<TextureManager> textureManager = createTextureManager(maxTextures, preferredTextures);
    TextureToken tokens[maxTextures];
    for (size_t i = 0; i < preferredTextures; ++i) {
        tokens[i] = textureManager->getToken();
        EXPECT_TRUE(requestTexture(textureManager.get(), tokens[i]));
        EXPECT_TRUE(textureManager->hasTexture(tokens[i]));
    }

    textureManager->unprotectTexture(tokens[0]);
    textureManager->unprotectTexture(tokens[2]);

    for (size_t i = preferredTextures; i < maxTextures; ++i) {
        tokens[i] = textureManager->getToken();
        EXPECT_TRUE(requestTexture(textureManager.get(), tokens[i]));
        EXPECT_TRUE(textureManager->hasTexture(tokens[i]));
        textureManager->unprotectTexture(tokens[i]);
    }

    EXPECT_FALSE(textureManager->hasTexture(tokens[0]));
    EXPECT_TRUE(textureManager->hasTexture(tokens[1]));
    EXPECT_TRUE(textureManager->isProtected(tokens[1]));
    EXPECT_FALSE(textureManager->hasTexture(tokens[2]));
    EXPECT_TRUE(textureManager->hasTexture(tokens[3]));
    EXPECT_TRUE(textureManager->isProtected(tokens[3]));

    EXPECT_EQ(texturesMemorySize(preferredTextures), textureManager->currentMemoryUseBytes());
}

TEST_F(TextureManagerTest, requestTextureExceedingMaxLimit)
{
    const size_t maxTextures = 8;
    const size_t preferredTextures = 4;
    OwnPtr<TextureManager> textureManager = createTextureManager(maxTextures, preferredTextures);
    TextureToken tokens[maxTextures];
    for (size_t i = 0; i < maxTextures; ++i) {
        tokens[i] = textureManager->getToken();
        EXPECT_TRUE(requestTexture(textureManager.get(), tokens[i]));
        EXPECT_TRUE(textureManager->hasTexture(tokens[i]));
    }

    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->currentMemoryUseBytes());

    for (size_t i = 0; i < maxTextures; ++i) {
        TextureToken token = textureManager->getToken();
        EXPECT_FALSE(requestTexture(textureManager.get(), token));
        EXPECT_FALSE(textureManager->hasTexture(token));
    }

    EXPECT_EQ(textureManager->currentMemoryUseBytes(), texturesMemorySize(maxTextures));

    textureManager->unprotectTexture(tokens[1]);
    textureManager->unprotectTexture(tokens[3]);
    EXPECT_TRUE(requestTexture(textureManager.get(), textureManager->getToken()));
    EXPECT_TRUE(requestTexture(textureManager.get(), textureManager->getToken()));
    EXPECT_FALSE(requestTexture(textureManager.get(), textureManager->getToken()));
    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->currentMemoryUseBytes());
    EXPECT_FALSE(textureManager->hasTexture(tokens[1]));
    EXPECT_FALSE(textureManager->hasTexture(tokens[3]));
}

TEST_F(TextureManagerTest, reduceMemoryToLimit)
{
    const size_t maxTextures = 8;
    const size_t preferredTextures = 4;
    OwnPtr<TextureManager> textureManager = createTextureManager(maxTextures, preferredTextures);
    TextureToken tokens[maxTextures];
    for (size_t i = 0; i < maxTextures; ++i) {
        tokens[i] = textureManager->getToken();
        EXPECT_TRUE(requestTexture(textureManager.get(), tokens[i]));
    }

    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->currentMemoryUseBytes());
    textureManager->reduceMemoryToLimit(texturesMemorySize(maxTextures));
    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->currentMemoryUseBytes());
    textureManager->reduceMemoryToLimit(texturesMemorySize(preferredTextures));
    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->currentMemoryUseBytes());

    const size_t unprotectedTextures = preferredTextures + 1;
    for (size_t i = 0; i < preferredTextures + 1; ++i)
        textureManager->unprotectTexture(tokens[i]);

    textureManager->reduceMemoryToLimit(texturesMemorySize(maxTextures));
    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->currentMemoryUseBytes());
    textureManager->reduceMemoryToLimit(texturesMemorySize(preferredTextures));
    EXPECT_EQ(texturesMemorySize(preferredTextures), textureManager->currentMemoryUseBytes());
    textureManager->reduceMemoryToLimit(texturesMemorySize(1));
    EXPECT_EQ(texturesMemorySize(maxTextures - unprotectedTextures), textureManager->currentMemoryUseBytes());

    // reduceMemoryToLimit doesn't change the current memory limits.
    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->maxMemoryLimitBytes());
    EXPECT_EQ(texturesMemorySize(preferredTextures), textureManager->preferredMemoryLimitBytes());
}

TEST_F(TextureManagerTest, setMaxMemoryLimitBytes)
{
    const size_t maxTextures = 8;
    const size_t preferredTextures = 4;
    OwnPtr<TextureManager> textureManager = createTextureManager(maxTextures, preferredTextures);
    TextureToken tokens[maxTextures];
    for (size_t i = 0; i < maxTextures; ++i) {
        tokens[i] = textureManager->getToken();
        EXPECT_TRUE(requestTexture(textureManager.get(), tokens[i]));
    }

    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->currentMemoryUseBytes());

    const size_t unprotectedTextures = preferredTextures + 1;
    for (size_t i = 0; i < unprotectedTextures; ++i)
        textureManager->unprotectTexture(tokens[i]);

    textureManager->setMaxMemoryLimitBytes(texturesMemorySize(maxTextures));
    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->currentMemoryUseBytes());
    textureManager->setMaxMemoryLimitBytes(texturesMemorySize(preferredTextures));
    EXPECT_EQ(texturesMemorySize(preferredTextures), textureManager->currentMemoryUseBytes());
    EXPECT_EQ(texturesMemorySize(preferredTextures), textureManager->maxMemoryLimitBytes());
}

TEST_F(TextureManagerTest, setPreferredMemoryLimitBytes)
{
    const size_t maxTextures = 8;
    const size_t preferredTextures = 4;
    OwnPtr<TextureManager> textureManager = createTextureManager(maxTextures, preferredTextures);
    TextureToken tokens[maxTextures];
    for (size_t i = 0; i < maxTextures; ++i) {
        tokens[i] = textureManager->getToken();
        EXPECT_TRUE(requestTexture(textureManager.get(), tokens[i]));
    }

    const size_t unprotectedTextures = preferredTextures + 1;
    for (size_t i = 0; i < unprotectedTextures; ++i)
        textureManager->unprotectTexture(tokens[i]);

    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->currentMemoryUseBytes());
    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->maxMemoryLimitBytes());

    // Setting preferred memory limit only won't force reduceMemoryToLimit.
    textureManager->setPreferredMemoryLimitBytes(texturesMemorySize(preferredTextures));
    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->currentMemoryUseBytes());
    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->maxMemoryLimitBytes());
    EXPECT_EQ(texturesMemorySize(preferredTextures), textureManager->preferredMemoryLimitBytes());
}

TEST_F(TextureManagerTest, textureManagerDestroyedBeforeManagedTexture)
{
    OwnPtr<TextureManager> textureManager = createTextureManager(1, 1);
    OwnPtr<ManagedTexture> managedTexture = ManagedTexture::create(textureManager.get());

    IntSize size(50, 50);
    unsigned format = GraphicsContext3D::RGBA;

    // Texture is initially invalid, but we should be able to reserve.
    EXPECT_FALSE(managedTexture->isValid(size, format));
    EXPECT_TRUE(managedTexture->reserve(size, format));
    EXPECT_TRUE(managedTexture->isValid(size, format));

    textureManager.clear();

    // Deleting the manager should invalidate the texture and reservation attempts should fail.
    EXPECT_FALSE(managedTexture->isValid(size, format));
    EXPECT_FALSE(managedTexture->reserve(size, format));
}

} // namespace
