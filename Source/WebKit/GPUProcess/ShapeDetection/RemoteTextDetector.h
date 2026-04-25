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
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakRef.h>

namespace WebCore::ShapeDetection {
struct DetectedText;
class TextDetector;
}

namespace WebKit {
class RemoteRenderingBackend;
struct SharedPreferencesForWebProcess;

namespace ShapeDetection {
class ObjectHeap;
}

class RemoteTextDetector : public IPC::StreamMessageReceiver {
public:
    WTF_MAKE_TZONE_ALLOCATED(RemoteTextDetector);
    WTF_MAKE_NONCOPYABLE(RemoteTextDetector);
public:
    static Ref<RemoteTextDetector> create(Ref<WebCore::ShapeDetection::TextDetector>&& textDetector, RemoteRenderingBackend& renderingBackend, ShapeDetectionIdentifier identifier)
    {
        return adoptRef(*new RemoteTextDetector(WTF::move(textDetector), renderingBackend, identifier));
    }

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

    virtual ~RemoteTextDetector();

private:
    RemoteTextDetector(Ref<WebCore::ShapeDetection::TextDetector>&&, RemoteRenderingBackend&, ShapeDetectionIdentifier);

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void detect(WebCore::RenderingResourceIdentifier, CompletionHandler<void(Vector<WebCore::ShapeDetection::DetectedText>&&)>&&);

    const Ref<WebCore::ShapeDetection::TextDetector> m_backing;
    const WeakRef<RemoteRenderingBackend> m_renderingBackend;
    const ShapeDetectionIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
