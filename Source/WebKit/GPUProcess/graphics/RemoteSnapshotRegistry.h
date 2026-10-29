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

#if ENABLE(GPU_PROCESS)

#include "RemoteSnapshotIdentifier.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {
class FloatSize;
class SharedBuffer;
}

namespace WebKit {

class RemoteSnapshot;

// The snapshots of the process that assembles them from the frames that web processes record.
// Called from the main thread and from the work queues of the rendering backends that record.
class RemoteSnapshotRegistry final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteSnapshotRegistry> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteSnapshotRegistry);
    WTF_MAKE_NONCOPYABLE(RemoteSnapshotRegistry);
public:
    static Ref<RemoteSnapshotRegistry> create();
    ~RemoteSnapshotRegistry();

    // Created by the process painting its root, or by whoever asks to draw it if that comes first.
    // Neither can come after it is released. A frame recording into one that does not exist records
    // into nothing, so none can bring back a snapshot that was released.
    void createSnapshot(RemoteSnapshotIdentifier, WebCore::FrameIdentifier rootFrameIdentifier, const WebCore::FloatSize&, WebCore::ProcessIdentifier rootProcessIdentifier);
    // For a root that could not be recorded.
    void failSnapshot(RemoteSnapshotIdentifier);
    Ref<RemoteSnapshot> snapshotForRecorder(RemoteSnapshotIdentifier);
    RefPtr<RemoteSnapshot> snapshot(RemoteSnapshotIdentifier);
    void abandonSnapshotFrame(RemoteSnapshotIdentifier, WebCore::FrameIdentifier);
    void snapshotFrameWillBeDrawnByProcess(RemoteSnapshotIdentifier, WebCore::FrameIdentifier, WebCore::ProcessIdentifier);
    void abandonSnapshotFramesOwnedBy(WebCore::ProcessIdentifier);
    // Only the minting process can ask for a snapshot to be drawn.
    void removeSnapshotsForProcess(WebCore::ProcessIdentifier);
    void releaseSnapshot(RemoteSnapshotIdentifier);
    void waitForSnapshot(RemoteSnapshotIdentifier, CompletionHandler<void()>&&);

    // Takes the snapshot once it is complete, or passes null if it failed.
    void takeSnapshotWhenComplete(RemoteSnapshotIdentifier, WebCore::FrameIdentifier rootFrameIdentifier, CompletionHandler<void(RefPtr<RemoteSnapshot>&&)>&&);
#if PLATFORM(COCOA)
    void sinkCompletedSnapshotToPDF(RemoteSnapshotIdentifier, WebCore::FrameIdentifier rootFrameIdentifier, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&);
#endif
    void sinkCompletedSnapshotToBitmap(RemoteSnapshotIdentifier, WebCore::FrameIdentifier rootFrameIdentifier, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&&);

private:
    RemoteSnapshotRegistry();

    Ref<RemoteSnapshot> ensureSnapshot(RemoteSnapshotIdentifier, std::optional<WebCore::FrameIdentifier> rootFrameIdentifier);
    void snapshotDeadlineExpired(RemoteSnapshotIdentifier);

    // FIXME: This is a sign that we lack local id exchange protocol. The outcome is that
    // the protocols are insecure and prone to id guessing and content stuffing.
    Lock m_lock;
    HashMap<RemoteSnapshotIdentifier, Ref<RemoteSnapshot>> m_snapshots WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
