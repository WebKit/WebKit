/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "GPUProcessConnection.h"
#include "MessageReceiver.h"
#include "RemoteRemoteCommandListenerIdentifier.h"
#include <WebCore/RemoteCommandListener.h>
#include <wtf/Identified.h>
#include <wtf/TZoneMalloc.h>

namespace IPC {
class Connection;
}

namespace WebKit {

class GPUProcessConnection;
class WebProcess;

class RemoteRemoteCommandListener final
    : public WebCore::RemoteCommandListener
    , private Identified<RemoteRemoteCommandListenerIdentifier>
    , private GPUProcessConnection::Client
    , private IPC::MessageReceiver
    , public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteRemoteCommandListener> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteRemoteCommandListener);
public:
    static Ref<RemoteRemoteCommandListener> create(WebCore::RemoteCommandListenerClient&);
    explicit RemoteRemoteCommandListener(WebCore::RemoteCommandListenerClient&);
    ~RemoteRemoteCommandListener();

    void ref() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteRemoteCommandListener>::ref(); }
    void deref() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteRemoteCommandListener>::deref(); }
    ThreadSafeWeakPtrControlBlock& controlBlock() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteRemoteCommandListener>::controlBlock(); }

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    // Messages
    void didReceiveRemoteControlCommand(WebCore::PlatformMediaSession::RemoteControlCommandType, const WebCore::PlatformMediaSession::RemoteCommandArgument&);

    // WebCore::RemoteCommandListener
    void updateSupportedCommands() final;

    GPUProcessConnection& ensureGPUProcessConnection();

    WebCore::RemoteCommandListener::RemoteCommandsSet m_currentCommands;
    bool m_currentSupportSeeking { false };
    ThreadSafeWeakPtr<GPUProcessConnection> m_gpuProcessConnection;
};

}

#endif
