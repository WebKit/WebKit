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

#include "GPUProcessProxy.h"
#include "RemoteSnapshotIdentifier.h"
#include "WebProcessProxy.h"
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

// A drawing that a caller may have to block the main thread for, as UIKit printing does. With remote
// snapshotting, frames hosted in other processes are asked to record by way of this process, so waiting
// for the drawing means waiting on the GPU process in a way that still dispatches those requests.
class PendingSnapshotDrawing : public RefCounted<PendingSnapshotDrawing> {
    WTF_MAKE_TZONE_ALLOCATED(PendingSnapshotDrawing);
public:
    static Ref<PendingSnapshotDrawing> create(std::optional<RemoteSnapshotIdentifier> snapshotIdentifier = std::nullopt)
    {
        return adoptRef(*new PendingSnapshotDrawing(snapshotIdentifier));
    }

    // Blocks until the drawing that was handed replyID has completed and its completion handler has
    // run, or has given up.
    static void wait(IPC::AsyncReplyID replyID)
    {
        if (RefPtr drawing = drawings().get(replyID))
            drawing->wait();
    }

    // Without remote snapshotting, the process painting the frame replies with the drawing itself.
    template<typename M>
    void setReply(WebProcessProxy& process, IPC::AsyncReplyID replyID)
    {
        if (m_isComplete)
            return;
        m_waitForReply = [weakProcess = WeakPtr { process }, replyID] {
            RefPtr process = weakProcess.get();
            if (process && process->hasConnection())
                protect(process->connection())->waitForAsyncReplyAndDispatchImmediately<M>(replyID, Seconds::infinity());
        };
        add(replyID);
    }

    // With remote snapshotting, the GPU process replies with the drawing once the snapshot is complete.
    // Identified to the caller by the root's reply, which comes before.
    template<typename M>
    void setReply(GPUProcessProxy& gpuProcess, std::optional<IPC::AsyncReplyID> sinkReplyID, IPC::AsyncReplyID rootReplyID)
    {
        if (!sinkReplyID || m_isComplete)
            return;
        m_waitForReply = [weakGPUProcess = WeakPtr { gpuProcess }, sinkReplyID = *sinkReplyID] {
            RefPtr gpuProcess = weakGPUProcess.get();
            if (gpuProcess && gpuProcess->hasConnection())
                protect(gpuProcess->connection())->waitForAsyncReplyAndDispatchImmediately<M>(sinkReplyID, replyTimeout);
        };
        add(rootReplyID);
    }

    template<typename... Arguments>
    CompletionHandler<void(Arguments...)> completionHandler(CompletionHandler<void(Arguments...)>&& completionHandler)
    {
        return [protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)](Arguments... arguments) mutable {
            protectedThis->didComplete();
            completionHandler(std::forward<Arguments>(arguments)...);
        };
    }

private:
    // Longer than the GPU process takes to give up on frames that do not come.
    static constexpr Seconds snapshotTimeout = 15_s;
    // Sent before the GPU process answered the wait, so it has already come in.
    static constexpr Seconds replyTimeout = 1_s;

    explicit PendingSnapshotDrawing(std::optional<RemoteSnapshotIdentifier> snapshotIdentifier)
        : m_snapshotIdentifier(snapshotIdentifier)
    {
    }

    static HashMap<IPC::AsyncReplyID, Ref<PendingSnapshotDrawing>>& drawings()
    {
        ASSERT(RunLoop::isMain());
        static NeverDestroyed<HashMap<IPC::AsyncReplyID, Ref<PendingSnapshotDrawing>>> drawings;
        return drawings;
    }

    void add(IPC::AsyncReplyID replyID)
    {
        m_replyID = replyID;
        drawings().add(replyID, *this);
    }

    void wait()
    {
        Ref protectedThis { *this };
        if (m_isComplete)
            return;

        if (m_snapshotIdentifier) {
            RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated();
            if (!gpuProcess)
                return;
            if (!gpuProcess->waitForSnapshot(*m_snapshotIdentifier, snapshotTimeout))
                gpuProcess->releaseSnapshot(*m_snapshotIdentifier);
        }

        if (auto waitForReply = std::exchange(m_waitForReply, nullptr))
            waitForReply();
    }

    void didComplete()
    {
        m_isComplete = true;
        m_waitForReply = nullptr;
        if (m_replyID)
            drawings().remove(*m_replyID);
    }

    const std::optional<RemoteSnapshotIdentifier> m_snapshotIdentifier;
    std::optional<IPC::AsyncReplyID> m_replyID;
    Function<void()> m_waitForReply;
    bool m_isComplete { false };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
