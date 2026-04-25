/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
 *
 */

#import "config.h"
#import "WebRTCVideoDecoderVTBAV1.h"

#if USE(LIBWEBRTC)

#import "AV1Utilities.h"
#import "CMUtilities.h"
#import "TrackInfo.h"
#import <wtf/BlockPtr.h>

namespace WebCore {

static RetainPtr<CMVideoFormatDescriptionRef> computeAV1InputFormat(std::span<const uint8_t> data, int32_t width, int32_t height)
{
#if ENABLE(AV1)
    RefPtr videoInfo = createVideoInfoFromAV1Stream(data);
    if (!videoInfo)
        return { };

    if (width && videoInfo->size().width() != width)
        return { };
    if (height && videoInfo->size().height() != height)
        return { };

    return createFormatDescriptionFromTrackInfo(*videoInfo);
#else
    UNUSED_PARAM(data);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    ASSERT_NOT_REACHED();
    return { };
#endif
}

WebRTCVideoDecoderVTBAV1::WebRTCVideoDecoderVTBAV1(WebRTCVideoDecoderCallback callback)
    : WebRTCVideoDecoderVTB(callback)
{
}

int32_t WebRTCVideoDecoderVTBAV1::decodeFrame(int64_t timeStamp, std::span<const uint8_t> data)
{
    if (auto videoFormat = computeAV1InputFormat(data, width(), height()))
        setVideoFormat(WTF::move(videoFormat));

    return decodeFrameInternal(timeStamp, data);
}

}
#endif // USE(LIBWEBRTC)
