//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "gtest/gtest.h"

#include "common/unsafe_buffers.h"
#include "libANGLE/Caps.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/QueryGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"

namespace rx
{
namespace
{

void GL_APIENTRY StubBeginQuery(GLenum, GLuint) {}
void GL_APIENTRY StubEndQuery(GLenum) {}
GLenum GL_APIENTRY StubGetError()
{
    return GL_NO_ERROR;
}
void GL_APIENTRY StubGetIntegerv(GLenum pname, GLint *data)
{
    if (pname == GL_VIEWPORT || pname == GL_SCISSOR_BOX)
    {
        ANGLE_UNSAFE_BUFFERS({
            data[0] = 0;
            data[1] = 0;
            data[2] = 0;
            data[3] = 0;
        });
    }
}

// Minimal FunctionsGL that never calls into a real driver. Only the entry
// points touched by StateManagerGL::beginQuery/endQuery are populated.
class TestFunctionsGL : public FunctionsGL
{
  public:
    TestFunctionsGL()
    {
        standard = STANDARD_GL_ES;
        version  = gl::Version(3, 0);
        profile  = 0;

        beginQuery  = StubBeginQuery;
        endQuery    = StubEndQuery;
        getError    = StubGetError;
        getIntegerv = StubGetIntegerv;
    }

  private:
    void *loadProcAddress(const std::string &function) const override { return nullptr; }
};

// QueryGL implementation that records pause/resume calls and can be forced to
// fail resume() so that error-propagation paths in StateManagerGL are covered.
class TestQueryGL : public QueryGL
{
  public:
    explicit TestQueryGL(gl::QueryType type) : QueryGL(type) {}

    angle::Result begin(const gl::Context *) override { return angle::Result::Continue; }
    angle::Result end(const gl::Context *) override { return angle::Result::Continue; }
    angle::Result queryCounter(const gl::Context *) override { return angle::Result::Continue; }
    angle::Result getResult(const gl::Context *, GLint *) override
    {
        return angle::Result::Continue;
    }
    angle::Result getResult(const gl::Context *, GLuint *) override
    {
        return angle::Result::Continue;
    }
    angle::Result getResult(const gl::Context *, GLint64 *) override
    {
        return angle::Result::Continue;
    }
    angle::Result getResult(const gl::Context *, GLuint64 *) override
    {
        return angle::Result::Continue;
    }
    angle::Result isResultAvailable(const gl::Context *, bool *) override
    {
        return angle::Result::Continue;
    }

    angle::Result pause(const gl::Context *) override
    {
        mPauseCount++;
        return angle::Result::Continue;
    }
    angle::Result resume(const gl::Context *) override
    {
        mResumeCount++;
        return mResumeResult;
    }

    void setResumeResult(angle::Result result) { mResumeResult = result; }
    int pauseCount() const { return mPauseCount; }
    int resumeCount() const { return mResumeCount; }

  private:
    angle::Result mResumeResult = angle::Result::Continue;
    int mPauseCount             = 0;
    int mResumeCount            = 0;
};

gl::Caps MakeMinimalCaps()
{
    gl::Caps caps;
    caps.maxDrawBuffers          = 1;
    caps.maxVertexAttributes     = 1;
    caps.maxVertexAttribBindings = 1;
    return caps;
}

class StateManagerGLTest : public ::testing::Test
{
  protected:
    StateManagerGLTest()
        : mCaps(MakeMinimalCaps()), mStateManager(&mFunctions, mCaps, mExtensions, mFeatures)
    {}

    StateManagerGL &stateManager() { return mStateManager; }

  private:
    TestFunctionsGL mFunctions;
    gl::Caps mCaps;
    gl::Extensions mExtensions;
    angle::FeaturesGL mFeatures;
    StateManagerGL mStateManager;
};

// When resume() reports an error for a paused query, StateManagerGL must still
// drop its reference so that a later resumeAllQueries() call does not touch the
// query again after it may have been destroyed.
TEST_F(StateManagerGLTest, ResumeAllQueriesClearsPausedQueryOnError)
{
    TestQueryGL query(gl::QueryType::AnySamples);

    stateManager().beginQuery(gl::QueryType::AnySamples, &query, 1);
    EXPECT_EQ(angle::Result::Continue, stateManager().pauseAllQueries(nullptr));
    EXPECT_EQ(1, query.pauseCount());

    query.setResumeResult(angle::Result::Stop);
    EXPECT_EQ(angle::Result::Stop, stateManager().resumeAllQueries(nullptr));
    EXPECT_EQ(1, query.resumeCount());

    // The query is no longer tracked; a subsequent resume must not reach it.
    query.setResumeResult(angle::Result::Continue);
    EXPECT_EQ(angle::Result::Continue, stateManager().resumeAllQueries(nullptr));
    EXPECT_EQ(1, query.resumeCount());
}

// If one paused query fails to resume, StateManagerGL must still drop its
// references to every other paused query rather than leaving them behind.
TEST_F(StateManagerGLTest, ResumeAllQueriesClearsAllPausedQueriesOnError)
{
    TestQueryGL failingQuery(gl::QueryType::AnySamples);
    TestQueryGL otherQuery(gl::QueryType::TransformFeedbackPrimitivesWritten);

    stateManager().beginQuery(failingQuery.getType(), &failingQuery, 1);
    stateManager().beginQuery(otherQuery.getType(), &otherQuery, 2);
    EXPECT_EQ(angle::Result::Continue, stateManager().pauseAllQueries(nullptr));

    failingQuery.setResumeResult(angle::Result::Stop);
    EXPECT_EQ(angle::Result::Stop, stateManager().resumeAllQueries(nullptr));

    // Neither query should be reached by a subsequent resume.
    int failingResumeCount = failingQuery.resumeCount();
    int otherResumeCount   = otherQuery.resumeCount();
    EXPECT_EQ(angle::Result::Continue, stateManager().resumeAllQueries(nullptr));
    EXPECT_EQ(failingResumeCount, failingQuery.resumeCount());
    EXPECT_EQ(otherResumeCount, otherQuery.resumeCount());
}

// resumeQuery() must clear its paused-query reference even when resume() fails.
TEST_F(StateManagerGLTest, ResumeQueryClearsPausedQueryOnError)
{
    TestQueryGL query(gl::QueryType::AnySamples);

    stateManager().beginQuery(gl::QueryType::AnySamples, &query, 1);
    EXPECT_EQ(angle::Result::Continue,
              stateManager().pauseQuery(nullptr, gl::QueryType::AnySamples));

    query.setResumeResult(angle::Result::Stop);
    EXPECT_EQ(angle::Result::Stop, stateManager().resumeQuery(nullptr, gl::QueryType::AnySamples));
    EXPECT_EQ(1, query.resumeCount());

    query.setResumeResult(angle::Result::Continue);
    EXPECT_EQ(angle::Result::Continue,
              stateManager().resumeQuery(nullptr, gl::QueryType::AnySamples));
    EXPECT_EQ(1, query.resumeCount());
}

}  // namespace
}  // namespace rx
