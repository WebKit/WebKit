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

#include <WebCore/ClientOrigin.h>
#include <WebCore/PermissionController.h>
#include <WebCore/PermissionDescriptor.h>
#include <wtf/Deque.h>
#include <wtf/WeakHashSet.h>

namespace WebKit {

class WebPage;

class WebPermissionController final : public CanMakeWeakPtr<WebPermissionController>, public WebCore::PermissionController {
public:
    static Ref<WebPermissionController> create(WebPage&);

private:
    explicit WebPermissionController(WebPage&);

    // WebCore::PermissionController
    WebCore::PermissionState query(WebCore::ClientOrigin&&, WebCore::PermissionDescriptor&&) final;
    void request(WebCore::ClientOrigin&&, WebCore::PermissionDescriptor&&, CompletionHandler<void(WebCore::PermissionState)>&&) final;
    void addObserver(WebCore::PermissionObserver&) final;
    void removeObserver(WebCore::PermissionObserver&) final;

    WebCore::PermissionState queryCache(const WebCore::ClientOrigin&, const WebCore::PermissionDescriptor&);
    void updateCache(const WebCore::ClientOrigin&, const WebCore::PermissionDescriptor&, WebCore::PermissionState);
    void tryProcessingRequests();
    void permissionChanged(const WebCore::ClientOrigin&, const WebCore::PermissionDescriptor&, WebCore::PermissionState);

    WeakPtr<WebPage> m_page;
    WeakHashSet<WebCore::PermissionObserver> m_observers;

    using PermissionEntry = std::pair<WebCore::PermissionDescriptor, WebCore::PermissionState>;
    HashMap<WebCore::ClientOrigin, Vector<PermissionEntry>> m_cachedPermissionEntries;

    struct PermissionRequest {
        WebCore::ClientOrigin origin;
        WebCore::PermissionDescriptor descriptor;
        CompletionHandler<void(WebCore::PermissionState)> completionHandler;
        bool isWaitingForReply { false };
    };
    Deque<PermissionRequest> m_requests;
};

} // namespace WebCore
