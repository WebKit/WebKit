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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "RemoteDisplayListIdentifier.h"
#include "RemoteGraphicsContext.h"
#include "RemoteSnapshotRecorderIdentifier.h"
#include <WebCore/DisplayListRecorderImpl.h>

namespace WebKit {
class RemoteSnapshot;

// RemoteSnapshotRecorder is a display list recorder that can convert a snapshot subframe rendering into
// its own draw item. In other words, RemoteSnapshotRecorder has the right to source snapshot subframe
// renderings.
class RemoteSnapshotRecorder final : public RemoteGraphicsContext {
public:
    static Ref<RemoteSnapshotRecorder> create(RemoteSnapshotRecorderIdentifier, RemoteSnapshot&, RemoteRenderingBackend&);
    ~RemoteSnapshotRecorder();
    void stopListeningForIPC();
    Ref<RemoteSnapshot> NODELETE snapshot() const;
    Ref<const WebCore::DisplayList::DisplayList> takeDisplayList() { return m_recorder->takeDisplayList(); }

private:
    RemoteSnapshotRecorder(UniqueRef<WebCore::DisplayList::RecorderImpl>&&, RemoteSnapshotRecorderIdentifier, RemoteSnapshot&, RemoteRenderingBackend&);
    void startListeningForIPC();

    // RemoteGraphicsContext overrides.
    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    // Messages.
    void drawSnapshotFrame(WebCore::FrameIdentifier);

    const Ref<RemoteSnapshot> m_snapshot;
    UniqueRef<WebCore::DisplayList::RecorderImpl> m_recorder;
    const RemoteSnapshotRecorderIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
