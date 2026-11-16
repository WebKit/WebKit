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
#include "RemoteSnapshotPDFDrawingTask.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#include "DisplayListData.h"
#include "GPUConnectionToWebProcess.h"
#include "GPUProcess.h"
#include "GPUProcessConnectionMessages.h"
#include "Logging.h"
#include "RemoteSnapshot.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSnapshotPDFDrawingTask);

static constexpr Seconds printProcessReplyTimeout = 500_ms;

uint64_t RemoteSnapshotPDFDrawingTask::s_drawnInPrintProcessCount { 0 };
uint64_t RemoteSnapshotPDFDrawingTask::s_drawnAfterPrintProcessFailedCount { 0 };

Ref<RemoteSnapshotPDFDrawingTask> RemoteSnapshotPDFDrawingTask::create(Ref<RemoteSnapshot>&& snapshot, FrameIdentifier rootFrameIdentifier, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
    return adoptRef(*new RemoteSnapshotPDFDrawingTask(WTF::move(snapshot), rootFrameIdentifier, WTF::move(completionHandler)));
}

RemoteSnapshotPDFDrawingTask::RemoteSnapshotPDFDrawingTask(Ref<RemoteSnapshot>&& snapshot, FrameIdentifier rootFrameIdentifier, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
    : m_snapshot(WTF::move(snapshot))
    , m_rootFrameIdentifier(rootFrameIdentifier)
    , m_completionHandler(WTF::move(completionHandler))
{
}

RemoteSnapshotPDFDrawingTask::~RemoteSnapshotPDFDrawingTask() = default;

void RemoteSnapshotPDFDrawingTask::start(std::optional<ProcessIdentifier> printProcessIdentifier)
{
    if (!printProcessIdentifier) {
        drawLocally();
        return;
    }
    sendToPrintProcess(*printProcessIdentifier);
}

void RemoteSnapshotPDFDrawingTask::sendToPrintProcess(ProcessIdentifier printProcessIdentifier)
{
    RefPtr snapshot = m_snapshot;
    RefPtr connection = protect(GPUProcess::singleton())->webProcessConnection(printProcessIdentifier);
    auto displayListData = connection ? snapshot->createDisplayListData(m_rootFrameIdentifier) : std::nullopt;
    if (!displayListData) {
        fallback();
        return;
    }

    protect(connection->connection())->sendWithAsyncReply(Messages::GPUProcessConnection::DrawDisplayListToPDF(WTF::move(*displayListData), snapshot->size()), [protectedThis = Ref { *this }](RefPtr<SharedBuffer>&& pdf) {
        if (pdf)
            protectedThis->complete(pdf.releaseNonNull());
        else
            protectedThis->fallback();
    }, 0);
    RunLoop::mainSingleton().dispatchAfter(printProcessReplyTimeout, [protectedThis = Ref { *this }] {
        protectedThis->fallback();
    });
}

void RemoteSnapshotPDFDrawingTask::complete(Ref<SharedBuffer>&& pdf)
{
    if (isFinished())
        return;

    ++s_drawnInPrintProcessCount;
    m_snapshot = nullptr;
    m_completionHandler(WTF::move(pdf));
}

void RemoteSnapshotPDFDrawingTask::fallback()
{
    if (isFinished())
        return;

    RELEASE_LOG_ERROR(Process, "%p - RemoteSnapshotPDFDrawingTask::fallback: The print process did not draw the snapshot, so the GPU process draws it", this);
    ++s_drawnAfterPrintProcessFailedCount;
    drawLocally();
}

void RemoteSnapshotPDFDrawingTask::drawLocally()
{
    RefPtr snapshot = std::exchange(m_snapshot, nullptr);
    auto result = snapshot->drawToPDF(snapshot->size(), m_rootFrameIdentifier);
    if (!result) {
        ASSERT_NOT_REACHED();
        m_completionHandler({ });
        return;
    }
    m_completionHandler(WTF::move(*result));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
