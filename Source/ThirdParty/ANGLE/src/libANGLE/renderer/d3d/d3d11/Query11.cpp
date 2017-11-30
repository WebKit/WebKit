//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query11.cpp: Defines the rx::Query11 class which implements rx::QueryImpl.

#include "libANGLE/renderer/d3d/d3d11/Query11.h"

#include <GLES2/gl2ext.h>

#include "common/utilities.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

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

        case GL_TIME_ELAPSED_EXT:
            return currentResult + newResult;

        case GL_TIMESTAMP_EXT:
            return newResult;

        case GL_COMMANDS_COMPLETED_CHROMIUM:
            return newResult;

        default:
            UNREACHABLE();
            return 0;
    }
}

}  // anonymous namespace

namespace rx
{

Query11::QueryState::QueryState() : query(), beginTimestamp(), endTimestamp(), finished(false)
{
}

Query11::QueryState::~QueryState()
{
}

Query11::Query11(Renderer11 *renderer, GLenum type)
    : QueryImpl(type), mResult(0), mResultSum(0), mRenderer(renderer)
{
    mActiveQuery = std::unique_ptr<QueryState>(new QueryState());
}

Query11::~Query11()
{
    mRenderer->getStateManager()->onDeleteQueryObject(this);
}

gl::Error Query11::begin()
{
    mResultSum = 0;
    mRenderer->getStateManager()->onBeginQuery(this);
    return resume();
}

gl::Error Query11::end()
{
    return pause();
}

gl::Error Query11::queryCounter()
{
    // This doesn't do anything for D3D11 as we don't support timestamps
    ASSERT(getType() == GL_TIMESTAMP_EXT);
    mResultSum = 0;
    mPendingQueries.push_back(std::unique_ptr<QueryState>(new QueryState()));
    return gl::NoError();
}

template <typename T>
gl::Error Query11::getResultBase(T *params)
{
    ASSERT(!mActiveQuery->query.valid());
    ANGLE_TRY(flush(true));
    ASSERT(mPendingQueries.empty());
    *params = static_cast<T>(mResultSum);

    return gl::NoError();
}

gl::Error Query11::getResult(GLint *params)
{
    return getResultBase(params);
}

gl::Error Query11::getResult(GLuint *params)
{
    return getResultBase(params);
}

gl::Error Query11::getResult(GLint64 *params)
{
    return getResultBase(params);
}

gl::Error Query11::getResult(GLuint64 *params)
{
    return getResultBase(params);
}

gl::Error Query11::isResultAvailable(bool *available)
{
    ANGLE_TRY(flush(false));

    *available = mPendingQueries.empty();
    return gl::NoError();
}

gl::Error Query11::pause()
{
    if (mActiveQuery->query.valid())
    {
        ID3D11DeviceContext *context = mRenderer->getDeviceContext();
        GLenum queryType             = getType();

        // If we are doing time elapsed query the end timestamp
        if (queryType == GL_TIME_ELAPSED_EXT)
        {
            context->End(mActiveQuery->endTimestamp.get());
        }

        context->End(mActiveQuery->query.get());

        mPendingQueries.push_back(std::move(mActiveQuery));
        mActiveQuery = std::unique_ptr<QueryState>(new QueryState());
    }

    return flush(false);
}

gl::Error Query11::resume()
{
    if (!mActiveQuery->query.valid())
    {
        ANGLE_TRY(flush(false));

        GLenum queryType         = getType();
        D3D11_QUERY d3dQueryType = gl_d3d11::ConvertQueryType(queryType);

        D3D11_QUERY_DESC queryDesc;
        queryDesc.Query     = d3dQueryType;
        queryDesc.MiscFlags = 0;

        ANGLE_TRY(mRenderer->allocateResource(queryDesc, &mActiveQuery->query));

        // If we are doing time elapsed we also need a query to actually query the timestamp
        if (queryType == GL_TIME_ELAPSED_EXT)
        {
            D3D11_QUERY_DESC desc;
            desc.Query     = D3D11_QUERY_TIMESTAMP;
            desc.MiscFlags = 0;

            ANGLE_TRY(mRenderer->allocateResource(desc, &mActiveQuery->beginTimestamp));
            ANGLE_TRY(mRenderer->allocateResource(desc, &mActiveQuery->endTimestamp));
        }

        ID3D11DeviceContext *context = mRenderer->getDeviceContext();

        if (d3dQueryType != D3D11_QUERY_EVENT)
        {
            context->Begin(mActiveQuery->query.get());
        }

        // If we are doing time elapsed, query the begin timestamp
        if (queryType == GL_TIME_ELAPSED_EXT)
        {
            context->End(mActiveQuery->beginTimestamp.get());
        }
    }

    return gl::NoError();
}

gl::Error Query11::flush(bool force)
{
    while (!mPendingQueries.empty())
    {
        QueryState *query = mPendingQueries.front().get();

        do
        {
            ANGLE_TRY(testQuery(query));
            if (!query->finished && !force)
            {
                return gl::NoError();
            }
        } while (!query->finished);

        mResultSum = MergeQueryResults(getType(), mResultSum, mResult);
        mPendingQueries.pop_front();
    }

    return gl::NoError();
}

gl::Error Query11::testQuery(QueryState *queryState)
{
    if (!queryState->finished)
    {
        ID3D11DeviceContext *context = mRenderer->getDeviceContext();
        switch (getType())
        {
            case GL_ANY_SAMPLES_PASSED_EXT:
            case GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT:
            {
                ASSERT(queryState->query.valid());
                UINT64 numPixels = 0;
                HRESULT result =
                    context->GetData(queryState->query.get(), &numPixels, sizeof(numPixels), 0);
                if (FAILED(result))
                {
                    return gl::OutOfMemory()
                           << "Failed to get the data of an internal query, " << gl::FmtHR(result);
                }

                if (result == S_OK)
                {
                    queryState->finished = true;
                    mResult              = (numPixels > 0) ? GL_TRUE : GL_FALSE;
                }
            }
            break;

            case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
            {
                ASSERT(queryState->query.valid());
                D3D11_QUERY_DATA_SO_STATISTICS soStats = {0};
                HRESULT result =
                    context->GetData(queryState->query.get(), &soStats, sizeof(soStats), 0);
                if (FAILED(result))
                {
                    return gl::OutOfMemory()
                           << "Failed to get the data of an internal query, " << gl::FmtHR(result);
                }

                if (result == S_OK)
                {
                    queryState->finished = true;
                    mResult              = static_cast<GLuint64>(soStats.NumPrimitivesWritten);
                }
            }
            break;

            case GL_TIME_ELAPSED_EXT:
            {
                ASSERT(queryState->query.valid());
                ASSERT(queryState->beginTimestamp.valid());
                ASSERT(queryState->endTimestamp.valid());
                D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timeStats = {0};
                HRESULT result =
                    context->GetData(queryState->query.get(), &timeStats, sizeof(timeStats), 0);
                if (FAILED(result))
                {
                    return gl::OutOfMemory()
                           << "Failed to get the data of an internal query, " << gl::FmtHR(result);
                }

                if (result == S_OK)
                {
                    UINT64 beginTime = 0;
                    HRESULT beginRes = context->GetData(queryState->beginTimestamp.get(),
                                                        &beginTime, sizeof(UINT64), 0);
                    if (FAILED(beginRes))
                    {
                        return gl::OutOfMemory() << "Failed to get the data of an internal query, "
                                                 << gl::FmtHR(beginRes);
                    }
                    UINT64 endTime = 0;
                    HRESULT endRes = context->GetData(queryState->endTimestamp.get(), &endTime,
                                                      sizeof(UINT64), 0);
                    if (FAILED(endRes))
                    {
                        return gl::OutOfMemory() << "Failed to get the data of an internal query, "
                                                 << gl::FmtHR(endRes);
                    }

                    if (beginRes == S_OK && endRes == S_OK)
                    {
                        queryState->finished = true;
                        if (timeStats.Disjoint)
                        {
                            mRenderer->setGPUDisjoint();
                        }
                        static_assert(sizeof(UINT64) == sizeof(unsigned long long),
                                      "D3D UINT64 isn't 64 bits");

                        angle::CheckedNumeric<UINT64> checkedTime(endTime);
                        checkedTime -= beginTime;
                        checkedTime *= 1000000000ull;
                        checkedTime /= timeStats.Frequency;
                        if (checkedTime.IsValid())
                        {
                            mResult = checkedTime.ValueOrDie();
                        }
                        else
                        {
                            mResult = std::numeric_limits<GLuint64>::max() / timeStats.Frequency;
                            // If an overflow does somehow occur, there is no way the elapsed time
                            // is accurate, so we generate a disjoint event
                            mRenderer->setGPUDisjoint();
                        }
                    }
                }
            }
            break;

            case GL_TIMESTAMP_EXT:
            {
                // D3D11 doesn't support GL timestamp queries as D3D timestamps are not guaranteed
                // to have any sort of continuity outside of a disjoint timestamp query block, which
                // GL depends on
                ASSERT(!queryState->query.valid());
                mResult              = 0;
                queryState->finished = true;
            }
            break;

            case GL_COMMANDS_COMPLETED_CHROMIUM:
            {
                ASSERT(queryState->query.valid());
                BOOL completed = 0;
                HRESULT result =
                    context->GetData(queryState->query.get(), &completed, sizeof(completed), 0);
                if (FAILED(result))
                {
                    return gl::OutOfMemory()
                           << "Failed to get the data of an internal query, " << gl::FmtHR(result);
                }

                if (result == S_OK)
                {
                    queryState->finished = true;
                    ASSERT(completed == TRUE);
                    mResult = (completed == TRUE) ? GL_TRUE : GL_FALSE;
                }
            }
            break;

            default:
                UNREACHABLE();
                break;
        }

        if (!queryState->finished && mRenderer->testDeviceLost())
        {
            mRenderer->notifyDeviceLost();
            return gl::OutOfMemory() << "Failed to test get query result, device is lost.";
        }
    }

    return gl::NoError();
}

}  // namespace rx
