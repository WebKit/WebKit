/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/ParallelJobs.h>

namespace WebCore {

template<typename ApplyParameters>
inline bool applyPlatformParallel(typename ParallelJobs<ApplyParameters>::WorkerFunction func, int requestedJobNumber, const ApplyParameters& paintParameters, int extraHeight)
{
    if (requestedJobNumber <= 1)
        return false;

    ParallelJobs<ApplyParameters> parallelJobs(func, requestedJobNumber);
    int jobs = parallelJobs.numberOfJobs();
    if (jobs <= 1)
        return false;

    RefPtr sourceBuffer = paintParameters.sourceBuffer;
    RefPtr destinationBuffer = paintParameters.destinationBuffer;
    IntSize paintSize = destinationBuffer->size();

    // Split the job into "blockHeight"-sized jobs but there a few jobs that need to be slightly larger since
    // blockHeight * jobs < total size. These extras are handled by the remainder "jobsWithExtra".
    const int blockHeight = paintSize.height() / jobs;
    if (blockHeight <= extraHeight)
        return false;

    const int jobsWithExtra = paintSize.height() % jobs;
    const int scanline = 4 * paintSize.width();

    auto calculateAdjustedBlockHeight = [&](int job) -> int {
        return job < jobsWithExtra ? blockHeight + 1 : blockHeight;
    };

    int currentY = 0;
    for (int job = 0; job < jobs; ++job) {
        ApplyParameters& params = parallelJobs.parameter(job);
        int adjustedBlockHeight = calculateAdjustedBlockHeight(job);

        IntRect sourceRect { 0, currentY, paintSize.width(), adjustedBlockHeight };
        IntRect destinationRect { { }, sourceRect.size() };

        if (job) {
            sourceRect.shiftYEdgeBy(-extraHeight);
            destinationRect.setY(extraHeight);
        }

        if (job < jobs - 1)
            sourceRect.shiftMaxYEdgeBy(extraHeight);

        params = paintParameters;
        params.sourceSize = sourceRect.size();
        params.destinationRect = destinationRect;

        if (job) {
            params.sourceBuffer = sourceBuffer->createScratchPixelBuffer(params.sourceSize);
            memcpySpan(params.sourceBuffer->bytes(), sourceBuffer->bytes().subspan(sourceRect.y() * scanline, params.sourceBuffer->bytes().size()));
            params.destinationBuffer = sourceBuffer->createScratchPixelBuffer(params.destinationRect.size());
        }

        currentY += adjustedBlockHeight;
    }

    parallelJobs.execute();

    // Copy together the parts of the image.
    currentY = 0;
    int scratchHeight = calculateAdjustedBlockHeight(0);
    for (int job = 1; job < jobs; ++job) {
        ApplyParameters& params = parallelJobs.parameter(job);

        currentY += scratchHeight;
        scratchHeight = calculateAdjustedBlockHeight(job);

        unsigned destinationOffset = currentY * scanline;
        unsigned scratchSize = scratchHeight * scanline;

        // The final result of each job should be stored in ioBuffer.
        ASSERT(params.destinationBuffer->bytes().size() >= scratchSize);
        memcpySpan(destinationBuffer->bytes().subspan(destinationOffset), params.destinationBuffer->bytes().subspan(0, scratchSize));
    }

    return true;
}

} // namespace WebCore
