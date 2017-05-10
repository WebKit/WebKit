/*
 * Copyright (C) 2005, 2007, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2005 Ben La Monica <ben.lamonica@gmail.com>.  All rights reserved.
 * Copyright (C) 2011 Brent Fulgham. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PlatformImage.h"

#include <algorithm>
#include <cstdlib>

namespace ImageDiff {

bool PlatformImage::isCompatible(const PlatformImage& other) const
{
    return width() == other.width()
        && height() == other.height()
        && rowBytes() == other.rowBytes()
        && hasAlpha() == other.hasAlpha();
}

std::unique_ptr<PlatformImage> PlatformImage::difference(const PlatformImage& other, float& percentageDifference)
{
    size_t width = this->width();
    size_t height = this->height();

    // Compare the content of the 2 bitmaps
    void* diffBuffer = malloc(width * height);
    float count = 0.0f;
    float sum = 0.0f;
    float maxDistance = 0.0f;
    unsigned char* basePixel = this->pixels();
    unsigned char* pixel = other.pixels();
    unsigned char* diffPixel = reinterpret_cast<unsigned char*>(diffBuffer);
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            float red = (pixel[0] - basePixel[0]) / std::max<float>(255 - basePixel[0], basePixel[0]);
            float green = (pixel[1] - basePixel[1]) / std::max<float>(255 - basePixel[1], basePixel[1]);
            float blue = (pixel[2] - basePixel[2]) / std::max<float>(255 - basePixel[2], basePixel[2]);
            float alpha = (pixel[3] - basePixel[3]) / std::max<float>(255 - basePixel[3], basePixel[3]);
            float distance = sqrtf(red * red + green * green + blue * blue + alpha * alpha) / 2.0f;

            *diffPixel++ = static_cast<unsigned char>(distance * 255.0f);

            if (distance >= 1.0f / 255.0f) {
                count += 1.0f;
                sum += distance;
                if (distance > maxDistance)
                    maxDistance = distance;
            }

            basePixel += 4;
            pixel += 4;
        }
    }

    // Compute the difference as a percentage combining both the number of different pixels and their difference amount i.e. the average distance over the entire image
    if (count > 0.0f)
        percentageDifference = 100.0f * sum / (height * width);
    else
        percentageDifference = 0.0f;

    if (!percentageDifference) {
        free(diffBuffer);
        return nullptr;
    }

    // Generate a normalized diff image if there is any difference
    if (maxDistance < 1.0f) {
        diffPixel = reinterpret_cast<unsigned char*>(diffBuffer);
        for (size_t p = 0; p < height * width; ++p)
            diffPixel[p] /= maxDistance;
    }

    return PlatformImage::createFromDiffData(diffBuffer, width, height);
}

} // namespace ImageDiff
