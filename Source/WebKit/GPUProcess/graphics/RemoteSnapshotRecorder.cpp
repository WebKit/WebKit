/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)
#include "RemoteSnapshotRecorder.h"

#include "Logging.h"
#include "RemoteGraphicsContextMessages.h"
#include "RemoteSnapshot.h"
#include "RemoteSnapshotRecorderMessages.h"

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_renderingBackend->streamConnection());

namespace WebKit {
using namespace WebCore;

Ref<RemoteSnapshotRecorder> RemoteSnapshotRecorder::create(RemoteSnapshotRecorderIdentifier identifier, RemoteSnapshot& snapshot, RemoteRenderingBackend& renderingBackend)
{
    Ref instance = adoptRef(*new RemoteSnapshotRecorder(makeUniqueRef<DisplayList::RecorderImpl>(FloatSize { }), identifier, snapshot, renderingBackend));
    instance->startListeningForIPC();
    return instance;
}

RemoteSnapshotRecorder::RemoteSnapshotRecorder(UniqueRef<DisplayList::RecorderImpl>&& recorder, RemoteSnapshotRecorderIdentifier identifier, RemoteSnapshot& snapshot, RemoteRenderingBackend& renderingBackend)
    : RemoteGraphicsContext(recorder, renderingBackend)
    , m_snapshot(snapshot)
    , m_recorder(WTFMove(recorder))
    , m_identifier(identifier)
{
}

RemoteSnapshotRecorder::~RemoteSnapshotRecorder() = default;

void RemoteSnapshotRecorder::startListeningForIPC()
{
    m_renderingBackend->streamConnection().startReceivingMessages(*this, Messages::RemoteGraphicsContext::messageReceiverName(), m_identifier.toUInt64());
    m_renderingBackend->streamConnection().startReceivingMessages(*this, Messages::RemoteSnapshotRecorder::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteSnapshotRecorder::stopListeningForIPC()
{
    m_renderingBackend->streamConnection().stopReceivingMessages(Messages::RemoteGraphicsContext::messageReceiverName(), m_identifier.toUInt64());
    m_renderingBackend->streamConnection().stopReceivingMessages(Messages::RemoteSnapshotRecorder::messageReceiverName(), m_identifier.toUInt64());
}

Ref<RemoteSnapshot> RemoteSnapshotRecorder::snapshot() const
{
    return m_snapshot;
}

void RemoteSnapshotRecorder::drawSnapshotFrame(FrameIdentifier frameIdentifier)
{
    bool result = m_snapshot->addFrameReference(frameIdentifier);
    MESSAGE_CHECK(result);
    m_recorder->drawPlaceholder([snapshot = m_snapshot, frameIdentifier] (GraphicsContext& context) {
        bool result = snapshot->applyFrame(frameIdentifier, context);
        ASSERT_UNUSED(result, result); // Programming error, consistency checked with isComplete().
    });
}

}

#undef MESSAGE_CHECK

#endif
