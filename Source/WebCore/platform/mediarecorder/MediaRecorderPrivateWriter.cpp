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

#include "MediaRecorderPrivateWriterAVFObjC.h"
#include "MediaRecorderPrivateWriterWebM.h"
#include "MediaSample.h"
#include "MediaStrategy.h"
#include "PlatformStrategies.h"
#include <wtf/MediaTime.h>
#include <wtf/NativePromise.h>

namespace WebCore {

MediaRecorderPrivateWriter::MediaRecorderPrivateWriter() = default;
MediaRecorderPrivateWriter::~MediaRecorderPrivateWriter() = default;

std::unique_ptr<MediaRecorderPrivateWriter> MediaRecorderPrivateWriter::create(String type, MediaRecorderPrivateWriterListener& listener)
{
    auto containerType = [](const String& type) -> std::optional<MediaRecorderContainerType> {
        if (equalLettersIgnoringASCIICase(type, "video/mp4"_s) || equalLettersIgnoringASCIICase(type, "audio/mp4"_s))
            return MediaRecorderContainerType::Mp4;
#if ENABLE(MEDIA_RECORDER_WEBM)
        if (equalLettersIgnoringASCIICase(type, "video/webm"_s) || equalLettersIgnoringASCIICase(type, "audio/webm"_s))
            return MediaRecorderContainerType::WebM;
#endif
        return { };
    }(type);
    if (!containerType)
        return nullptr;

    return create(*containerType, listener);
}

std::unique_ptr<MediaRecorderPrivateWriter> MediaRecorderPrivateWriter::create(MediaRecorderContainerType type, MediaRecorderPrivateWriterListener& listener)
{
    switch (type) {
    case MediaRecorderContainerType::Mp4:
        return MediaRecorderPrivateWriterAVFObjC::create(listener);
#if ENABLE(MEDIA_RECORDER_WEBM)
    case MediaRecorderContainerType::WebM:
        return MediaRecorderPrivateWriterWebM::create(listener);
#endif
    default:
        return nullptr;
    }
}

Ref<MediaRecorderPrivateWriter::WriterPromise> MediaRecorderPrivateWriter::writeFrames(Deque<UniqueRef<MediaSamplesBlock>>&& samples, const MediaTime& endTime)
{
    while (!samples.isEmpty())
        m_pendingFrames.append(samples.takeFirst());

    auto result = Result::Success;
    while (!m_pendingFrames.isEmpty() && result == Result::Success)
        result = writeFrame(m_pendingFrames.takeFirst().get());

    // End the segment if we succeded in writing all frames, otherwise we will retry them on the next call.
    if (m_pendingFrames.isEmpty())
        forceNewSegment(endTime);

    m_lastEndTime = endTime;

    return result == Result::Success ? WriterPromise::createAndResolve() : WriterPromise::createAndReject(result);
}

Ref<GenericPromise> MediaRecorderPrivateWriter::close()
{
    ASSERT(m_lastEndTime.isValid(), "writeFrames must have been called once");

    if (!m_pendingFrames.isEmpty())
        writeFrames({ }, m_lastEndTime); // Attempt one last time to write the frames we do have.

    m_pendingFrames.clear();
    return close(m_lastEndTime);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_RECORDER)
