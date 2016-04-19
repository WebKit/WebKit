//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Caps.h"
#include "libANGLE/TransformFeedback.h"
#include "libANGLE/renderer/BufferImpl_mock.h"
#include "libANGLE/renderer/TransformFeedbackImpl_mock.h"
#include "tests/angle_unittests_utils.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace
{

class MockFactory : public rx::NullFactory
{
  public:
    MOCK_METHOD0(createTransformFeedback, rx::TransformFeedbackImpl *());
};

class TransformFeedbackTest : public testing::Test
{
  protected:
    TransformFeedbackTest() : mImpl(nullptr), mFeedback(nullptr) {}

    void SetUp() override
    {
        mImpl = new rx::MockTransformFeedbackImpl;
        EXPECT_CALL(mMockFactory, createTransformFeedback())
            .WillOnce(Return(mImpl))
            .RetiresOnSaturation();

        // Set a reasonable number of tf attributes
        mCaps.maxTransformFeedbackSeparateAttributes = 8;

        EXPECT_CALL(*mImpl, destructor());
        mFeedback = new gl::TransformFeedback(&mMockFactory, 1, mCaps);
        mFeedback->addRef();
    }

    void TearDown() override
    {
        if (mFeedback)
        {
            mFeedback->release();
        }

        // Only needed because the mock is leaked if bugs are present,
        // which logs an error, but does not cause the test to fail.
        // Ordinarily mocks are verified when destroyed.
        testing::Mock::VerifyAndClear(mImpl);
    }

    MockFactory mMockFactory;
    rx::MockTransformFeedbackImpl* mImpl;
    gl::TransformFeedback* mFeedback;
    gl::Caps mCaps;
};

TEST_F(TransformFeedbackTest, SideEffectsOfStartAndStop)
{
    testing::InSequence seq;

    EXPECT_FALSE(mFeedback->isActive());
    EXPECT_CALL(*mImpl, begin(GL_TRIANGLES));
    mFeedback->begin(GL_TRIANGLES, nullptr);
    EXPECT_TRUE(mFeedback->isActive());
    EXPECT_EQ(static_cast<GLenum>(GL_TRIANGLES), mFeedback->getPrimitiveMode());
    EXPECT_CALL(*mImpl, end());
    mFeedback->end();
    EXPECT_FALSE(mFeedback->isActive());
}

TEST_F(TransformFeedbackTest, SideEffectsOfPauseAndResume)
{
    testing::InSequence seq;

    EXPECT_FALSE(mFeedback->isActive());
    EXPECT_CALL(*mImpl, begin(GL_TRIANGLES));
    mFeedback->begin(GL_TRIANGLES, nullptr);
    EXPECT_FALSE(mFeedback->isPaused());
    EXPECT_CALL(*mImpl, pause());
    mFeedback->pause();
    EXPECT_TRUE(mFeedback->isPaused());
    EXPECT_CALL(*mImpl, resume());
    mFeedback->resume();
    EXPECT_FALSE(mFeedback->isPaused());
    EXPECT_CALL(*mImpl, end());
    mFeedback->end();
}

TEST_F(TransformFeedbackTest, BufferBinding)
{
    rx::MockBufferImpl *bufferImpl = new rx::MockBufferImpl;
    gl::Buffer *buffer = new gl::Buffer(bufferImpl, 1);
    EXPECT_CALL(*bufferImpl, destructor()).Times(1).RetiresOnSaturation();

    static const size_t bindIndex = 0;

    EXPECT_EQ(mFeedback->getIndexedBufferCount(), mCaps.maxTransformFeedbackSeparateAttributes);

    EXPECT_CALL(*mImpl, bindGenericBuffer(_));
    mFeedback->bindGenericBuffer(buffer);
    EXPECT_EQ(mFeedback->getGenericBuffer().get(), buffer);

    EXPECT_CALL(*mImpl, bindIndexedBuffer(_, _));
    mFeedback->bindIndexedBuffer(bindIndex, buffer, 0, 1);
    for (size_t i = 0; i < mFeedback->getIndexedBufferCount(); i++)
    {
        if (i == bindIndex)
        {
            EXPECT_EQ(mFeedback->getIndexedBuffer(i).get(), buffer);
        }
        else
        {
            EXPECT_EQ(mFeedback->getIndexedBuffer(i).get(), nullptr);
        }
    }

    // force-release the feedback object to ensure the buffer is released.
    const size_t releaseCount = mFeedback->getRefCount();
    for (size_t count = 0; count < releaseCount; ++count)
    {
        mFeedback->release();
    }

    mFeedback = nullptr;

    testing::Mock::VerifyAndClear(bufferImpl);
}

}  // anonymous namespace
