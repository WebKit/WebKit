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
#include "RemoteSnapshotHostProxy.h"
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
// for the drawing means waiting on the snapshot host, the GPU process or the print process, in a way
// that still dispatches those requests.
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
        m_waitForReply = [weakProcess = WeakPtr { process }, replyID](IPC::Connection*) {
            RefPtr process = weakProcess.get();
            if (process && process->hasConnection())
                protect(process->connection())->waitForAsyncReplyAndDispatchImmediately<M>(replyID, Seconds::infinity());
        };
        add(replyID);
    }

    // With remote snapshotting, the snapshot host replies with the drawing once the snapshot is complete.
    // Identified to the caller by replyID, such as the reply of the root, which comes before. The host
    // may still be launching, which the wait waits for first, since processes finish launching on the
    // main thread that the wait blocks.
    template<typename M>
    void setReply(RemoteSnapshotHostProxy& host, std::optional<IPC::AsyncReplyID> sinkReplyID, IPC::AsyncReplyID replyID)
    {
        if (!sinkReplyID || m_isComplete)
            return;
        m_snapshotHostConnection = host.snapshotHostConnectionWaiter();
        if (m_snapshotIdentifier)
            m_waitForSnapshot = host.snapshotWaiter(*m_snapshotIdentifier);
        m_waitForReply = [sinkReplyID = *sinkReplyID](IPC::Connection* connection) {
            if (connection)
                connection->waitForAsyncReplyAndDispatchImmediately<M>(sinkReplyID, replyTimeout);
        };
        add(replyID);
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
    static constexpr Seconds launchTimeout = 15_s;

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
        if (m_replyIDs.contains(replyID))
            return;
        m_replyIDs.append(replyID);
        drawings().add(replyID, *this);
    }

    void wait()
    {
        Ref protectedThis { *this };
        if (m_isComplete)
            return;

        RefPtr<IPC::Connection> snapshotHostConnection;
        if (auto getSnapshotHostConnection = std::exchange(m_snapshotHostConnection, nullptr))
            snapshotHostConnection = getSnapshotHostConnection(launchTimeout);

        if (auto waitForSnapshot = std::exchange(m_waitForSnapshot, nullptr))
            waitForSnapshot(snapshotTimeout);

        if (auto waitForReply = std::exchange(m_waitForReply, nullptr))
            waitForReply(snapshotHostConnection.get());
    }

    void didComplete()
    {
        m_isComplete = true;
        m_snapshotHostConnection = nullptr;
        m_waitForSnapshot = nullptr;
        m_waitForReply = nullptr;
        for (auto replyID : std::exchange(m_replyIDs, { })) {
            auto iterator = drawings().find(replyID);
            if (iterator != drawings().end() && iterator->value.ptr() == this)
                drawings().remove(iterator);
        }
    }

    const std::optional<RemoteSnapshotIdentifier> m_snapshotIdentifier;
    Vector<IPC::AsyncReplyID> m_replyIDs;
    Function<RefPtr<IPC::Connection>(Seconds launchTimeout)> m_snapshotHostConnection;
    Function<void(Seconds)> m_waitForSnapshot;
    Function<void(IPC::Connection*)> m_waitForReply;
    bool m_isComplete { false };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
