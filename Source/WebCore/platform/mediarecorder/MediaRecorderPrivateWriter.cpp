/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "MediaRecorderPrivateWriter.h"

#if ENABLE(MEDIA_RECORDER)

#include "ContentType.h"
#include "MediaRecorderPrivateOptions.h"
#include "MediaRecorderPrivateWriterCocoa.h"
#include "MediaRecorderPrivateWriterWebM.h"

namespace WebCore {

RefPtr<MediaRecorderPrivateWriter> MediaRecorderPrivateWriter::create(bool hasAudio, bool hasVideo, const MediaRecorderPrivateOptions& options)
{
#if PLATFORM(COCOA)
    ContentType mimeType(options.mimeType);
    auto containerType = mimeType.containerType();
    Ref writer =
#if ENABLE(MEDIA_RECORDER_WEBM)
    (equalLettersIgnoringASCIICase(containerType, "audio/webm"_s) || equalLettersIgnoringASCIICase(containerType, "video/webm"_s)) ? MediaRecorderPrivateWriterWebM::create(hasAudio, hasVideo) :
#endif
    MediaRecorderPrivateWriterAVFObjC::create(hasAudio, hasVideo);

    if (!writer->initialize(options))
        return nullptr;
    return writer;
#else
    UNUSED_VARIABLE(hasAudio);
    UNUSED_VARIABLE(hasVideo);
    UNUSED_VARIABLE(options);
    return nullptr;
#endif
}

} // namespae WebCore

#endif // ENABLE(MEDIA_RECORDER)
