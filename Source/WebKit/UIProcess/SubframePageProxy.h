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
#include <WebCore/PageIdentifier.h>

namespace IPC {
class Connection;
class Decoder;
class Encoder;
}

namespace WebKit {

class WebFrameProxy;
class WebPageProxy;
class WebProcessProxy;

class SubframePageProxy : public IPC::MessageReceiver, public IPC::MessageSender {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SubframePageProxy(WebFrameProxy&, WebPageProxy&, WebProcessProxy&);
    ~SubframePageProxy();

private:
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    WebCore::PageIdentifier m_webPageID;
    Ref<WebProcessProxy> m_process;
    WeakPtr<WebFrameProxy> m_frame;
    WeakPtr<WebPageProxy> m_page;
};

}
