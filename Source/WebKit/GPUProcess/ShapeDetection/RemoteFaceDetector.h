/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "Connection.h"
#include "ShapeDetectionIdentifier.h"
#include "StreamMessageReceiver.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>

namespace WebCore::ShapeDetection {
struct DetectedFace;
class FaceDetector;
}

namespace WebKit {

namespace ShapeDetection {
class ObjectHeap;
}

class RemoteFaceDetector : public IPC::StreamMessageReceiver {
public:
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteFaceDetector> create(Ref<WebCore::ShapeDetection::FaceDetector>&& faceDetector, ShapeDetection::ObjectHeap& objectHeap, ShapeDetectionIdentifier identifier)
    {
        return adoptRef(*new RemoteFaceDetector(WTFMove(faceDetector), objectHeap, identifier));
    }

    virtual ~RemoteFaceDetector();

private:
    RemoteFaceDetector(Ref<WebCore::ShapeDetection::FaceDetector>&&, ShapeDetection::ObjectHeap&, ShapeDetectionIdentifier);

    RemoteFaceDetector(const RemoteFaceDetector&) = delete;
    RemoteFaceDetector(RemoteFaceDetector&&) = delete;
    RemoteFaceDetector& operator=(const RemoteFaceDetector&) = delete;
    RemoteFaceDetector& operator=(RemoteFaceDetector&&) = delete;

    WebCore::ShapeDetection::FaceDetector& backing() { return m_backing; }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void detect(CompletionHandler<void(Vector<WebCore::ShapeDetection::DetectedFace>&&)>&&);

    Ref<WebCore::ShapeDetection::FaceDetector> m_backing;
    ShapeDetection::ObjectHeap& m_objectHeap;
    ShapeDetectionIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
