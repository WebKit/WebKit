

/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)

#include "MediaRecorderIdentifier.h"
#include "MessageReceiver.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
struct ExceptionData;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteMediaRecorder;

class RemoteMediaRecorderManager : private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RemoteMediaRecorderManager(GPUConnectionToWebProcess&);
    ~RemoteMediaRecorderManager();

    void didReceiveRemoteMediaRecorderMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void createRecorder(MediaRecorderIdentifier, bool recordAudio, int width, int height, CompletionHandler<void(Optional<WebCore::ExceptionData>&&)>&&);
    void releaseRecorder(MediaRecorderIdentifier);

    GPUConnectionToWebProcess& m_gpuConnectionToWebProcess;
    HashMap<MediaRecorderIdentifier, std::unique_ptr<RemoteMediaRecorder>> m_recorders;
};

}

#endif
