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

#if ENABLE(GPU_PROCESS) && ENABLE(MODEL_PROCESS)

#include "Connection.h"
#include "ImageBufferSetIdentifier.h"
#include "RemoteRenderingBackendIdentifier.h"
#include <WebCore/ProcessIdentifier.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UUID.h>

#if HAVE(IOSURFACE)
#include <wtf/MachSendRight.h>
#endif

namespace WebKit {

class GPUProcess;

class GPUConnectionToModelProcess final
    : public ThreadSafeRefCounted<GPUConnectionToModelProcess, WTF::DestructionThread::Main>
    , public IPC::Connection::Client {
    WTF_MAKE_NONCOPYABLE(GPUConnectionToModelProcess);
    WTF_MAKE_TZONE_ALLOCATED(GPUConnectionToModelProcess);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(GPUConnectionToModelProcess);
public:
    static Ref<GPUConnectionToModelProcess> create(GPUProcess&, WebCore::ProcessIdentifier, IPC::Connection::Handle&&);
    ~GPUConnectionToModelProcess();

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    IPC::Connection& connection() { return m_connection.get(); }
    WebCore::ProcessIdentifier webProcessIdentifier() const { return m_webProcessIdentifier; }

private:
    GPUConnectionToModelProcess(GPUProcess&, WebCore::ProcessIdentifier, IPC::Connection::Handle&&);

    // IPC::Connection::Client
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, const Vector<uint32_t>& indicesOfObjectsFailingDecoding) final;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

#if ENABLE(SPATIAL_PORTAL) && HAVE(IOSURFACE)
    void frontBufferForExternalRead(RemoteRenderingBackendIdentifier, ImageBufferSetIdentifier, WTF::UUID externalReadToken, Vector<uint64_t>&& alreadyMappedBuffers, CompletionHandler<void(std::optional<MachSendRight>&&, uint64_t bufferIdentifier, bool isBusy)>&&);
#endif

    const Ref<IPC::Connection> m_connection;
    const Ref<GPUProcess> m_gpuProcess;
    const WebCore::ProcessIdentifier m_webProcessIdentifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MODEL_PROCESS)
