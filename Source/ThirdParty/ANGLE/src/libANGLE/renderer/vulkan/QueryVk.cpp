//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// QueryVk.cpp:
//    Implements the class methods for QueryVk.
//

#include "libANGLE/renderer/vulkan/QueryVk.h"
#include "libANGLE/Context.h"
#include "libANGLE/TransformFeedback.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/TransformFeedbackVk.h"

#include "common/debug.h"

namespace rx
{

QueryVk::QueryVk(gl::QueryType type)
    : QueryImpl(type),
      mTransformFeedbackPrimitivesDrawn(0),
      mCachedResult(0),
      mCachedResultValid(false)
{}

QueryVk::~QueryVk() = default;

void QueryVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    if (getType() != gl::QueryType::TransformFeedbackPrimitivesWritten)
    {
        vk::DynamicQueryPool *queryPool = contextVk->getQueryPool(getType());
        queryPool->freeQuery(contextVk, &mQueryHelper);
        queryPool->freeQuery(contextVk, &mQueryHelperTimeElapsedBegin);
    }
}

angle::Result QueryVk::stashQueryHelper(ContextVk *contextVk)
{
    ASSERT(isOcclusionQuery());
    mStashedQueryHelpers.emplace_back(mQueryHelper);
    mQueryHelper.deinit();
    ANGLE_TRY(contextVk->getQueryPool(getType())->allocateQuery(contextVk, &mQueryHelper));
    return angle::Result::Continue;
}

angle::Result QueryVk::retrieveStashedQueryResult(ContextVk *contextVk, uint64_t *result)
{
    uint64_t total = 0;
    for (vk::QueryHelper &query : mStashedQueryHelpers)
    {
        uint64_t v;
        ANGLE_TRY(query.getUint64Result(contextVk, &v));
        total += v;
    }
    mStashedQueryHelpers.clear();
    *result = total;
    return angle::Result::Continue;
}

angle::Result QueryVk::begin(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    mCachedResultValid = false;

    // Transform feedback query is a handled by a CPU-calculated value when emulated.
    if (getType() == gl::QueryType::TransformFeedbackPrimitivesWritten)
    {
        mTransformFeedbackPrimitivesDrawn = 0;
        // We could consider using VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT.
        return angle::Result::Continue;
    }

    if (!mQueryHelper.valid())
    {
        ANGLE_TRY(contextVk->getQueryPool(getType())->allocateQuery(contextVk, &mQueryHelper));
    }

    if (isOcclusionQuery())
    {
        // For pathological usage case where begin/end is called back to back without flush and get
        // result, we have to force flush so that the same mQueryHelper will not encoded in the same
        // renderpass twice without resetting it.
        if (mQueryHelper.hasPendingWork(contextVk))
        {
            ANGLE_TRY(contextVk->flushImpl(nullptr));
            // As soon as beginQuery is called, previous query's result will not retrievable by API.
            // We must clear it so that it will not count against current beginQuery call.
            mStashedQueryHelpers.clear();
            mQueryHelper.deinit();
            ANGLE_TRY(contextVk->getQueryPool(getType())->allocateQuery(contextVk, &mQueryHelper));
        }
        contextVk->beginOcclusionQuery(this);
    }
    else if (getType() == gl::QueryType::TimeElapsed)
    {
        // Note: TimeElapsed is implemented by using two Timestamp queries and taking the diff.
        if (!mQueryHelperTimeElapsedBegin.valid())
        {
            ANGLE_TRY(contextVk->getQueryPool(getType())->allocateQuery(
                contextVk, &mQueryHelperTimeElapsedBegin));
        }

        ANGLE_TRY(mQueryHelperTimeElapsedBegin.flushAndWriteTimestamp(contextVk));
    }
    else
    {
        ANGLE_TRY(mQueryHelper.beginQuery(contextVk));
    }

    return angle::Result::Continue;
}

angle::Result QueryVk::end(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (getType() == gl::QueryType::TransformFeedbackPrimitivesWritten)
    {
        mCachedResult = mTransformFeedbackPrimitivesDrawn;

        // There could be transform feedback in progress, so add the primitives drawn so far from
        // the current transform feedback object.
        gl::TransformFeedback *transformFeedback =
            context->getState().getCurrentTransformFeedback();
        if (transformFeedback)
        {
            mCachedResult += transformFeedback->getPrimitivesDrawn();
        }
        mCachedResultValid = true;
        // We could consider using VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT.
    }
    else if (isOcclusionQuery())
    {
        contextVk->endOcclusionQuery(this);
    }
    else if (getType() == gl::QueryType::TimeElapsed)
    {
        ANGLE_TRY(mQueryHelper.flushAndWriteTimestamp(contextVk));
    }
    else
    {
        ANGLE_TRY(mQueryHelper.endQuery(contextVk));
    }

    return angle::Result::Continue;
}

angle::Result QueryVk::queryCounter(const gl::Context *context)
{
    ASSERT(getType() == gl::QueryType::Timestamp);
    ContextVk *contextVk = vk::GetImpl(context);

    mCachedResultValid = false;

    if (!mQueryHelper.valid())
    {
        ANGLE_TRY(contextVk->getQueryPool(getType())->allocateQuery(contextVk, &mQueryHelper));
    }

    return mQueryHelper.flushAndWriteTimestamp(contextVk);
}

angle::Result QueryVk::getResult(const gl::Context *context, bool wait)
{
    if (mCachedResultValid)
    {
        return angle::Result::Continue;
    }

    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    // glGetQueryObject* requires an implicit flush of the command buffers to guarantee execution in
    // finite time.
    // Note regarding time-elapsed: end should have been called after begin, so flushing when end
    // has pending work should flush begin too.
    if (mQueryHelper.hasPendingWork(contextVk))
    {
        ANGLE_TRY(contextVk->flushImpl(nullptr));

        ASSERT(!mQueryHelperTimeElapsedBegin.hasPendingWork(contextVk));
        ASSERT(!mQueryHelper.hasPendingWork(contextVk));
    }

    // If the command buffer this query is being written to is still in flight, its reset command
    // may not have been performed by the GPU yet.  To avoid a race condition in this case, wait
    // for the batch to finish first before querying (or return not-ready if not waiting).
    ANGLE_TRY(contextVk->checkCompletedCommands());
    if (contextVk->isSerialInUse(mQueryHelper.getStoredQueueSerial()))
    {
        if (!wait)
        {
            return angle::Result::Continue;
        }
        ANGLE_TRY(contextVk->finishToSerial(mQueryHelper.getStoredQueueSerial()));
    }

    if (wait)
    {
        ANGLE_TRY(mQueryHelper.getUint64Result(contextVk, &mCachedResult));
        uint64_t v;
        ANGLE_TRY(retrieveStashedQueryResult(contextVk, &v));
        mCachedResult += v;
    }
    else
    {
        bool available = false;
        ANGLE_TRY(mQueryHelper.getUint64ResultNonBlocking(contextVk, &mCachedResult, &available));
        if (!available)
        {
            // If the results are not ready, do nothing.  mCachedResultValid remains false.
            return angle::Result::Continue;
        }
        uint64_t v;
        ANGLE_TRY(retrieveStashedQueryResult(contextVk, &v));
        mCachedResult += v;
    }

    double timestampPeriod = renderer->getPhysicalDeviceProperties().limits.timestampPeriod;

    // Fix up the results to what OpenGL expects.
    switch (getType())
    {
        case gl::QueryType::AnySamples:
        case gl::QueryType::AnySamplesConservative:
            // OpenGL query result in these cases is binary
            mCachedResult = !!mCachedResult;
            break;
        case gl::QueryType::Timestamp:
            mCachedResult = static_cast<uint64_t>(mCachedResult * timestampPeriod);
            break;
        case gl::QueryType::TimeElapsed:
        {
            uint64_t timeElapsedEnd = mCachedResult;

            // Since the result of the end query of time-elapsed is already available, the
            // result of begin query must be available too.
            ANGLE_TRY(mQueryHelperTimeElapsedBegin.getUint64Result(contextVk, &mCachedResult));

            mCachedResult = timeElapsedEnd - mCachedResult;
            mCachedResult = static_cast<uint64_t>(mCachedResult * timestampPeriod);

            break;
        }
        default:
            UNREACHABLE();
            break;
    }

    mCachedResultValid = true;
    return angle::Result::Continue;
}
angle::Result QueryVk::getResult(const gl::Context *context, GLint *params)
{
    ANGLE_TRY(getResult(context, true));
    *params = static_cast<GLint>(mCachedResult);
    return angle::Result::Continue;
}

angle::Result QueryVk::getResult(const gl::Context *context, GLuint *params)
{
    ANGLE_TRY(getResult(context, true));
    *params = static_cast<GLuint>(mCachedResult);
    return angle::Result::Continue;
}

angle::Result QueryVk::getResult(const gl::Context *context, GLint64 *params)
{
    ANGLE_TRY(getResult(context, true));
    *params = static_cast<GLint64>(mCachedResult);
    return angle::Result::Continue;
}

angle::Result QueryVk::getResult(const gl::Context *context, GLuint64 *params)
{
    ANGLE_TRY(getResult(context, true));
    *params = mCachedResult;
    return angle::Result::Continue;
}

angle::Result QueryVk::isResultAvailable(const gl::Context *context, bool *available)
{
    ANGLE_TRY(getResult(context, false));
    *available = mCachedResultValid;

    return angle::Result::Continue;
}

void QueryVk::onTransformFeedbackEnd(GLsizeiptr primitivesDrawn)
{
    mTransformFeedbackPrimitivesDrawn += primitivesDrawn;
}
}  // namespace rx
