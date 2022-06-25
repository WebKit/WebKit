/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioChannel.h"

#include "VectorMath.h"
#include <algorithm>
#include <math.h>

namespace WebCore {

void AudioChannel::scale(float scale)
{
    if (isSilent())
        return;

    VectorMath::multiplyByScalar(data(), scale, mutableData(), length());
}

void AudioChannel::copyFrom(const AudioChannel* sourceChannel)
{
    bool isSafe = (sourceChannel && sourceChannel->length() >= length());
    ASSERT(isSafe);

    if (!isSafe || sourceChannel->isSilent()) {
        zero();
        return;
    }
    memcpy(mutableData(), sourceChannel->data(), sizeof(float) * length());
}

void AudioChannel::copyFromRange(const AudioChannel* sourceChannel, unsigned startFrame, unsigned endFrame)
{
    // Check that range is safe for reading from sourceChannel.
    bool isRangeSafe = sourceChannel && startFrame < endFrame && endFrame <= sourceChannel->length();
    ASSERT(isRangeSafe);
    if (!isRangeSafe)
        return;

    if (sourceChannel->isSilent() && isSilent())
        return;

    // Check that this channel has enough space.
    size_t rangeLength = endFrame - startFrame;
    bool isRangeLengthSafe = rangeLength <= length();
    ASSERT(isRangeLengthSafe);
    if (!isRangeLengthSafe)
        return;

    const float* source = sourceChannel->data();
    float* destination = mutableData();

    if (sourceChannel->isSilent()) {
        if (rangeLength == length())
            zero();
        else
            memset(destination, 0, sizeof(float) * rangeLength);
    } else
        memcpy(destination, source + startFrame, sizeof(float) * rangeLength);
}

void AudioChannel::sumFrom(const AudioChannel* sourceChannel)
{
    bool isSafe = sourceChannel && sourceChannel->length() >= length();
    ASSERT(isSafe);
    if (!isSafe)
        return;

    if (sourceChannel->isSilent())
        return;

    if (isSilent())
        copyFrom(sourceChannel);
    else
        VectorMath::add(data(), sourceChannel->data(), mutableData(), length());
}

float AudioChannel::maxAbsValue() const
{
    if (isSilent())
        return 0;

    return VectorMath::maximumMagnitude(data(), length());
}

} // WebCore

#endif // ENABLE(WEB_AUDIO)
