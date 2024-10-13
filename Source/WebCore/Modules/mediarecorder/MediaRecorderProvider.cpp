/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "MediaRecorderProvider.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(MEDIA_RECORDER)

#include "ContentType.h"
#include "MediaRecorderPrivate.h"

#if PLATFORM(COCOA) && USE(AVFOUNDATION)
#include "MediaRecorderPrivateAVFImpl.h"
#endif

#if USE(GSTREAMER_TRANSCODER)
#include "MediaRecorderPrivateGStreamer.h"
#endif

#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaRecorderProvider);

#if ENABLE(MEDIA_RECORDER)

std::unique_ptr<MediaRecorderPrivate> MediaRecorderProvider::createMediaRecorderPrivate(MediaStreamPrivate& stream, const MediaRecorderPrivateOptions& options)
{
#if PLATFORM(COCOA) && USE(AVFOUNDATION)
    return MediaRecorderPrivateAVFImpl::create(stream, options);
#elif USE(GSTREAMER_TRANSCODER)
    return MediaRecorderPrivateGStreamer::create(stream, options);
#else
    UNUSED_PARAM(stream);
    UNUSED_PARAM(options);
#endif
    return nullptr;
}

bool MediaRecorderProvider::isSupported(const String& value)
{
    if (value.isEmpty())
        return true;

    ContentType mimeType(value);
#if PLATFORM(COCOA)
    auto containerType = mimeType.containerType();
    if (equalLettersIgnoringASCIICase(containerType, "audio/mp4"_s) || equalLettersIgnoringASCIICase(containerType, "video/mp4"_s)) {
        for (auto& codec : mimeType.codecs()) {
            // FIXME: We should further validate parameters.
            if (!startsWithLettersIgnoringASCIICase(codec, "avc1"_s) && !startsWithLettersIgnoringASCIICase(codec, "mp4a"_s))
                return false;
        }
        return true;
    }
    return isWebMAndSupported(value);

#elif USE(GSTREAMER_TRANSCODER)
    return MediaRecorderPrivateGStreamer::isTypeSupported(mimeType);
#else
    UNUSED_VARIABLE(mimeType);
    return false;
#endif
}

bool MediaRecorderProvider::isWebMAndSupported(const String& mimeType)
{
    ContentType contentType(mimeType);
    auto containerType = contentType.containerType();
    if (!equalLettersIgnoringASCIICase(containerType, "audio/webm"_s) && !equalLettersIgnoringASCIICase(containerType, "video/webm"_s))
        return false;
#if PLATFORM(COCOA) && ENABLE(MEDIA_RECORDER_WEBM)
    for (auto& codec : contentType.codecs()) {
        // FIXME: We should further validate parameters.
        bool isVP9 = codec.startsWith("vp09"_s) || equal(codec, "vp9"_s) || equal(codec, "vp9.0"_s);
        bool isVP8 = codec.startsWith("vp08"_s) || equal(codec, "vp8"_s) || equal(codec, "vp8.0"_s);
        bool isOpus = codec == "opus"_s;
        if (!isVP9 && !isVP8 && !isOpus)
            return false;
    }
    return true;
#elif USE(GSTREAMER_TRANSCODER)
    return MediaRecorderPrivateGStreamer::isTypeSupported(contentType);
#else
    UNUSED_VARIABLE(contentType);
    return false;
#endif
}
#endif
}
