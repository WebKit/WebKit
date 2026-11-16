/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#include <WebCore/FrameIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <wtf/CompletionHandler.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class SharedBuffer;
}

namespace WebKit {

class RemoteSnapshot;

class RemoteSnapshotPDFDrawingTask : public RefCounted<RemoteSnapshotPDFDrawingTask> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteSnapshotPDFDrawingTask);
public:
    static Ref<RemoteSnapshotPDFDrawingTask> create(Ref<RemoteSnapshot>&&, WebCore::FrameIdentifier rootFrameIdentifier, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&);
    ~RemoteSnapshotPDFDrawingTask();

    void start(std::optional<WebCore::ProcessIdentifier> printProcessIdentifier);

    static uint64_t drawnInPrintProcessCountForTesting() { return s_drawnInPrintProcessCount; }
    static uint64_t drawnAfterPrintProcessFailedCountForTesting() { return s_drawnAfterPrintProcessFailedCount; }

private:
    RemoteSnapshotPDFDrawingTask(Ref<RemoteSnapshot>&&, WebCore::FrameIdentifier rootFrameIdentifier, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&);

    void sendToPrintProcess(WebCore::ProcessIdentifier);
    void complete(Ref<WebCore::SharedBuffer>&&);
    void fallback();
    void drawLocally();
    bool isFinished() const { return !m_snapshot; }

    RefPtr<RemoteSnapshot> m_snapshot;
    WebCore::FrameIdentifier m_rootFrameIdentifier;
    CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)> m_completionHandler;

    static uint64_t s_drawnInPrintProcessCount;
    static uint64_t s_drawnAfterPrintProcessFailedCount;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
