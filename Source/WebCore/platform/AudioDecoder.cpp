/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2023 Igalia S.L
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
#include "AudioDecoder.h"

#if ENABLE(WEB_CODECS)

#if USE(GSTREAMER)
#include "AudioDecoderGStreamer.h"
#include "GStreamerRegistryScanner.h"
#endif

#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

bool AudioDecoder::isCodecSupported(const StringView& codec)
{
    bool isMPEG4AAC = codec == "mp4a.40.2"_s || codec == "mp4a.40.02"_s || codec == "mp4a.40.5"_s
        || codec == "mp4a.40.05"_s || codec == "mp4a.40.29"_s || codec == "mp4a.40.42"_s;
    bool isCodecAllowed = isMPEG4AAC || codec == "mp3"_s || codec == "opus"_s
        || codec == "alaw"_s || codec == "ulaw"_s || codec == "flac"_s
        || codec == "vorbis"_s || codec.startsWith("pcm-"_s);

    if (!isCodecAllowed)
        return false;

    bool result = false;
#if USE(GSTREAMER)
    auto& scanner = GStreamerRegistryScanner::singleton();
    result = scanner.isCodecSupported(GStreamerRegistryScanner::Configuration::Decoding, codec.toString());
#endif

    return result;
}

void AudioDecoder::create(const String& codecName, const Config& config, CreateCallback&& callback, OutputCallback&& outputCallback, PostTaskCallback&& postCallback)
{
#if USE(GSTREAMER)
    GStreamerAudioDecoder::create(codecName, config, WTFMove(callback), WTFMove(outputCallback), WTFMove(postCallback));
    return;
#else
    UNUSED_PARAM(codecName);
    UNUSED_PARAM(config);
    UNUSED_PARAM(outputCallback);
    UNUSED_PARAM(postCallback);
#endif

    callback(makeUnexpected("Not supported"_s));
}

AudioDecoder::AudioDecoder() = default;
AudioDecoder::~AudioDecoder() = default;

}

#endif // ENABLE(WEB_CODECS)
