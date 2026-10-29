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

#include "config.h"
#include "PrintProcessProxy.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PrintProcessProxy);

Ref<PrintProcessProxy> PrintProcessProxy::create(WebProcessPool& processPool, WebProcessProxy& printProcess)
{
    return adoptRef(*new PrintProcessProxy(processPool, printProcess));
}

PrintProcessProxy::PrintProcessProxy(WebProcessPool& processPool, WebProcessProxy& printProcess)
    : m_processPool(processPool)
    , m_process(printProcess)
    , m_lockdownMode(printProcess.lockdownMode())
{
    ASSERT(printProcess.isPrintProcess());
}

PrintProcessProxy::~PrintProcessProxy() = default;

// So that the print process does not exit while the drawing is pending.
template<typename... Arguments>
CompletionHandler<void(Arguments...)> PrintProcessProxy::keepingPrintProcess(CompletionHandler<void(Arguments...)>&& completionHandler)
{
    RefPtr processPool = m_processPool;
    if (!processPool)
        return WTF::move(completionHandler);
    return [completionHandler = WTF::move(completionHandler), useToken = processPool->ensurePrintProcess(m_lockdownMode)](Arguments... arguments) mutable {
        completionHandler(std::forward<Arguments>(arguments)...);
    };
}

std::optional<IPC::Connection::AsyncReplyID> PrintProcessProxy::sinkCompletedSnapshotToPDF(RemoteSnapshotIdentifier identifier, WebCore::FrameIdentifier rootFrameIdentifier, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    RefPtr process = m_process;
    if (!process) {
        completionHandler(nullptr);
        return std::nullopt;
    }
    return process->sendWithAsyncReply(Messages::WebProcess::SinkCompletedPrintSnapshotToPDF(identifier, rootFrameIdentifier), keepingPrintProcess(WTF::move(completionHandler)));
}

std::optional<IPC::Connection::AsyncReplyID> PrintProcessProxy::sinkCompletedSnapshotToBitmap(RemoteSnapshotIdentifier identifier, WebCore::FrameIdentifier rootFrameIdentifier, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&& completionHandler)
{
    RefPtr process = m_process;
    if (!process) {
        completionHandler(std::nullopt);
        return std::nullopt;
    }
    return process->sendWithAsyncReply(Messages::WebProcess::SinkCompletedPrintSnapshotToBitmap(identifier, rootFrameIdentifier), keepingPrintProcess(WTF::move(completionHandler)));
}

void PrintProcessProxy::releaseSnapshot(RemoteSnapshotIdentifier identifier)
{
    if (RefPtr process = m_process)
        process->send(Messages::WebProcess::ReleasePrintSnapshot(identifier), 0);
}

void PrintProcessProxy::abandonSnapshotFrame(RemoteSnapshotIdentifier identifier, WebCore::FrameIdentifier frameIdentifier)
{
    if (RefPtr process = m_process)
        process->send(Messages::WebProcess::AbandonPrintSnapshotFrame(identifier, frameIdentifier), 0);
}

void PrintProcessProxy::snapshotFrameWillBeDrawnByProcess(RemoteSnapshotIdentifier identifier, WebCore::FrameIdentifier frameIdentifier, WebCore::ProcessIdentifier processIdentifier)
{
    if (RefPtr process = m_process)
        process->send(Messages::WebProcess::PrintSnapshotFrameWillBeDrawnByProcess(identifier, frameIdentifier, processIdentifier), 0);
}

void PrintProcessProxy::abandonSnapshotFramesOwnedBy(WebCore::ProcessIdentifier processIdentifier)
{
    if (RefPtr process = m_process)
        process->send(Messages::WebProcess::AbandonPrintSnapshotFramesOwnedBy(processIdentifier), 0);
}

CompletionHandler<void(bool)> PrintProcessProxy::releaseSnapshotIfRootFails(RemoteSnapshotIdentifier identifier)
{
    // The process rather than this, which may not be kept.
    return [weakProcess = m_process, identifier](bool success) {
        RefPtr process = weakProcess;
        if (!success && process && process->hasConnection())
            process->send(Messages::WebProcess::ReleasePrintSnapshot(identifier), 0);
    };
}

bool PrintProcessProxy::waitForSnapshot(RemoteSnapshotIdentifier identifier, Seconds timeout)
{
    RefPtr process = m_process;
    if (!process)
        return false;
    // As GPUProcessProxy::waitForSnapshot(): synchronous, so that the messages that ask the remaining
    // frames to record are dispatched meanwhile, and kept behind the sink, so that the sink's reply is
    // sent first. The print process replies once the snapshot is complete.
    return process->sendSync(Messages::WebProcess::WaitForPrintSnapshot(identifier), 0, timeout, IPC::SendSyncOption::MaintainOrderingWithAsyncMessages).succeeded();
}

Function<RefPtr<IPC::Connection>(Seconds)> PrintProcessProxy::snapshotHostConnectionWaiter()
{
    return [process = m_process](Seconds launchTimeout) -> RefPtr<IPC::Connection> {
        RefPtr protectedProcess = process;
        if (!protectedProcess || !protectedProcess->waitForLaunch(launchTimeout))
            return nullptr;
        return &protectedProcess->connection();
    };
}

Function<void(Seconds)> PrintProcessProxy::snapshotWaiter(RemoteSnapshotIdentifier identifier)
{
    return [processPool = m_processPool, process = m_process, identifier](Seconds timeout) {
        RefPtr protectedProcessPool = processPool;
        RefPtr protectedProcess = process;
        if (!protectedProcessPool || !protectedProcess)
            return;
        Ref printProcessProxy = PrintProcessProxy::create(*protectedProcessPool, *protectedProcess);
        if (!printProcessProxy->waitForSnapshot(identifier, timeout))
            printProcessProxy->releaseSnapshot(identifier);
    };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
