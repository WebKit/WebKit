//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// QueryGL.cpp: Implements the class methods for QueryGL.

#include "libANGLE/renderer/gl/QueryGL.h"

#include "common/debug.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"

namespace
{

GLuint64 MergeQueryResults(GLenum type, GLuint64 currentResult, GLuint64 newResult)
{
    switch (type)
    {
        case GL_ANY_SAMPLES_PASSED:
        case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
            return (currentResult == GL_TRUE || newResult == GL_TRUE) ? GL_TRUE : GL_FALSE;

        case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
            return currentResult + newResult;

        case GL_TIME_ELAPSED:
            return currentResult + newResult;

        case GL_TIMESTAMP:
            return newResult;

        default:
            UNREACHABLE();
            return 0;
    }
}

}  // anonymous namespace

namespace rx
{

QueryGL::QueryGL(GLenum type, const FunctionsGL *functions, StateManagerGL *stateManager)
    : QueryImpl(type),
      mType(type),
      mFunctions(functions),
      mStateManager(stateManager),
      mActiveQuery(0),
      mPendingQueries(),
      mResultSum(0)
{
}

QueryGL::~QueryGL()
{
    mStateManager->deleteQuery(mActiveQuery);
    mStateManager->onDeleteQueryObject(this);
    while (!mPendingQueries.empty())
    {
        mStateManager->deleteQuery(mPendingQueries.front());
        mPendingQueries.pop_front();
    }
}

gl::Error QueryGL::begin()
{
    mResultSum = 0;
    mStateManager->onBeginQuery(this);
    return resume();
}

gl::Error QueryGL::end()
{
    return pause();
}

gl::Error QueryGL::queryCounter()
{
    ASSERT(mType == GL_TIMESTAMP);

    // Directly create a query for the timestamp and add it to the pending query queue, as timestamp
    // queries do not have the traditional begin/end block and never need to be paused/resumed
    GLuint query;
    mFunctions->genQueries(1, &query);
    mFunctions->queryCounter(query, GL_TIMESTAMP);
    mPendingQueries.push_back(query);

    return gl::Error(GL_NO_ERROR);
}

template <typename T>
gl::Error QueryGL::getResultBase(T *params)
{
    ASSERT(mActiveQuery == 0);

    gl::Error error = flush(true);
    if (error.isError())
    {
        return error;
    }

    ASSERT(mPendingQueries.empty());
    *params = static_cast<T>(mResultSum);

    return gl::Error(GL_NO_ERROR);
}

gl::Error QueryGL::getResult(GLint *params)
{
    return getResultBase(params);
}

gl::Error QueryGL::getResult(GLuint *params)
{
    return getResultBase(params);
}

gl::Error QueryGL::getResult(GLint64 *params)
{
    return getResultBase(params);
}

gl::Error QueryGL::getResult(GLuint64 *params)
{
    return getResultBase(params);
}

gl::Error QueryGL::isResultAvailable(bool *available)
{
    ASSERT(mActiveQuery == 0);

    gl::Error error = flush(false);
    if (error.isError())
    {
        return error;
    }

    *available = mPendingQueries.empty();
    return gl::Error(GL_NO_ERROR);
}

gl::Error QueryGL::pause()
{
    if (mActiveQuery != 0)
    {
        mStateManager->endQuery(mType, mActiveQuery);

        mPendingQueries.push_back(mActiveQuery);
        mActiveQuery = 0;
    }

    // Flush to make sure the pending queries don't add up too much.
    gl::Error error = flush(false);
    if (error.isError())
    {
        return error;
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error QueryGL::resume()
{
    if (mActiveQuery == 0)
    {
        // Flush to make sure the pending queries don't add up too much.
        gl::Error error = flush(false);
        if (error.isError())
        {
            return error;
        }

        mFunctions->genQueries(1, &mActiveQuery);
        mStateManager->beginQuery(mType, mActiveQuery);
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error QueryGL::flush(bool force)
{
    while (!mPendingQueries.empty())
    {
        GLuint id = mPendingQueries.front();
        if (!force)
        {
            GLuint resultAvailable = 0;
            mFunctions->getQueryObjectuiv(id, GL_QUERY_RESULT_AVAILABLE, &resultAvailable);
            if (resultAvailable == GL_FALSE)
            {
                return gl::Error(GL_NO_ERROR);
            }
        }

        // Even though getQueryObjectui64v was introduced for timer queries, there is nothing in the
        // standard that says that it doesn't work for any other queries. It also passes on all the
        // trybots, so we use it if it is available
        if (mFunctions->getQueryObjectui64v != nullptr)
        {
            GLuint64 result = 0;
            mFunctions->getQueryObjectui64v(id, GL_QUERY_RESULT, &result);
            mResultSum = MergeQueryResults(mType, mResultSum, result);
        }
        else
        {
            GLuint result = 0;
            mFunctions->getQueryObjectuiv(id, GL_QUERY_RESULT, &result);
            mResultSum = MergeQueryResults(mType, mResultSum, static_cast<GLuint64>(result));
        }

        mStateManager->deleteQuery(id);

        mPendingQueries.pop_front();
    }

    return gl::Error(GL_NO_ERROR);
}

}
