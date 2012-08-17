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

#include "CCTextureUpdateController.h"

#include "CCSchedulerTestCommon.h"
#include "CCSingleThreadProxy.h" // For DebugScopedSetImplThread
#include "CCTiledLayerTestCommon.h"
#include "FakeWebCompositorOutputSurface.h"
#include "FakeWebGraphicsContext3D.h"
#include <gtest/gtest.h>
#include <public/WebCompositor.h>
#include <public/WebThread.h>
#include <wtf/RefPtr.h>

using namespace WebCore;
using namespace WebKit;
using namespace WebKitTests;
using testing::Test;


namespace {

const int kFlushPeriodFull = 4;
const int kFlushPeriodPartial = kFlushPeriodFull;

class CCTextureUpdateControllerTest;

class WebGraphicsContext3DForUploadTest : public FakeWebGraphicsContext3D {
public:
    WebGraphicsContext3DForUploadTest(CCTextureUpdateControllerTest *test)
        : m_test(test)
        , m_supportShallowFlush(true)
    { }

    virtual void flush(void);
    virtual void shallowFlushCHROMIUM(void);
    virtual GrGLInterface* onCreateGrGLInterface() { return 0; }

    virtual WebString getString(WGC3Denum name)
    {
        if (m_supportShallowFlush)
            return WebString("GL_CHROMIUM_shallow_flush");
        return WebString("");
    }

private:
    CCTextureUpdateControllerTest* m_test;
    bool m_supportShallowFlush;
};


class TextureUploaderForUploadTest : public FakeTextureUploader {
public:
    TextureUploaderForUploadTest(CCTextureUpdateControllerTest *test) : m_test(test) { }

    virtual void beginUploads() OVERRIDE;
    virtual void endUploads() OVERRIDE;
    virtual void uploadTexture(WebCore::CCResourceProvider*, Parameters) OVERRIDE;

private:
    CCTextureUpdateControllerTest* m_test;
};

class TextureForUploadTest : public LayerTextureUpdater::Texture {
public:
    TextureForUploadTest() : LayerTextureUpdater::Texture(adoptPtr<CCPrioritizedTexture>(0)) { }
    virtual void updateRect(CCResourceProvider*, const IntRect& sourceRect, const IntSize& destOffset) { }
};


class CCTextureUpdateControllerTest : public Test {
public:
    CCTextureUpdateControllerTest()
    : m_queue(adoptPtr(new CCTextureUpdateQueue))
    , m_uploader(this)
    , m_fullUploadCountExpected(0)
    , m_partialCountExpected(0)
    , m_totalUploadCountExpected(0)
    , m_maxUploadCountPerUpdate(0)
    , m_numBeginUploads(0)
    , m_numEndUploads(0)
    , m_numConsecutiveFlushes(0)
    , m_numDanglingUploads(0)
    , m_numTotalUploads(0)
    , m_numTotalFlushes(0)
    , m_numPreviousUploads(0)
    , m_numPreviousFlushes(0)
    { }

public:
    void onFlush()
    {
        // Check for back-to-back flushes.
        EXPECT_EQ(0, m_numConsecutiveFlushes) << "Back-to-back flushes detected.";

        // Check for premature flushes
        if (m_numPreviousUploads != m_maxUploadCountPerUpdate) {
            if (m_numTotalUploads < m_fullUploadCountExpected)
                EXPECT_GE(m_numDanglingUploads, kFlushPeriodFull) << "Premature flush detected in full uploads.";
            else if (m_numTotalUploads > m_fullUploadCountExpected && m_numTotalUploads < m_totalUploadCountExpected)
                EXPECT_GE(m_numDanglingUploads, kFlushPeriodPartial) << "Premature flush detected in partial uploads.";
        }

        m_numDanglingUploads = 0;
        m_numConsecutiveFlushes++;
        m_numTotalFlushes++;
        m_numPreviousFlushes++;
    }

    void onBeginUploads()
    {
        m_numPreviousFlushes = 0;
        m_numPreviousUploads = 0;
        m_numBeginUploads++;
    }

    void onUpload()
    {
        // Check for too many consecutive uploads
        if (m_numTotalUploads < m_fullUploadCountExpected)
            EXPECT_LT(m_numDanglingUploads, kFlushPeriodFull) << "Too many consecutive full uploads detected.";
        else
            EXPECT_LT(m_numDanglingUploads, kFlushPeriodPartial) << "Too many consecutive partial uploads detected.";

        m_numConsecutiveFlushes = 0;
        m_numDanglingUploads++;
        m_numTotalUploads++;
        m_numPreviousUploads++;
    }

    void onEndUploads()
    {
        EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";

        // Note: The m_numTotalUploads != m_fullUploadCountExpected comparison
        // allows for the quota not to be hit in the case where we are trasitioning
        // from full uploads to partial uploads.
        if (m_numTotalUploads != m_totalUploadCountExpected && m_numTotalUploads != m_fullUploadCountExpected) {
            EXPECT_EQ(m_maxUploadCountPerUpdate, m_numPreviousUploads)
                << "endUpload() was called when there are textures to upload, but the upload quota hasn't been filled.";
        }

        m_numEndUploads++;
    }

protected:
    virtual void SetUp()
    {
        OwnPtr<WebThread> thread;
        WebCompositor::initialize(thread.get());

        m_context = FakeWebCompositorOutputSurface::create(adoptPtr(new WebGraphicsContext3DForUploadTest(this)));
        DebugScopedSetImplThread implThread;
        m_resourceProvider = CCResourceProvider::create(m_context.get());
    }

    virtual void TearDown()
    {
        WebCompositor::shutdown();
    }

    void appendFullUploadsToUpdateQueue(int count)
    {
        m_fullUploadCountExpected += count;
        m_totalUploadCountExpected += count;

        const IntRect rect(0, 0, 300, 150);
        const TextureUploader::Parameters upload = { &m_texture, rect, IntSize() };
        for (int i = 0; i < count; i++)
            m_queue->appendFullUpload(upload);
    }

    void appendPartialUploadsToUpdateQueue(int count)
    {
        m_partialCountExpected += count;
        m_totalUploadCountExpected += count;

        const IntRect rect(0, 0, 100, 100);
        const TextureUploader::Parameters upload = { &m_texture, rect, IntSize() };
        for (int i = 0; i < count; i++)
            m_queue->appendPartialUpload(upload);
    }

    void setMaxUploadCountPerUpdate(int count)
    {
        m_maxUploadCountPerUpdate = count;
    }

protected:
    // Classes required to interact and test the CCTextureUpdateController
    OwnPtr<CCGraphicsContext> m_context;
    OwnPtr<CCResourceProvider> m_resourceProvider;
    OwnPtr<CCTextureUpdateQueue> m_queue;
    TextureForUploadTest m_texture;
    FakeTextureCopier m_copier;
    TextureUploaderForUploadTest m_uploader;

    // Properties / expectations of this test
    int m_fullUploadCountExpected;
    int m_partialCountExpected;
    int m_totalUploadCountExpected;
    int m_maxUploadCountPerUpdate;

    // Dynamic properties of this test
    int m_numBeginUploads;
    int m_numEndUploads;
    int m_numConsecutiveFlushes;
    int m_numDanglingUploads;
    int m_numTotalUploads;
    int m_numTotalFlushes;
    int m_numPreviousUploads;
    int m_numPreviousFlushes;
};

void WebGraphicsContext3DForUploadTest::flush(void)
{
    m_test->onFlush();
}

void WebGraphicsContext3DForUploadTest::shallowFlushCHROMIUM(void)
{
    m_test->onFlush();
}

void TextureUploaderForUploadTest::beginUploads()
{
    m_test->onBeginUploads();
}

void TextureUploaderForUploadTest::endUploads()
{
    m_test->onEndUploads();
}

void TextureUploaderForUploadTest::uploadTexture(WebCore::CCResourceProvider*, Parameters)
{
    m_test->onUpload();
}


// ZERO UPLOADS TESTS
TEST_F(CCTextureUpdateControllerTest, ZeroUploads)
{
    appendFullUploadsToUpdateQueue(0);
    appendPartialUploadsToUpdateQueue(0);
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), m_totalUploadCountExpected);

    EXPECT_EQ(0, m_numBeginUploads);
    EXPECT_EQ(0, m_numEndUploads);
    EXPECT_EQ(0, m_numPreviousFlushes);
    EXPECT_EQ(0, m_numPreviousUploads);
}


// ONE UPLOAD TESTS
TEST_F(CCTextureUpdateControllerTest, OneFullUpload)
{
    appendFullUploadsToUpdateQueue(1);
    appendPartialUploadsToUpdateQueue(0);
    DebugScopedSetImplThread implThread;
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), m_totalUploadCountExpected);

    EXPECT_EQ(1, m_numBeginUploads);
    EXPECT_EQ(1, m_numEndUploads);
    EXPECT_EQ(1, m_numPreviousFlushes);
    EXPECT_EQ(1, m_numPreviousUploads);
}

TEST_F(CCTextureUpdateControllerTest, OnePartialUpload)
{
    appendFullUploadsToUpdateQueue(0);
    appendPartialUploadsToUpdateQueue(1);
    DebugScopedSetImplThread implThread;
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), m_totalUploadCountExpected);

    EXPECT_EQ(1, m_numBeginUploads);
    EXPECT_EQ(1, m_numEndUploads);
    EXPECT_EQ(1, m_numPreviousFlushes);
    EXPECT_EQ(1, m_numPreviousUploads);
}

TEST_F(CCTextureUpdateControllerTest, OneFullOnePartialUpload)
{
    appendFullUploadsToUpdateQueue(1);
    appendPartialUploadsToUpdateQueue(1);
    DebugScopedSetImplThread implThread;
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), m_totalUploadCountExpected);

    // We expect the full uploads to be followed by a flush
    // before the partial uploads begin.
    EXPECT_EQ(1, m_numBeginUploads);
    EXPECT_EQ(1, m_numEndUploads);
    EXPECT_EQ(2, m_numPreviousFlushes);
    EXPECT_EQ(2, m_numPreviousUploads);
}


// NO REMAINDER TESTS
// This class of tests upload a number of textures that is a multiple of the flush period.
const int fullUploadFlushMultipler = 7;
const int fullNoRemainderCount = fullUploadFlushMultipler * kFlushPeriodFull;

const int partialUploadFlushMultipler = 11;
const int partialNoRemainderCount = partialUploadFlushMultipler * kFlushPeriodPartial;

TEST_F(CCTextureUpdateControllerTest, ManyFullUploadsNoRemainder)
{
    appendFullUploadsToUpdateQueue(fullNoRemainderCount);
    appendPartialUploadsToUpdateQueue(0);
    DebugScopedSetImplThread implThread;
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), m_totalUploadCountExpected);

    EXPECT_EQ(1, m_numBeginUploads);
    EXPECT_EQ(1, m_numEndUploads);
    EXPECT_EQ(fullUploadFlushMultipler, m_numPreviousFlushes);
    EXPECT_EQ(fullNoRemainderCount, m_numPreviousUploads);
}

TEST_F(CCTextureUpdateControllerTest, ManyPartialUploadsNoRemainder)
{
    appendFullUploadsToUpdateQueue(0);
    appendPartialUploadsToUpdateQueue(partialNoRemainderCount);
    DebugScopedSetImplThread implThread;
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), m_totalUploadCountExpected);

    EXPECT_EQ(1, m_numBeginUploads);
    EXPECT_EQ(1, m_numEndUploads);
    EXPECT_EQ(partialUploadFlushMultipler, m_numPreviousFlushes);
    EXPECT_EQ(partialNoRemainderCount, m_numPreviousUploads);
}

TEST_F(CCTextureUpdateControllerTest, ManyFullManyPartialUploadsNoRemainder)
{
    appendFullUploadsToUpdateQueue(fullNoRemainderCount);
    appendPartialUploadsToUpdateQueue(partialNoRemainderCount);
    DebugScopedSetImplThread implThread;
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), m_totalUploadCountExpected);

    EXPECT_EQ(1, m_numBeginUploads);
    EXPECT_EQ(1, m_numEndUploads);
    EXPECT_EQ(fullUploadFlushMultipler + partialUploadFlushMultipler, m_numPreviousFlushes);
    EXPECT_EQ(fullNoRemainderCount + partialNoRemainderCount, m_numPreviousUploads);
}


// MIN/MAX REMAINDER TESTS
// This class of tests mix and match uploading 1 more and 1 less texture
// than a multiple of the flush period.

const int fullMinRemainderCount = fullNoRemainderCount + 1;
const int fullMaxRemainderCount = fullNoRemainderCount - 1;
const int partialMinRemainderCount = partialNoRemainderCount + 1;
const int partialMaxRemainderCount = partialNoRemainderCount - 1;

TEST_F(CCTextureUpdateControllerTest, ManyFullAndPartialMinRemainder)
{
    appendFullUploadsToUpdateQueue(fullMinRemainderCount);
    appendPartialUploadsToUpdateQueue(partialMinRemainderCount);
    DebugScopedSetImplThread implThread;
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), m_totalUploadCountExpected);

    EXPECT_EQ(1, m_numBeginUploads);
    EXPECT_EQ(1, m_numEndUploads);
    EXPECT_EQ(fullUploadFlushMultipler + partialUploadFlushMultipler + 2, m_numPreviousFlushes);
    EXPECT_EQ(fullMinRemainderCount + partialMinRemainderCount, m_numPreviousUploads);
}

TEST_F(CCTextureUpdateControllerTest, ManyFullAndPartialUploadsMaxRemainder)
{
    appendFullUploadsToUpdateQueue(fullMaxRemainderCount);
    appendPartialUploadsToUpdateQueue(partialMaxRemainderCount);
    DebugScopedSetImplThread implThread;
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), m_totalUploadCountExpected);

    EXPECT_EQ(1, m_numBeginUploads);
    EXPECT_EQ(1, m_numEndUploads);
    EXPECT_EQ(fullUploadFlushMultipler + partialUploadFlushMultipler, m_numPreviousFlushes);
    EXPECT_EQ(fullMaxRemainderCount + partialMaxRemainderCount, m_numPreviousUploads);
}

TEST_F(CCTextureUpdateControllerTest, ManyFullMinRemainderManyPartialMaxRemainder)
{
    appendFullUploadsToUpdateQueue(fullMinRemainderCount);
    appendPartialUploadsToUpdateQueue(partialMaxRemainderCount);
    DebugScopedSetImplThread implThread;
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), m_totalUploadCountExpected);

    EXPECT_EQ(1, m_numBeginUploads);
    EXPECT_EQ(1, m_numEndUploads);
    EXPECT_EQ((fullUploadFlushMultipler+1) + partialUploadFlushMultipler, m_numPreviousFlushes);
    EXPECT_EQ(fullMinRemainderCount + partialMaxRemainderCount, m_numPreviousUploads);
}

TEST_F(CCTextureUpdateControllerTest, ManyFullMaxRemainderManyPartialMinRemainder)
{
    appendFullUploadsToUpdateQueue(fullMaxRemainderCount);
    appendPartialUploadsToUpdateQueue(partialMinRemainderCount);
    DebugScopedSetImplThread implThread;
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), m_totalUploadCountExpected);

    EXPECT_EQ(1, m_numBeginUploads);
    EXPECT_EQ(1, m_numEndUploads);
    EXPECT_EQ(fullUploadFlushMultipler + (partialUploadFlushMultipler+1), m_numPreviousFlushes);
    EXPECT_EQ(fullMaxRemainderCount + partialMinRemainderCount, m_numPreviousUploads);
}


// MULTIPLE UPDATE TESTS
// These tests attempt to upload too many textures at once, requiring
// multiple calls to update().

int expectedFlushes(int uploads, int flushPeriod)
{
    return (uploads + flushPeriod - 1) / flushPeriod;
}

TEST_F(CCTextureUpdateControllerTest, TripleUpdateFinalUpdateFullAndPartial)
{
    const int kMaxUploadsPerUpdate = 40;
    const int kFullUploads = 100;
    const int kPartialUploads = 20;

    int expectedPreviousFlushes = 0;
    int expectedPreviousUploads = 0;

    setMaxUploadCountPerUpdate(kMaxUploadsPerUpdate);
    appendFullUploadsToUpdateQueue(kFullUploads);
    appendPartialUploadsToUpdateQueue(kPartialUploads);

    // First update (40 full)
    DebugScopedSetImplThread implThread;
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), kMaxUploadsPerUpdate);

    EXPECT_EQ(1, m_numBeginUploads);
    EXPECT_EQ(1, m_numEndUploads);

    expectedPreviousFlushes = expectedFlushes(kMaxUploadsPerUpdate, kFlushPeriodFull);
    EXPECT_EQ(expectedPreviousFlushes, m_numPreviousFlushes);

    expectedPreviousUploads = kMaxUploadsPerUpdate;
    EXPECT_EQ(expectedPreviousUploads, m_numPreviousUploads);

    // Second update (40 full)
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), kMaxUploadsPerUpdate);

    EXPECT_EQ(2, m_numBeginUploads);
    EXPECT_EQ(2, m_numEndUploads);

    expectedPreviousFlushes = expectedFlushes(kMaxUploadsPerUpdate, kFlushPeriodFull);
    EXPECT_EQ(expectedPreviousFlushes, m_numPreviousFlushes);

    expectedPreviousUploads = kMaxUploadsPerUpdate;
    EXPECT_EQ(expectedPreviousUploads, m_numPreviousUploads);

    // Third update (20 full, 20 partial)
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), kMaxUploadsPerUpdate);

    EXPECT_EQ(3, m_numBeginUploads);
    EXPECT_EQ(3, m_numEndUploads);

    expectedPreviousFlushes = expectedFlushes(kFullUploads-kMaxUploadsPerUpdate*2, kFlushPeriodFull) +
                              expectedFlushes(kPartialUploads, kFlushPeriodPartial);
    EXPECT_EQ(expectedPreviousFlushes, m_numPreviousFlushes);

    expectedPreviousUploads = (kFullUploads-kMaxUploadsPerUpdate*2)+kPartialUploads;
    EXPECT_EQ(expectedPreviousUploads, m_numPreviousUploads);

    // Final sanity checks
    EXPECT_EQ(kFullUploads + kPartialUploads, m_numTotalUploads);
}

TEST_F(CCTextureUpdateControllerTest, TripleUpdateFinalUpdateAllPartial)
{
    const int kMaxUploadsPerUpdate = 40;
    const int kFullUploads = 70;
    const int kPartialUploads = 30;

    int expectedPreviousFlushes = 0;
    int expectedPreviousUploads = 0;

    setMaxUploadCountPerUpdate(kMaxUploadsPerUpdate);
    appendFullUploadsToUpdateQueue(kFullUploads);
    appendPartialUploadsToUpdateQueue(kPartialUploads);

    // First update (40 full)
    DebugScopedSetImplThread implThread;
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), kMaxUploadsPerUpdate);

    EXPECT_EQ(1, m_numBeginUploads);
    EXPECT_EQ(1, m_numEndUploads);

    expectedPreviousFlushes = expectedFlushes(kMaxUploadsPerUpdate, kFlushPeriodFull);
    EXPECT_EQ(expectedPreviousFlushes, m_numPreviousFlushes);

    expectedPreviousUploads = kMaxUploadsPerUpdate;
    EXPECT_EQ(expectedPreviousUploads, m_numPreviousUploads);

    // Second update (30 full, optionally 10 partial)
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), kMaxUploadsPerUpdate);

    EXPECT_EQ(2, m_numBeginUploads);
    EXPECT_EQ(2, m_numEndUploads);
    EXPECT_LE(m_numPreviousUploads, kMaxUploadsPerUpdate);
    // Be lenient on the exact number of flushes here, as the number of flushes
    // will depend on whether some partial uploads were performed.
    // onFlush(), onUpload(), and onEndUpload() will do basic flush checks for us anyway.

    // Third update (30 partial OR 20 partial if 10 partial uploaded in second update)
    CCTextureUpdateController::updateTextures(m_resourceProvider.get(), &m_copier, &m_uploader, m_queue.get(), kMaxUploadsPerUpdate);

    EXPECT_EQ(3, m_numBeginUploads);
    EXPECT_EQ(3, m_numEndUploads);
    EXPECT_LE(m_numPreviousUploads, kMaxUploadsPerUpdate);
    // Be lenient on the exact number of flushes here as well.

    // Final sanity checks
    EXPECT_EQ(kFullUploads + kPartialUploads, m_numTotalUploads);
}

} // namespace
