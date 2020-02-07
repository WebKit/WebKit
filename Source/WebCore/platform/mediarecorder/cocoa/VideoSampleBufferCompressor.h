/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#include <CoreMedia/CoreMedia.h>
#include <VideoToolbox/VTErrors.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef struct OpaqueVTCompressionSession *VTCompressionSessionRef;

namespace WebCore {

class VideoSampleBufferCompressor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<VideoSampleBufferCompressor> create(CMVideoCodecType, CMBufferQueueTriggerCallback, void* callbackObject);
    ~VideoSampleBufferCompressor();

    void finish();
    void addSampleBuffer(CMSampleBufferRef);
    CMSampleBufferRef getOutputSampleBuffer();
    RetainPtr<CMSampleBufferRef> takeOutputSampleBuffer();

private:
    explicit VideoSampleBufferCompressor(CMVideoCodecType);

    bool initialize(CMBufferQueueTriggerCallback, void* callbackObject);

    void processSampleBuffer(CMSampleBufferRef);
    bool initCompressionSession(CMVideoFormatDescriptionRef);

    static void videoCompressionCallback(void *refCon, void*, OSStatus, VTEncodeInfoFlags, CMSampleBufferRef);

    dispatch_queue_t m_serialDispatchQueue;
    RetainPtr<CMBufferQueueRef> m_outputBufferQueue;
    RetainPtr<VTCompressionSessionRef> m_vtSession;

    bool m_isEncoding { false };

    CMVideoCodecType m_outputCodecType;
    float m_maxKeyFrameIntervalDuration { 2.0 };
    unsigned m_expectedFrameRate { 30 };
};

}

#endif
