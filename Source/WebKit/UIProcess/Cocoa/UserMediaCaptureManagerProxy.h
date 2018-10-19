/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "Connection.h"
#include "MessageReceiver.h"
#include "UserMediaCaptureManager.h"
#include "UserMediaCaptureManagerProxyMessages.h"
#include <WebCore/RealtimeMediaSource.h>

namespace WebKit {

class SharedMemory;
class WebProcessProxy;

class UserMediaCaptureManagerProxy : private IPC::MessageReceiver {
public:
    UserMediaCaptureManagerProxy(WebProcessProxy&);
    ~UserMediaCaptureManagerProxy();

    WebProcessProxy& process() const { return m_process; }

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) final;

    void createMediaSourceForCaptureDeviceWithConstraints(uint64_t id, const WebCore::CaptureDevice& deviceID, String&&, const WebCore::MediaConstraints&, bool& succeeded, String& invalidConstraints, WebCore::RealtimeMediaSourceSettings&);
    void startProducingData(uint64_t);
    void stopProducingData(uint64_t);
    void capabilities(uint64_t, WebCore::RealtimeMediaSourceCapabilities&);
    void setMuted(uint64_t, bool);
    void applyConstraints(uint64_t, const WebCore::MediaConstraints&);

    class SourceProxy;
    friend class SourceProxy;
    HashMap<uint64_t, std::unique_ptr<SourceProxy>> m_proxies;
    WebProcessProxy& m_process;
};

}

#endif
