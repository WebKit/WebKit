//
// Copyright 2016-2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// QueryVk.cpp:
//    Implements the class methods for QueryVk.
//

#include "libANGLE/renderer/vulkan/QueryVk.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

#include "common/debug.h"

namespace rx
{

QueryVk::QueryVk(gl::QueryType type) : QueryImpl(type), mCachedResult(0), mCachedResultValid(false)
{}

QueryVk::~QueryVk() = default;

void QueryVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk            = vk::GetImpl(context);
    vk::DynamicQueryPool *queryPool = contextVk->getQueryPool(getType());
    queryPool->freeQuery(contextVk, &mQueryHelper);
    queryPool->freeQuery(contextVk, &mQueryHelperTimeElapsedBegin);
}

angle::Result QueryVk::begin(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    mCachedResultValid = false;

    if (!mQueryHelper.getQueryPool())
    {
        ANGLE_TRY(contextVk->getQueryPool(getType())->allocateQuery(contextVk, &mQueryHelper));
    }

    // Note: TimeElapsed is implemented by using two Timestamp queries and taking the diff.
    if (getType() == gl::QueryType::TimeElapsed)
    {
        if (!mQueryHelperTimeElapsedBegin.getQueryPool())
        {
            ANGLE_TRY(contextVk->getQueryPool(getType())->allocateQuery(
                contextVk, &mQueryHelperTimeElapsedBegin));
        }

        mQueryHelperTimeElapsedBegin.writeTimestamp(contextVk);
    }
    else
    {
        mQueryHelper.beginQuery(contextVk);
    }

    return angle::Result::Continue;
}

angle::Result QueryVk::end(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (getType() == gl::QueryType::TimeElapsed)
    {
        mQueryHelper.writeTimestamp(contextVk);
    }
    else
    {
        mQueryHelper.endQuery(contextVk);
    }

    return angle::Result::Continue;
}

angle::Result QueryVk::queryCounter(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    mCachedResultValid = false;

    if (!mQueryHelper.getQueryPool())
    {
        ANGLE_TRY(contextVk->getQueryPool(getType())->allocateQuery(contextVk, &mQueryHelper));
    }

    ASSERT(getType() == gl::QueryType::Timestamp);

    mQueryHelper.writeTimestamp(contextVk);

    return angle::Result::Continue;
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
    if (mQueryHelper.hasPendingWork(renderer))
    {
        ANGLE_TRY(contextVk->flushImpl());

        ASSERT(!mQueryHelperTimeElapsedBegin.hasPendingWork(renderer));
        ASSERT(!mQueryHelper.hasPendingWork(renderer));
    }

    // If the command buffer this query is being written to is still in flight, its reset command
    // may not have been performed by the GPU yet.  To avoid a race condition in this case, wait
    // for the batch to finish first before querying (or return not-ready if not waiting).
    ANGLE_TRY(renderer->checkCompletedCommands(contextVk));
    if (renderer->isSerialInUse(mQueryHelper.getStoredQueueSerial()))
    {
        if (!wait)
        {
            return angle::Result::Continue;
        }
        ANGLE_TRY(renderer->finishToSerial(contextVk, mQueryHelper.getStoredQueueSerial()));
    }

    VkQueryResultFlags flags = (wait ? VK_QUERY_RESULT_WAIT_BIT : 0) | VK_QUERY_RESULT_64_BIT;

    VkResult result = mQueryHelper.getQueryPool()->getResults(
        contextVk->getDevice(), mQueryHelper.getQuery(), 1, sizeof(mCachedResult), &mCachedResult,
        sizeof(mCachedResult), flags);
    // If the results are not ready, do nothing.  mCachedResultValid remains false.
    if (result == VK_NOT_READY)
    {
        // If VK_QUERY_RESULT_WAIT_BIT was given, VK_NOT_READY cannot have been returned.
        ASSERT(!wait);
        return angle::Result::Continue;
    }
    ANGLE_VK_TRY(contextVk, result);

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

            result = mQueryHelperTimeElapsedBegin.getQueryPool()->getResults(
                contextVk->getDevice(), mQueryHelperTimeElapsedBegin.getQuery(), 1,
                sizeof(mCachedResult), &mCachedResult, sizeof(mCachedResult), flags);
            // Since the result of the end query of time-elapsed is already available, the
            // result of begin query must be available too.
            ASSERT(result != VK_NOT_READY);
            ANGLE_VK_TRY(contextVk, result);

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

}  // namespace rx
