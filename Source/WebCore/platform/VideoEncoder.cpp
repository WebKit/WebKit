/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "VideoEncoder.h"

#if ENABLE(WEB_CODECS)

#include "LibWebRTCVPXVideoEncoder.h"

namespace WebCore {

VideoEncoder::CreatorFunction VideoEncoder::s_customCreator = nullptr;

void VideoEncoder::setCreatorCallback(CreatorFunction&& function)
{
    s_customCreator = WTFMove(function);
}

void VideoEncoder::create(const String& codecName, const Config& config, CreateCallback&& callback, DescriptionCallback&& descriptionCallback, OutputCallback&& outputCallback, PostTaskCallback&& postCallback)
{
    if (s_customCreator) {
        s_customCreator(codecName, config, WTFMove(callback), WTFMove(descriptionCallback), WTFMove(outputCallback), WTFMove(postCallback));
        return;
    }
    createLocalEncoder(codecName, config, WTFMove(callback), WTFMove(descriptionCallback), WTFMove(outputCallback), WTFMove(postCallback));
}

void VideoEncoder::createLocalEncoder(const String& codecName, const Config& config, CreateCallback&& callback, DescriptionCallback&& descriptionCallback, OutputCallback&& outputCallback, PostTaskCallback&& postCallback)
{
#if USE(LIBWEBRTC)
    if (codecName == "vp8"_s) {
        LibWebRTCVPXVideoEncoder::create(LibWebRTCVPXVideoEncoder::Type::VP8, config, WTFMove(callback), WTFMove(descriptionCallback), WTFMove(outputCallback), WTFMove(postCallback));
        return;
    }
    if (codecName.startsWith("vp09.00"_s)) {
        LibWebRTCVPXVideoEncoder::create(LibWebRTCVPXVideoEncoder::Type::VP9, config, WTFMove(callback), WTFMove(descriptionCallback), WTFMove(outputCallback), WTFMove(postCallback));
        return;
    }
#else
    UNUSED_PARAM(codecName);
    UNUSED_PARAM(outputCallback);
    UNUSED_PARAM(postCallback);
#endif

    callback(makeUnexpected("Not supported"_s));
}

}

#endif // ENABLE(WEB_CODECS)
