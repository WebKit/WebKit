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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaSamplesDecoder.h"

#include <WebCore/TrackInfo.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaSamplesDecoder);

MediaSamplesDecoder::MediaSamplesDecoder() = default;

MediaSamplesDecoder::~MediaSamplesDecoder() = default;

bool MediaSamplesDecoder::canDecode(const TrackInfo&, Filters)
{
    // FIXME: only in-process software decoding of audio and VP8/VP9/AV1 video is planned.
    // GPU process, hardware and H.264/HEVC decoding are not supported yet.
    return false;
}

Ref<MediaSamplesDecoder::CreatePromise> MediaSamplesDecoder::create(const TrackInfo&, Filters)
{
    // FIXME: create the underlying AudioDecoder or VideoDecoder and resolve with the new decoder.
    return CreatePromise::createAndReject(PlatformMediaError::NotSupportedError);
}

bool MediaSamplesDecoder::canContinueWith(const TrackInfo&) const
{
    // FIXME: compare against the configuration the underlying decoder was created with.
    return false;
}

Ref<MediaSamplesDecoder::DecodePromise> MediaSamplesDecoder::decode(MediaSamplesBlock&&)
{
    // FIXME: decode into PCM (audio) or raw pixels (video).
    return DecodePromise::createAndReject(PlatformMediaError::NotSupportedError);
}

Ref<MediaSamplesDecoder::DecodePromise> MediaSamplesDecoder::drain()
{
    // FIXME: emit all pending decoded output.
    return DecodePromise::createAndReject(PlatformMediaError::NotSupportedError);
}

Ref<MediaPromise> MediaSamplesDecoder::flush()
{
    // FIXME: discard all queued input and pending output.
    return MediaPromise::createAndResolve();
}

} // namespace WebCore
