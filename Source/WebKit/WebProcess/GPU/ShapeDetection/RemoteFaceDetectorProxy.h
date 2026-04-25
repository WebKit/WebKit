/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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

#include "ShapeDetectionIdentifier.h"
#include <WebCore/FaceDetectorInterface.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Identified.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

#if ENABLE(GPU_PROCESS)

namespace IPC {
class StreamClientConnection;
}

namespace WebCore::ShapeDetection {
struct DetectedFace;
}

namespace WebKit {
class RemoteRenderingBackendProxy;
}

namespace WebKit::ShapeDetection {

class RemoteFaceDetectorProxy : public WebCore::ShapeDetection::FaceDetector, public Identified<ShapeDetectionIdentifier> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteFaceDetectorProxy);
    WTF_MAKE_NONCOPYABLE(RemoteFaceDetectorProxy);
public:
    static Ref<RemoteFaceDetectorProxy> create(RemoteRenderingBackendProxy&);
    virtual ~RemoteFaceDetectorProxy();

private:
    RemoteFaceDetectorProxy(RemoteRenderingBackendProxy&);
    void detect(const WebCore::NativeImage&, CompletionHandler<void(Vector<WebCore::ShapeDetection::DetectedFace>&&)>&&) final;

    WeakPtr<RemoteRenderingBackendProxy> m_renderingBackend;
};

} // namespace WebKit::ShapeDetection

#endif // ENABLE(GPU_PROCESS)
