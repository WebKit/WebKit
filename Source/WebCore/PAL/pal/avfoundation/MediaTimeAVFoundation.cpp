/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MediaTimeAVFoundation.h"

#if USE(AVFOUNDATION)

#include "CoreMediaSoftLink.h"

namespace PAL {

static bool CMTimeHasFlags(const CMTime& cmTime, uint32_t flags)
{
    return (cmTime.flags & flags) == flags;
}

MediaTime toMediaTime(const CMTime& cmTime)
{
    uint32_t flags = 0;
    if (CMTimeHasFlags(cmTime, kCMTimeFlags_Valid))
        flags |= MediaTime::Valid;
    if (CMTimeHasFlags(cmTime, kCMTimeFlags_Valid | kCMTimeFlags_HasBeenRounded))
        flags |= MediaTime::HasBeenRounded;
    if (CMTimeHasFlags(cmTime, kCMTimeFlags_Valid | kCMTimeFlags_PositiveInfinity))
        flags |= MediaTime::PositiveInfinite;
    if (CMTimeHasFlags(cmTime, kCMTimeFlags_Valid | kCMTimeFlags_NegativeInfinity))
        flags |= MediaTime::NegativeInfinite;
    if (CMTimeHasFlags(cmTime, kCMTimeFlags_Valid | kCMTimeFlags_Indefinite))
        flags |= MediaTime::Indefinite;

    return MediaTime(cmTime.value, cmTime.timescale, flags);
}

CMTime toCMTime(const MediaTime& mediaTime)
{
    CMTime time;

    if (mediaTime.hasDoubleValue())
        time = CMTimeMakeWithSeconds(mediaTime.toDouble(), mediaTime.timeScale());
    else
        time = CMTimeMake(mediaTime.timeValue(), mediaTime.timeScale());

    if (mediaTime.isValid())
        time.flags |= kCMTimeFlags_Valid;
    if (mediaTime.hasBeenRounded())
        time.flags |= kCMTimeFlags_HasBeenRounded;
    if (mediaTime.isPositiveInfinite())
        time.flags |= kCMTimeFlags_PositiveInfinity;
    if (mediaTime.isNegativeInfinite())
        time.flags |= kCMTimeFlags_NegativeInfinity;

    return time;
}

}

#endif
