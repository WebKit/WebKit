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
#include "WebPageProxyIdentifier.h"

namespace WebCore {
enum class PermissionQuerySource : uint8_t;
enum class PermissionState : uint8_t;
struct ClientOrigin;
struct PermissionDescriptor;
class SecurityOriginData;
}

namespace WebKit {

class WebPageProxy;
class WebProcessProxy;

class WebPermissionControllerProxy final : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WebPermissionControllerProxy(WebProcessProxy&);
    ~WebPermissionControllerProxy();

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    RefPtr<WebPageProxy> mostReasonableWebPageProxy(const WebCore::SecurityOriginData&, WebCore::PermissionQuerySource) const;

    // IPC Message handlers.
    void query(const WebCore::ClientOrigin&, const WebCore::PermissionDescriptor&, std::optional<WebPageProxyIdentifier>, WebCore::PermissionQuerySource, CompletionHandler<void(std::optional<WebCore::PermissionState>)>&&);

    WebProcessProxy& m_process;
};

} // namespace WebKit
