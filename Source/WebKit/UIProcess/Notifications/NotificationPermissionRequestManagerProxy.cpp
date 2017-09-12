/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NotificationPermissionRequestManagerProxy.h"

#include "NotificationPermissionRequest.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

namespace WebKit {

NotificationPermissionRequestManagerProxy::NotificationPermissionRequestManagerProxy(WebPageProxy& page)
    : m_page(page)
{
}

void NotificationPermissionRequestManagerProxy::invalidateRequests()
{
    for (auto& request : m_pendingRequests.values())
        request->deny();

    m_pendingRequests.clear();
}

Ref<NotificationPermissionRequest> NotificationPermissionRequestManagerProxy::createRequest(uint64_t notificationID)
{
    auto request = NotificationPermissionRequest::create([this, notificationID, page = makeRef(m_page)](bool allowed) {
        m_pendingRequests.take(notificationID);
        page->process().send(Messages::WebPage::DidReceiveNotificationPermissionDecision(notificationID, allowed), page->pageID());
    });
    m_pendingRequests.add(notificationID, request.ptr());
    return request;
}

} // namespace WebKit
