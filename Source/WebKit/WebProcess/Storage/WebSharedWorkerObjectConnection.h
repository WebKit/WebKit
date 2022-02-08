/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "MessageSender.h"
#include <WebCore/SharedWorkerObjectConnection.h>

namespace WebKit {

class WebSharedWorkerObjectConnection final : public WebCore::SharedWorkerObjectConnection, private IPC::MessageSender, public IPC::MessageReceiver {
public:
    static Ref<WebSharedWorkerObjectConnection> create() { return adoptRef(*new WebSharedWorkerObjectConnection); }
    ~WebSharedWorkerObjectConnection();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    WebSharedWorkerObjectConnection();

    // WebCore::SharedWorkerObjectConnection.
    void requestSharedWorker(const WebCore::SharedWorkerKey&, WebCore::SharedWorkerObjectIdentifier, WebCore::TransferredMessagePort&&, const WebCore::WorkerOptions&) final;
    void sharedWorkerObjectIsGoingAway(const WebCore::SharedWorkerKey&, WebCore::SharedWorkerObjectIdentifier) final;

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final { return 0; }
};

} // namespace WebKit
