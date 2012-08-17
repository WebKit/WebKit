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

#include "CCPrioritizedTexture.h"

#include "CCPrioritizedTextureManager.h"
#include "CCSingleThreadProxy.h" // For DebugScopedSetImplThread
#include "CCTexture.h"
#include "CCTiledLayerTestCommon.h"
#include "FakeCCGraphicsContext.h"
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKitTests;
using namespace WTF;

namespace {

class CCPrioritizedTextureTest : public testing::Test {
public:
    CCPrioritizedTextureTest()
        : m_textureSize(256, 256)
        , m_textureFormat(GraphicsContext3D::RGBA)
        , m_context(WebKit::createFakeCCGraphicsContext())
    {
        DebugScopedSetImplThread implThread;
        m_resourceProvider = CCResourceProvider::create(m_context.get());
    }

    virtual ~CCPrioritizedTextureTest()
    {
        DebugScopedSetImplThread implThread;
        m_resourceProvider.clear();
    }

    size_t texturesMemorySize(size_t textureCount)
    {
        return CCTexture::memorySizeBytes(m_textureSize, m_textureFormat) * textureCount;
    }

    PassOwnPtr<CCPrioritizedTextureManager> createManager(size_t maxTextures)
    {
        return CCPrioritizedTextureManager::create(texturesMemorySize(maxTextures), 1024, 0);
    }

    bool validateTexture(OwnPtr<CCPrioritizedTexture>& texture, bool requestLate)
    {
        DebugScopedSetImplThread implThread;
#if !ASSERT_DISABLED
        texture->textureManager()->assertInvariants();
#endif
        if (requestLate)
            texture->requestLate();
        bool success = texture->canAcquireBackingTexture();
        if (success)
            texture->acquireBackingTexture(resourceProvider());
        return success;
    }

    CCResourceProvider* resourceProvider()
    {
       return m_resourceProvider.get();
    }

protected:
    const IntSize m_textureSize;
    const GC3Denum m_textureFormat;
    OwnPtr<CCGraphicsContext> m_context;
    OwnPtr<CCResourceProvider> m_resourceProvider;
};

TEST_F(CCPrioritizedTextureTest, requestTextureExceedingMaxLimit)
{
    const size_t maxTextures = 8;
    OwnPtr<CCPrioritizedTextureManager> textureManager = createManager(maxTextures);

    // Create textures for double our memory limit.
    OwnPtr<CCPrioritizedTexture> textures[maxTextures*2];

    for (size_t i = 0; i < maxTextures*2; ++i)
        textures[i] = textureManager->createTexture(m_textureSize, m_textureFormat);

    // Set decreasing priorities
    for (size_t i = 0; i < maxTextures*2; ++i)
        textures[i]->setRequestPriority(100 + i);

    // Only lower half should be available.
    textureManager->prioritizeTextures();
    EXPECT_TRUE(validateTexture(textures[0], false));
    EXPECT_TRUE(validateTexture(textures[7], false));
    EXPECT_FALSE(validateTexture(textures[8], false));
    EXPECT_FALSE(validateTexture(textures[15], false));

    // Set increasing priorities
    for (size_t i = 0; i < maxTextures*2; ++i)
        textures[i]->setRequestPriority(100 - i);

    // Only upper half should be available.
    textureManager->prioritizeTextures();
    EXPECT_FALSE(validateTexture(textures[0], false));
    EXPECT_FALSE(validateTexture(textures[7], false));
    EXPECT_TRUE(validateTexture(textures[8], false));
    EXPECT_TRUE(validateTexture(textures[15], false));

    EXPECT_EQ(texturesMemorySize(maxTextures), textureManager->memoryAboveCutoffBytes());
    EXPECT_LE(textureManager->memoryUseBytes(), textureManager->memoryAboveCutoffBytes());

    DebugScopedSetImplThread implThread;
    textureManager->clearAllMemory(resourceProvider());
}

TEST_F(CCPrioritizedTextureTest, changeMemoryLimits)
{
    const size_t maxTextures = 8;
    OwnPtr<CCPrioritizedTextureManager> textureManager = createManager(maxTextures);
    OwnPtr<CCPrioritizedTexture> textures[maxTextures];

    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = textureManager->createTexture(m_textureSize, m_textureFormat);
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i]->setRequestPriority(100 + i);

    // Set max limit to 8 textures
    textureManager->setMaxMemoryLimitBytes(texturesMemorySize(8));
    textureManager->prioritizeTextures();
    for (size_t i = 0; i < maxTextures; ++i)
        validateTexture(textures[i], false);
    {
        DebugScopedSetImplThread implThread;
        textureManager->reduceMemory(resourceProvider());
    }

    EXPECT_EQ(texturesMemorySize(8), textureManager->memoryAboveCutoffBytes());
    EXPECT_LE(textureManager->memoryUseBytes(), textureManager->memoryAboveCutoffBytes());

    // Set max limit to 5 textures
    textureManager->setMaxMemoryLimitBytes(texturesMemorySize(5));
    textureManager->prioritizeTextures();
    for (size_t i = 0; i < maxTextures; ++i)
        EXPECT_EQ(validateTexture(textures[i], false), i < 5);
    {
        DebugScopedSetImplThread implThread;
        textureManager->reduceMemory(resourceProvider());
    }

    EXPECT_EQ(texturesMemorySize(5), textureManager->memoryAboveCutoffBytes());
    EXPECT_LE(textureManager->memoryUseBytes(), textureManager->memoryAboveCutoffBytes());

    // Set max limit to 4 textures
    textureManager->setMaxMemoryLimitBytes(texturesMemorySize(4));
    textureManager->prioritizeTextures();
    for (size_t i = 0; i < maxTextures; ++i)
        EXPECT_EQ(validateTexture(textures[i], false), i < 4);
    {
        DebugScopedSetImplThread implThread;
        textureManager->reduceMemory(resourceProvider());
    }

    EXPECT_EQ(texturesMemorySize(4), textureManager->memoryAboveCutoffBytes());
    EXPECT_LE(textureManager->memoryUseBytes(), textureManager->memoryAboveCutoffBytes());

    DebugScopedSetImplThread implThread;
    textureManager->clearAllMemory(resourceProvider());
}

TEST_F(CCPrioritizedTextureTest, textureManagerPartialUpdateTextures)
{
    const size_t maxTextures = 4;
    const size_t numTextures = 4;
    OwnPtr<CCPrioritizedTextureManager> textureManager = createManager(maxTextures);
    OwnPtr<CCPrioritizedTexture> textures[numTextures];
    OwnPtr<CCPrioritizedTexture> moreTextures[numTextures];

    for (size_t i = 0; i < numTextures; ++i) {
        textures[i] = textureManager->createTexture(m_textureSize, m_textureFormat);
        moreTextures[i] = textureManager->createTexture(m_textureSize, m_textureFormat);
    }

    for (size_t i = 0; i < numTextures; ++i)
        textures[i]->setRequestPriority(200 + i);
    textureManager->prioritizeTextures();

    // Allocate textures which are currently high priority.
    EXPECT_TRUE(validateTexture(textures[0], false));
    EXPECT_TRUE(validateTexture(textures[1], false));
    EXPECT_TRUE(validateTexture(textures[2], false));
    EXPECT_TRUE(validateTexture(textures[3], false));

    EXPECT_TRUE(textures[0]->haveBackingTexture());
    EXPECT_TRUE(textures[1]->haveBackingTexture());
    EXPECT_TRUE(textures[2]->haveBackingTexture());
    EXPECT_TRUE(textures[3]->haveBackingTexture());

    for (size_t i = 0; i < numTextures; ++i)
        moreTextures[i]->setRequestPriority(100 + i);
    textureManager->prioritizeTextures();

    // Textures are now below cutoff.
    EXPECT_FALSE(validateTexture(textures[0], false));
    EXPECT_FALSE(validateTexture(textures[1], false));
    EXPECT_FALSE(validateTexture(textures[2], false));
    EXPECT_FALSE(validateTexture(textures[3], false));

    // But they are still valid to use.
    EXPECT_TRUE(textures[0]->haveBackingTexture());
    EXPECT_TRUE(textures[1]->haveBackingTexture());
    EXPECT_TRUE(textures[2]->haveBackingTexture());
    EXPECT_TRUE(textures[3]->haveBackingTexture());

    // Higher priority textures are finally needed.
    EXPECT_TRUE(validateTexture(moreTextures[0], false));
    EXPECT_TRUE(validateTexture(moreTextures[1], false));
    EXPECT_TRUE(validateTexture(moreTextures[2], false));
    EXPECT_TRUE(validateTexture(moreTextures[3], false));

    // Lower priority have been fully evicted.
    EXPECT_FALSE(textures[0]->haveBackingTexture());
    EXPECT_FALSE(textures[1]->haveBackingTexture());
    EXPECT_FALSE(textures[2]->haveBackingTexture());
    EXPECT_FALSE(textures[3]->haveBackingTexture());

    DebugScopedSetImplThread implThread;
    textureManager->clearAllMemory(resourceProvider());
}

TEST_F(CCPrioritizedTextureTest, textureManagerPrioritiesAreEqual)
{
    const size_t maxTextures = 16;
    OwnPtr<CCPrioritizedTextureManager> textureManager = createManager(maxTextures);
    OwnPtr<CCPrioritizedTexture> textures[maxTextures];

    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = textureManager->createTexture(m_textureSize, m_textureFormat);

    // All 16 textures have the same priority except 2 higher priority.
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i]->setRequestPriority(100);
    textures[0]->setRequestPriority(99);
    textures[1]->setRequestPriority(99);

    // Set max limit to 8 textures
    textureManager->setMaxMemoryLimitBytes(texturesMemorySize(8));
    textureManager->prioritizeTextures();

    // The two high priority textures should be available, others should not.
    for (size_t i = 0; i < 2; ++i)
        EXPECT_TRUE(validateTexture(textures[i], false));
    for (size_t i = 2; i < maxTextures; ++i)
        EXPECT_FALSE(validateTexture(textures[i], false));
    EXPECT_EQ(texturesMemorySize(2), textureManager->memoryAboveCutoffBytes());
    EXPECT_LE(textureManager->memoryUseBytes(), textureManager->memoryAboveCutoffBytes());

    // Manually reserving textures should only succeed on the higher priority textures,
    // and on remaining textures up to the memory limit.
    for (size_t i = 0; i < 8; i++)
        EXPECT_TRUE(validateTexture(textures[i], true));
    for (size_t i = 9; i < maxTextures; i++)
        EXPECT_FALSE(validateTexture(textures[i], true));
    EXPECT_EQ(texturesMemorySize(8), textureManager->memoryAboveCutoffBytes());
    EXPECT_LE(textureManager->memoryUseBytes(), textureManager->memoryAboveCutoffBytes());

    DebugScopedSetImplThread implThread;
    textureManager->clearAllMemory(resourceProvider());
}

TEST_F(CCPrioritizedTextureTest, textureManagerDestroyedFirst)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = createManager(1);
    OwnPtr<CCPrioritizedTexture> texture = textureManager->createTexture(m_textureSize, m_textureFormat);

    // Texture is initially invalid, but it will become available.
    EXPECT_FALSE(texture->haveBackingTexture());

    texture->setRequestPriority(100);
    textureManager->prioritizeTextures();

    EXPECT_TRUE(validateTexture(texture, false));
    EXPECT_TRUE(texture->canAcquireBackingTexture());
    EXPECT_TRUE(texture->haveBackingTexture());

    {
        DebugScopedSetImplThread implThread;
        textureManager->clearAllMemory(resourceProvider());
    }
    textureManager.clear();

    EXPECT_FALSE(texture->canAcquireBackingTexture());
    EXPECT_FALSE(texture->haveBackingTexture());
}

TEST_F(CCPrioritizedTextureTest, textureMovedToNewManager)
{
    OwnPtr<CCPrioritizedTextureManager> textureManagerOne = createManager(1);
    OwnPtr<CCPrioritizedTextureManager> textureManagerTwo = createManager(1);
    OwnPtr<CCPrioritizedTexture> texture = textureManagerOne->createTexture(m_textureSize, m_textureFormat);

    // Texture is initially invalid, but it will become available.
    EXPECT_FALSE(texture->haveBackingTexture());

    texture->setRequestPriority(100);
    textureManagerOne->prioritizeTextures();

    EXPECT_TRUE(validateTexture(texture, false));
    EXPECT_TRUE(texture->canAcquireBackingTexture());
    EXPECT_TRUE(texture->haveBackingTexture());

    texture->setTextureManager(0);

    {
        DebugScopedSetImplThread implThread;
        textureManagerOne->clearAllMemory(resourceProvider());
    }
    textureManagerOne.clear();

    EXPECT_FALSE(texture->canAcquireBackingTexture());
    EXPECT_FALSE(texture->haveBackingTexture());

    texture->setTextureManager(textureManagerTwo.get());

    textureManagerTwo->prioritizeTextures();

    EXPECT_TRUE(validateTexture(texture, false));
    EXPECT_TRUE(texture->canAcquireBackingTexture());
    EXPECT_TRUE(texture->haveBackingTexture());

    DebugScopedSetImplThread implThread;
    textureManagerTwo->clearAllMemory(resourceProvider());
}

TEST_F(CCPrioritizedTextureTest, renderSurfacesReduceMemoryAvailableOutsideRootSurface)
{
    const size_t maxTextures = 8;
    OwnPtr<CCPrioritizedTextureManager> textureManager = createManager(maxTextures);

    // Half of the memory is taken by surfaces (with high priority place-holder)
    OwnPtr<CCPrioritizedTexture> renderSurfacePlaceHolder = textureManager->createTexture(m_textureSize, m_textureFormat);
    renderSurfacePlaceHolder->setToSelfManagedMemoryPlaceholder(texturesMemorySize(4));
    renderSurfacePlaceHolder->setRequestPriority(CCPriorityCalculator::renderSurfacePriority());

    // Create textures to fill our memory limit.
    OwnPtr<CCPrioritizedTexture> textures[maxTextures];

    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = textureManager->createTexture(m_textureSize, m_textureFormat);

    // Set decreasing non-visible priorities outside root surface.
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i]->setRequestPriority(100 + i);

    // Only lower half should be available.
    textureManager->prioritizeTextures();
    EXPECT_TRUE(validateTexture(textures[0], false));
    EXPECT_TRUE(validateTexture(textures[3], false));
    EXPECT_FALSE(validateTexture(textures[4], false));
    EXPECT_FALSE(validateTexture(textures[7], false));

    // Set increasing non-visible priorities outside root surface.
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i]->setRequestPriority(100 - i);

    // Only upper half should be available.
    textureManager->prioritizeTextures();
    EXPECT_FALSE(validateTexture(textures[0], false));
    EXPECT_FALSE(validateTexture(textures[3], false));
    EXPECT_TRUE(validateTexture(textures[4], false));
    EXPECT_TRUE(validateTexture(textures[7], false));

    EXPECT_EQ(texturesMemorySize(4), textureManager->memoryAboveCutoffBytes());
    EXPECT_EQ(texturesMemorySize(4), textureManager->memoryForSelfManagedTextures());
    EXPECT_LE(textureManager->memoryUseBytes(), textureManager->memoryAboveCutoffBytes());

    DebugScopedSetImplThread implThread;
    textureManager->clearAllMemory(resourceProvider());
}

TEST_F(CCPrioritizedTextureTest, renderSurfacesReduceMemoryAvailableForRequestLate)
{
    const size_t maxTextures = 8;
    OwnPtr<CCPrioritizedTextureManager> textureManager = createManager(maxTextures);

    // Half of the memory is taken by surfaces (with high priority place-holder)
    OwnPtr<CCPrioritizedTexture> renderSurfacePlaceHolder = textureManager->createTexture(m_textureSize, m_textureFormat);
    renderSurfacePlaceHolder->setToSelfManagedMemoryPlaceholder(texturesMemorySize(4));
    renderSurfacePlaceHolder->setRequestPriority(CCPriorityCalculator::renderSurfacePriority());

    // Create textures to fill our memory limit.
    OwnPtr<CCPrioritizedTexture> textures[maxTextures];

    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = textureManager->createTexture(m_textureSize, m_textureFormat);

    // Set equal priorities.
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i]->setRequestPriority(100);

    // The first four to be requested late will be available.
    textureManager->prioritizeTextures();
    for (unsigned i = 0; i < maxTextures; ++i)
        EXPECT_FALSE(validateTexture(textures[i], false));
    for (unsigned i = 0; i < maxTextures; i += 2)
        EXPECT_TRUE(validateTexture(textures[i], true));
    for (unsigned i = 1; i < maxTextures; i += 2)
        EXPECT_FALSE(validateTexture(textures[i], true));

    EXPECT_EQ(texturesMemorySize(4), textureManager->memoryAboveCutoffBytes());
    EXPECT_EQ(texturesMemorySize(4), textureManager->memoryForSelfManagedTextures());
    EXPECT_LE(textureManager->memoryUseBytes(), textureManager->memoryAboveCutoffBytes());

    DebugScopedSetImplThread implThread;
    textureManager->clearAllMemory(resourceProvider());
}

TEST_F(CCPrioritizedTextureTest, whenRenderSurfaceNotAvailableTexturesAlsoNotAvailable)
{
    const size_t maxTextures = 8;
    OwnPtr<CCPrioritizedTextureManager> textureManager = createManager(maxTextures);

    // Half of the memory is taken by surfaces (with high priority place-holder)
    OwnPtr<CCPrioritizedTexture> renderSurfacePlaceHolder = textureManager->createTexture(m_textureSize, m_textureFormat);
    renderSurfacePlaceHolder->setToSelfManagedMemoryPlaceholder(texturesMemorySize(4));
    renderSurfacePlaceHolder->setRequestPriority(CCPriorityCalculator::renderSurfacePriority());

    // Create textures to fill our memory limit.
    OwnPtr<CCPrioritizedTexture> textures[maxTextures];

    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = textureManager->createTexture(m_textureSize, m_textureFormat);

    // Set 6 visible textures in the root surface, and 2 in a child surface.
    for (size_t i = 0; i < 6; ++i)
        textures[i]->setRequestPriority(CCPriorityCalculator::visiblePriority(true));
    for (size_t i = 6; i < 8; ++i)
        textures[i]->setRequestPriority(CCPriorityCalculator::visiblePriority(false));

    textureManager->prioritizeTextures();

    // Unable to requestLate textures in the child surface.
    EXPECT_FALSE(validateTexture(textures[6], true));
    EXPECT_FALSE(validateTexture(textures[7], true));

    // Root surface textures are valid.
    for (size_t i = 0; i < 6; ++i)
        EXPECT_TRUE(validateTexture(textures[i], false));

    EXPECT_EQ(texturesMemorySize(6), textureManager->memoryAboveCutoffBytes());
    EXPECT_EQ(texturesMemorySize(2), textureManager->memoryForSelfManagedTextures());
    EXPECT_LE(textureManager->memoryUseBytes(), textureManager->memoryAboveCutoffBytes());

    DebugScopedSetImplThread implThread;
    textureManager->clearAllMemory(resourceProvider());
}

} // namespace
