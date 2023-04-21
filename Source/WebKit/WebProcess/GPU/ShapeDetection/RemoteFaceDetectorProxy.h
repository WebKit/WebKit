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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "RenderingBackendIdentifier.h"
#include "ShapeDetectionIdentifier.h"
#include <WebCore/FaceDetectorInterface.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class StreamClientConnection;
}

namespace WebCore::ShapeDetection {
struct DetectedFace;
struct FaceDetectorOptions;
}

namespace WebKit::ShapeDetection {

class RemoteFaceDetectorProxy : public WebCore::ShapeDetection::FaceDetector {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteFaceDetectorProxy> create(Ref<IPC::StreamClientConnection>&&, RenderingBackendIdentifier, ShapeDetectionIdentifier, const WebCore::ShapeDetection::FaceDetectorOptions&);

    virtual ~RemoteFaceDetectorProxy();

private:
    RemoteFaceDetectorProxy(Ref<IPC::StreamClientConnection>&&, RenderingBackendIdentifier, ShapeDetectionIdentifier);

    RemoteFaceDetectorProxy(const RemoteFaceDetectorProxy&) = delete;
    RemoteFaceDetectorProxy(RemoteFaceDetectorProxy&&) = delete;
    RemoteFaceDetectorProxy& operator=(const RemoteFaceDetectorProxy&) = delete;
    RemoteFaceDetectorProxy& operator=(RemoteFaceDetectorProxy&&) = delete;

    ShapeDetectionIdentifier backing() const { return m_backing; }

    void detect(Ref<WebCore::ImageBuffer>&&, CompletionHandler<void(Vector<WebCore::ShapeDetection::DetectedFace>&&)>&&) final;

    ShapeDetectionIdentifier m_backing;
    Ref<IPC::StreamClientConnection> m_streamClientConnection;
    RenderingBackendIdentifier m_renderingBackendIdentifier;
};

} // namespace WebKit::ShapeDetection

#endif // ENABLE(GPU_PROCESS)
