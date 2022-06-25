//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Query11.cpp: Defines the rx::Query11 class which implements rx::QueryImpl.

#include "libANGLE/renderer/d3d/d3d11/Query11.h"

#include <GLES2/gl2ext.h>

#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/d3d/d3d11/Context11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

namespace
{

GLuint64 MergeQueryResults(gl::QueryType type, GLuint64 currentResult, GLuint64 newResult)
{
    switch (type)
    {
        case gl::QueryType::AnySamples:
        case gl::QueryType::AnySamplesConservative:
            return (currentResult == GL_TRUE || newResult == GL_TRUE) ? GL_TRUE : GL_FALSE;

        case gl::QueryType::TransformFeedbackPrimitivesWritten:
            return currentResult + newResult;

        case gl::QueryType::TimeElapsed:
            return currentResult + newResult;

        case gl::QueryType::Timestamp:
            return newResult;

        case gl::QueryType::CommandsCompleted:
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
    : getDataAttemptCount(0), query(), beginTimestamp(), endTimestamp(), finished(false)
{}

Query11::QueryState::~QueryState() {}

Query11::Query11(Renderer11 *renderer, gl::QueryType type)
    : QueryImpl(type), mResult(0), mResultSum(0), mRenderer(renderer)
{
    mActiveQuery = std::unique_ptr<QueryState>(new QueryState());
}

Query11::~Query11()
{
    mRenderer->getStateManager()->onDeleteQueryObject(this);
}

angle::Result Query11::begin(const gl::Context *context)
{
    mResultSum = 0;
    mRenderer->getStateManager()->onBeginQuery(this);
    return resume(GetImplAs<Context11>(context));
}

angle::Result Query11::end(const gl::Context *context)
{
    return pause(GetImplAs<Context11>(context));
}

angle::Result Query11::queryCounter(const gl::Context *context)
{
    // This doesn't do anything for D3D11 as we don't support timestamps
    ASSERT(getType() == gl::QueryType::Timestamp);
    mResultSum = 0;
    mPendingQueries.push_back(std::unique_ptr<QueryState>(new QueryState()));
    return angle::Result::Continue;
}

template <typename T>
angle::Result Query11::getResultBase(Context11 *context11, T *params)
{
    ASSERT(!mActiveQuery->query.valid());
    ANGLE_TRY(flush(context11, true));
    ASSERT(mPendingQueries.empty());
    *params = static_cast<T>(mResultSum);

    return angle::Result::Continue;
}

angle::Result Query11::getResult(const gl::Context *context, GLint *params)
{
    return getResultBase(GetImplAs<Context11>(context), params);
}

angle::Result Query11::getResult(const gl::Context *context, GLuint *params)
{
    return getResultBase(GetImplAs<Context11>(context), params);
}

angle::Result Query11::getResult(const gl::Context *context, GLint64 *params)
{
    return getResultBase(GetImplAs<Context11>(context), params);
}

angle::Result Query11::getResult(const gl::Context *context, GLuint64 *params)
{
    return getResultBase(GetImplAs<Context11>(context), params);
}

angle::Result Query11::isResultAvailable(const gl::Context *context, bool *available)
{
    ANGLE_TRY(flush(GetImplAs<Context11>(context), false));

    *available = mPendingQueries.empty();
    return angle::Result::Continue;
}

angle::Result Query11::pause(Context11 *context11)
{
    if (mActiveQuery->query.valid())
    {
        ID3D11DeviceContext *context = mRenderer->getDeviceContext();
        gl::QueryType type           = getType();

        // If we are doing time elapsed query the end timestamp
        if (type == gl::QueryType::TimeElapsed)
        {
            context->End(mActiveQuery->endTimestamp.get());
        }

        context->End(mActiveQuery->query.get());

        mPendingQueries.push_back(std::move(mActiveQuery));
        mActiveQuery = std::unique_ptr<QueryState>(new QueryState());
    }

    return flush(context11, false);
}

angle::Result Query11::resume(Context11 *context11)
{
    if (!mActiveQuery->query.valid())
    {
        ANGLE_TRY(flush(context11, false));

        gl::QueryType type       = getType();
        D3D11_QUERY d3dQueryType = gl_d3d11::ConvertQueryType(type);

        D3D11_QUERY_DESC queryDesc;
        queryDesc.Query     = d3dQueryType;
        queryDesc.MiscFlags = 0;

        ANGLE_TRY(mRenderer->allocateResource(context11, queryDesc, &mActiveQuery->query));

        // If we are doing time elapsed we also need a query to actually query the timestamp
        if (type == gl::QueryType::TimeElapsed)
        {
            D3D11_QUERY_DESC desc;
            desc.Query     = D3D11_QUERY_TIMESTAMP;
            desc.MiscFlags = 0;

            ANGLE_TRY(mRenderer->allocateResource(context11, desc, &mActiveQuery->beginTimestamp));
            ANGLE_TRY(mRenderer->allocateResource(context11, desc, &mActiveQuery->endTimestamp));
        }

        ID3D11DeviceContext *context = mRenderer->getDeviceContext();

        if (d3dQueryType != D3D11_QUERY_EVENT)
        {
            context->Begin(mActiveQuery->query.get());
        }

        // If we are doing time elapsed, query the begin timestamp
        if (type == gl::QueryType::TimeElapsed)
        {
            context->End(mActiveQuery->beginTimestamp.get());
        }
    }

    return angle::Result::Continue;
}

angle::Result Query11::flush(Context11 *context11, bool force)
{
    while (!mPendingQueries.empty())
    {
        QueryState *query = mPendingQueries.front().get();

        do
        {
            ANGLE_TRY(testQuery(context11, query));
            if (!query->finished && !force)
            {
                return angle::Result::Continue;
            }
        } while (!query->finished);

        mResultSum = MergeQueryResults(getType(), mResultSum, mResult);
        mPendingQueries.pop_front();
    }

    return angle::Result::Continue;
}

angle::Result Query11::testQuery(Context11 *context11, QueryState *queryState)
{
    if (!queryState->finished)
    {
        ID3D11DeviceContext *context = mRenderer->getDeviceContext();
        switch (getType())
        {
            case gl::QueryType::AnySamples:
            case gl::QueryType::AnySamplesConservative:
            {
                ASSERT(queryState->query.valid());
                UINT64 numPixels = 0;
                HRESULT result =
                    context->GetData(queryState->query.get(), &numPixels, sizeof(numPixels), 0);
                ANGLE_TRY_HR(context11, result, "Failed to get the data of an internal query");

                if (result == S_OK)
                {
                    queryState->finished = true;
                    mResult              = (numPixels > 0) ? GL_TRUE : GL_FALSE;
                }
            }
            break;

            case gl::QueryType::TransformFeedbackPrimitivesWritten:
            {
                ASSERT(queryState->query.valid());
                D3D11_QUERY_DATA_SO_STATISTICS soStats = {};
                HRESULT result =
                    context->GetData(queryState->query.get(), &soStats, sizeof(soStats), 0);
                ANGLE_TRY_HR(context11, result, "Failed to get the data of an internal query");

                if (result == S_OK)
                {
                    queryState->finished = true;
                    mResult              = static_cast<GLuint64>(soStats.NumPrimitivesWritten);
                }
            }
            break;

            case gl::QueryType::TimeElapsed:
            {
                ASSERT(queryState->query.valid());
                ASSERT(queryState->beginTimestamp.valid());
                ASSERT(queryState->endTimestamp.valid());
                D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timeStats = {};
                HRESULT result =
                    context->GetData(queryState->query.get(), &timeStats, sizeof(timeStats), 0);
                ANGLE_TRY_HR(context11, result, "Failed to get the data of an internal query");

                if (result == S_OK)
                {
                    UINT64 beginTime = 0;
                    HRESULT beginRes = context->GetData(queryState->beginTimestamp.get(),
                                                        &beginTime, sizeof(UINT64), 0);
                    ANGLE_TRY_HR(context11, beginRes,
                                 "Failed to get the data of an internal query");

                    UINT64 endTime = 0;
                    HRESULT endRes = context->GetData(queryState->endTimestamp.get(), &endTime,
                                                      sizeof(UINT64), 0);
                    ANGLE_TRY_HR(context11, endRes, "Failed to get the data of an internal query");

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

            case gl::QueryType::Timestamp:
            {
                // D3D11 doesn't support GL timestamp queries as D3D timestamps are not guaranteed
                // to have any sort of continuity outside of a disjoint timestamp query block, which
                // GL depends on
                ASSERT(!queryState->query.valid());
                mResult              = 0;
                queryState->finished = true;
            }
            break;

            case gl::QueryType::CommandsCompleted:
            {
                ASSERT(queryState->query.valid());
                BOOL completed = 0;
                HRESULT result =
                    context->GetData(queryState->query.get(), &completed, sizeof(completed), 0);
                ANGLE_TRY_HR(context11, result, "Failed to get the data of an internal query");

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

        queryState->getDataAttemptCount++;
        bool checkDeviceLost =
            (queryState->getDataAttemptCount % kPollingD3DDeviceLostCheckFrequency) == 0;
        if (!queryState->finished && checkDeviceLost && mRenderer->testDeviceLost())
        {
            mRenderer->notifyDeviceLost();
            ANGLE_TRY_HR(context11, E_OUTOFMEMORY,
                         "Failed to test get query result, device is lost.");
        }
    }

    return angle::Result::Continue;
}

}  // namespace rx
