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
#include "RemoteDisplayListRecorderIdentifier.h"
#include "RemoteGraphicsContext.h"
#include <WebCore/DisplayListRecorderImpl.h>

namespace WebKit {

// RemoteGraphicsContext playing back the IPC GraphicsContext drawing commands to a DisplayList::Recorder.
// Used to create DisplayList instances.
class RemoteDisplayListRecorder final : public RemoteGraphicsContext {
public:
    static Ref<RemoteDisplayListRecorder> create(RemoteDisplayListRecorderIdentifier, RemoteRenderingBackend&);
    ~RemoteDisplayListRecorder();
    void stopListeningForIPC();

    Ref<const WebCore::DisplayList::DisplayList> takeDisplayList() { return m_recorder->takeDisplayList(); }

private:
    RemoteDisplayListRecorder(UniqueRef<WebCore::DisplayList::RecorderImpl>&&, RemoteDisplayListRecorderIdentifier, RemoteRenderingBackend&);
    void startListeningForIPC();

    UniqueRef<WebCore::DisplayList::RecorderImpl> m_recorder;
    const RemoteDisplayListRecorderIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
