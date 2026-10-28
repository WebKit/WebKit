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

#include "RemoteSnapshotHostProxy.h"
#include "WebProcessProxy.h"
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class WebProcessPool;

// The snapshot API of the print process with a Lockdown Mode, which assembles print snapshots as the
// GPU process assembles the others. Given out by WebProcessPool::printProcessProxy() while the print
// process runs. A drawing keeps the print process from exiting until it is replied to.
class PrintProcessProxy final : public RefCounted<PrintProcessProxy>, public RemoteSnapshotHostProxy {
    WTF_MAKE_TZONE_ALLOCATED(PrintProcessProxy);
public:
    static Ref<PrintProcessProxy> create(WebProcessPool&, WebProcessProxy& printProcess);
    ~PrintProcessProxy();

    WebProcessProxy* process() const { return m_process.get(); }

    // RemoteSnapshotHostProxy.
    std::optional<IPC::Connection::AsyncReplyID> sinkCompletedSnapshotToPDF(RemoteSnapshotIdentifier, WebCore::FrameIdentifier root, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&) final;
    std::optional<IPC::Connection::AsyncReplyID> sinkCompletedSnapshotToBitmap(RemoteSnapshotIdentifier, WebCore::FrameIdentifier root, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&&) final;
    void releaseSnapshot(RemoteSnapshotIdentifier) final;
    void abandonSnapshotFrame(RemoteSnapshotIdentifier, WebCore::FrameIdentifier) final;
    void snapshotFrameWillBeDrawnByProcess(RemoteSnapshotIdentifier, WebCore::FrameIdentifier, WebCore::ProcessIdentifier) final;
    void abandonSnapshotFramesOwnedBy(WebCore::ProcessIdentifier) final;
    CompletionHandler<void(bool)> releaseSnapshotIfRootFails(RemoteSnapshotIdentifier) final;
    bool waitForSnapshot(RemoteSnapshotIdentifier, Seconds timeout) final;
    RefPtr<IPC::Connection> snapshotHostConnection() const final;
    Function<void(Seconds timeout)> snapshotWaiter(RemoteSnapshotIdentifier) final;

private:
    PrintProcessProxy(WebProcessPool&, WebProcessProxy& printProcess);

    template<typename... Arguments>
    CompletionHandler<void(Arguments...)> keepingPrintProcess(CompletionHandler<void(Arguments...)>&&);

    const WeakPtr<WebProcessPool> m_processPool;
    const WeakPtr<WebProcessProxy> m_process;
    const WebProcessProxy::LockdownMode m_lockdownMode;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
