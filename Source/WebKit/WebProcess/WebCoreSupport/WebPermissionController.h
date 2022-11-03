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

#include "MessageReceiver.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/PermissionController.h>
#include <WebCore/PermissionDescriptor.h>
#include <wtf/Deque.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {
enum class PermissionQuerySource : uint8_t;
enum class PermissionState : uint8_t;
class Page;
struct SecurityOriginData;
}

namespace WebKit {

class WebProcess;

class WebPermissionController final : public WebCore::PermissionController, public IPC::MessageReceiver {
public:
    static Ref<WebPermissionController> create(WebProcess&);
    ~WebPermissionController();

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    explicit WebPermissionController(WebProcess&);

    // WebCore::PermissionController
    void query(WebCore::ClientOrigin&&, WebCore::PermissionDescriptor, const WeakPtr<WebCore::Page>&, WebCore::PermissionQuerySource, CompletionHandler<void(std::optional<WebCore::PermissionState>)>&&) final;
    void addObserver(WebCore::PermissionObserver&) final;
    void removeObserver(WebCore::PermissionObserver&) final;
    void permissionChanged(WebCore::PermissionName, const WebCore::SecurityOriginData&) final;

    WeakHashSet<WebCore::PermissionObserver> m_observers;
};

} // namespace WebCore
