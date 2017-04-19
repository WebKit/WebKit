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

Query11::QueryState::QueryState()
    : query(nullptr), beginTimestamp(nullptr), endTimestamp(nullptr), finished(false)
{
}

Query11::QueryState::~QueryState()
{
    SafeRelease(beginTimestamp);
    SafeRelease(endTimestamp);
    SafeRelease(query);
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
    ASSERT(mActiveQuery->query == nullptr);
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
    if (mActiveQuery->query != nullptr)
    {
        ID3D11DeviceContext *context = mRenderer->getDeviceContext();
        GLenum queryType             = getType();

        // If we are doing time elapsed query the end timestamp
        if (queryType == GL_TIME_ELAPSED_EXT)
        {
            context->End(mActiveQuery->endTimestamp);
        }

        context->End(mActiveQuery->query);

        mPendingQueries.push_back(std::move(mActiveQuery));
        mActiveQuery = std::unique_ptr<QueryState>(new QueryState());
    }

    return flush(false);
}

gl::Error Query11::resume()
{
    if (mActiveQuery->query == nullptr)
    {
        ANGLE_TRY(flush(false));

        GLenum queryType         = getType();
        D3D11_QUERY d3dQueryType = gl_d3d11::ConvertQueryType(queryType);

        D3D11_QUERY_DESC queryDesc;
        queryDesc.Query     = d3dQueryType;
        queryDesc.MiscFlags = 0;

        ID3D11Device *device = mRenderer->getDevice();

        HRESULT result = device->CreateQuery(&queryDesc, &mActiveQuery->query);
        if (FAILED(result))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Internal query creation failed, result: 0x%X.",
                             result);
        }

        // If we are doing time elapsed we also need a query to actually query the timestamp
        if (queryType == GL_TIME_ELAPSED_EXT)
        {
            D3D11_QUERY_DESC desc;
            desc.Query     = D3D11_QUERY_TIMESTAMP;
            desc.MiscFlags = 0;
            result         = device->CreateQuery(&desc, &mActiveQuery->beginTimestamp);
            if (FAILED(result))
            {
                return gl::Error(GL_OUT_OF_MEMORY, "Internal query creation failed, result: 0x%X.",
                                 result);
            }
            result = device->CreateQuery(&desc, &mActiveQuery->endTimestamp);
            if (FAILED(result))
            {
                return gl::Error(GL_OUT_OF_MEMORY, "Internal query creation failed, result: 0x%X.",
                                 result);
            }
        }

        ID3D11DeviceContext *context = mRenderer->getDeviceContext();

        if (d3dQueryType != D3D11_QUERY_EVENT)
        {
            context->Begin(mActiveQuery->query);
        }

        // If we are doing time elapsed, query the begin timestamp
        if (queryType == GL_TIME_ELAPSED_EXT)
        {
            context->End(mActiveQuery->beginTimestamp);
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
                ASSERT(queryState->query);
                UINT64 numPixels = 0;
                HRESULT result =
                    context->GetData(queryState->query, &numPixels, sizeof(numPixels), 0);
                if (FAILED(result))
                {
                    return gl::Error(GL_OUT_OF_MEMORY,
                                     "Failed to get the data of an internal query, result: 0x%X.",
                                     result);
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
                ASSERT(queryState->query);
                D3D11_QUERY_DATA_SO_STATISTICS soStats = {0};
                HRESULT result = context->GetData(queryState->query, &soStats, sizeof(soStats), 0);
                if (FAILED(result))
                {
                    return gl::Error(GL_OUT_OF_MEMORY,
                                     "Failed to get the data of an internal query, result: 0x%X.",
                                     result);
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
                ASSERT(queryState->query);
                ASSERT(queryState->beginTimestamp);
                ASSERT(queryState->endTimestamp);
                D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timeStats = {0};
                HRESULT result =
                    context->GetData(queryState->query, &timeStats, sizeof(timeStats), 0);
                if (FAILED(result))
                {
                    return gl::Error(GL_OUT_OF_MEMORY,
                                     "Failed to get the data of an internal query, result: 0x%X.",
                                     result);
                }

                if (result == S_OK)
                {
                    UINT64 beginTime = 0;
                    HRESULT beginRes =
                        context->GetData(queryState->beginTimestamp, &beginTime, sizeof(UINT64), 0);
                    if (FAILED(beginRes))
                    {
                        return gl::Error(
                            GL_OUT_OF_MEMORY,
                            "Failed to get the data of an internal query, result: 0x%X.", beginRes);
                    }
                    UINT64 endTime = 0;
                    HRESULT endRes =
                        context->GetData(queryState->endTimestamp, &endTime, sizeof(UINT64), 0);
                    if (FAILED(endRes))
                    {
                        return gl::Error(
                            GL_OUT_OF_MEMORY,
                            "Failed to get the data of an internal query, result: 0x%X.", endRes);
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
                ASSERT(queryState->query == nullptr);
                mResult              = 0;
                queryState->finished = true;
            }
            break;

            case GL_COMMANDS_COMPLETED_CHROMIUM:
            {
                ASSERT(queryState->query);
                BOOL completed = 0;
                HRESULT result =
                    context->GetData(queryState->query, &completed, sizeof(completed), 0);
                if (FAILED(result))
                {
                    return gl::Error(GL_OUT_OF_MEMORY,
                                     "Failed to get the data of an internal query, result: 0x%X.",
                                     result);
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
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to test get query result, device is lost.");
        }
    }

    return gl::NoError();
}

}  // namespace rx
