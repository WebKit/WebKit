/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Zoltan Herczeg
 * Copyright (C) 2018-2024 Apple, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FELightingSoftwareParallelApplier.h"

#if !(CPU(ARM_NEON) && CPU(ARM_TRADITIONAL) && COMPILER(GCC_COMPATIBLE))
#include "FELightingSoftwareApplierInlines.h"
#include "ImageBuffer.h"
#include <wtf/ParallelJobs.h>

namespace WebCore {

// This appears to read from and write to the same pixel buffer, but it only reads the alpha channel, and writes the non-alpha channels.
void FELightingSoftwareParallelApplier::applyPlatformPaint(const LightingData& data, const LightSource::PaintingData& paintingData, int startY, int endY)
{
    // Make sure startY is > 0 since we read from the previous row in the loop.
    ASSERT(startY);
    ASSERT(endY > startY);

    for (int y = startY; y < endY; ++y) {
        int rowStartOffset = y * data.widthMultipliedByPixelSize;
        int previousRowStart = rowStartOffset - data.widthMultipliedByPixelSize;
        int nextRowStart = rowStartOffset + data.widthMultipliedByPixelSize;

        // alphaWindow is a local cache of alpha values.
        // Fill the two right columns putting the left edge value in the center column.
        // For each pixel, we shift each row left then fill the right column.
        AlphaWindow alphaWindow;
        alphaWindow.setTop(data.pixels->item(previousRowStart + cAlphaChannelOffset));
        alphaWindow.setTopRight(data.pixels->item(previousRowStart + cPixelSize + cAlphaChannelOffset));

        alphaWindow.setCenter(data.pixels->item(rowStartOffset + cAlphaChannelOffset));
        alphaWindow.setRight(data.pixels->item(rowStartOffset + cPixelSize + cAlphaChannelOffset));

        alphaWindow.setBottom(data.pixels->item(nextRowStart + cAlphaChannelOffset));
        alphaWindow.setBottomRight(data.pixels->item(nextRowStart + cPixelSize + cAlphaChannelOffset));

        int offset = rowStartOffset + cPixelSize;
        for (int x = 1; x < data.width - 1; ++x, offset += cPixelSize) {
            alphaWindow.shift();
            setPixelInternal(offset, data, paintingData, x, y, cFactor1div4, cFactor1div4, data.interiorNormal(offset, alphaWindow), alphaWindow.center());
        }
    }
}

void FELightingSoftwareParallelApplier::applyPlatformWorker(ApplyParameters* parameters)
{
    applyPlatformPaint(parameters->data, parameters->paintingData, parameters->yStart, parameters->yEnd);
}

void FELightingSoftwareParallelApplier::applyPlatformParallel(const LightingData& data, const LightSource::PaintingData& paintingData) const
{
    unsigned rowsToProcess = data.height - 2;
    unsigned maxNumThreads = rowsToProcess / 8;
    unsigned optimalThreadNumber = std::min<unsigned>(((data.width - 2) * rowsToProcess) / minimalRectDimension, maxNumThreads);

    if (optimalThreadNumber > 1) {
        // Initialize parallel jobs
        ParallelJobs<ApplyParameters> parallelJobs(&applyPlatformWorker, optimalThreadNumber);

        // Fill the parameter array
        int job = parallelJobs.numberOfJobs();
        if (job > 1) {
            // Split the job into "yStep"-sized jobs but there a few jobs that need to be slightly larger since
            // yStep * jobs < total size. These extras are handled by the remainder "jobsWithExtra".
            const int yStep = rowsToProcess / job;
            const int jobsWithExtra = rowsToProcess % job;

            int yStart = 1;
            for (--job; job >= 0; --job) {
                ApplyParameters& params = parallelJobs.parameter(job);
                params.data = data;
                params.paintingData = paintingData;
                params.yStart = yStart;
                yStart += job < jobsWithExtra ? yStep + 1 : yStep;
                params.yEnd = yStart;
            }
            parallelJobs.execute();
            return;
        }
        // Fallback to single threaded mode.
    }

    applyPlatformPaint(data, paintingData, 1, data.height - 1);
}

} // namespace WebCore

#endif // !(CPU(ARM_NEON) && CPU(ARM_TRADITIONAL) && COMPILER(GCC_COMPATIBLE))
