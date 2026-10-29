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

#include "Connection.h"
#include "RemoteSnapshotIdentifier.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>
#include <wtf/Seconds.h>

namespace WebCore {
class SharedBuffer;
}

namespace WebKit {

// The process that assembles a remote snapshot from the frames that web processes record into it: the
// GPU process, or the print process for printing.
class RemoteSnapshotHostProxy {
public:
    virtual ~RemoteSnapshotHostProxy() = default;

#if PLATFORM(COCOA)
    // Draws the snapshot at the size its root was recorded at, once it is complete. May be sent as soon
    // as the root has been asked for.
    virtual std::optional<IPC::Connection::AsyncReplyID> sinkCompletedSnapshotToPDF(RemoteSnapshotIdentifier, WebCore::FrameIdentifier root, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&) = 0;
#endif
    virtual std::optional<IPC::Connection::AsyncReplyID> sinkCompletedSnapshotToBitmap(RemoteSnapshotIdentifier, WebCore::FrameIdentifier root, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&&) = 0;
    virtual void releaseSnapshot(RemoteSnapshotIdentifier) = 0;
    virtual void abandonSnapshotFrame(RemoteSnapshotIdentifier, WebCore::FrameIdentifier) = 0;
    virtual void snapshotFrameWillBeDrawnByProcess(RemoteSnapshotIdentifier, WebCore::FrameIdentifier, WebCore::ProcessIdentifier) = 0;
    virtual void abandonSnapshotFramesOwnedBy(WebCore::ProcessIdentifier) = 0;
    // For the reply of the process asked to record a snapshot's root. Only failure matters: nothing more
    // is coming for the snapshot then. Sent after the sink, so that the sink is not left waiting.
    virtual CompletionHandler<void(bool)> releaseSnapshotIfRootFails(RemoteSnapshotIdentifier) = 0;
    // Blocks until a snapshot already sent to be drawn has been drawn, or has failed. Messages that ask
    // frames hosted in other processes to record into it are dispatched meanwhile. False on timeout.
    virtual bool waitForSnapshot(RemoteSnapshotIdentifier, Seconds timeout) = 0;

    // For PendingSnapshotDrawing, which may outlive this. Gets the connection that the replies of this
    // host come on, once the host has launched, waiting up to the timeout for that if it can.
    virtual Function<RefPtr<IPC::Connection>(Seconds launchTimeout)> snapshotHostConnectionWaiter() = 0;
    // Waits for the snapshot as waitForSnapshot() does, and releases it on timeout.
    virtual Function<void(Seconds timeout)> snapshotWaiter(RemoteSnapshotIdentifier) = 0;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
