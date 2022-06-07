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

#include "config.h"
#include "WebPermissionController.h"


#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/PermissionObserver.h>

namespace WebKit {

Ref<WebPermissionController> WebPermissionController::create(WebPage& page)
{
    return adoptRef(*new WebPermissionController(page));
}

WebPermissionController::WebPermissionController(WebPage& page)
    : m_page(page)
{
}

void WebPermissionController::query(WebCore::ClientOrigin&& origin, WebCore::PermissionDescriptor&& descriptor, CompletionHandler<void(std::optional<WebCore::PermissionState>)>&& completionHandler)
{
    if (!m_page)
        return completionHandler({ });

    auto cachedResult = queryCache(origin, descriptor);
    if (cachedResult != WebCore::PermissionState::Prompt)
        return completionHandler(cachedResult);

    m_requests.append(PermissionRequest { WTFMove(origin), WTFMove(descriptor), WTFMove(completionHandler) });
    tryProcessingRequests();
}

WebCore::PermissionState WebPermissionController::queryCache(const WebCore::ClientOrigin& origin, const WebCore::PermissionDescriptor& descriptor)
{
    auto iterator = m_cachedPermissionEntries.find(origin);
    if (iterator != m_cachedPermissionEntries.end()) {
        for (auto& entry : iterator->value) {
            if (entry.first == descriptor)
                return entry.second;
        }
    }

    return WebCore::PermissionState::Prompt;
}

void WebPermissionController::updateCache(const WebCore::ClientOrigin& origin, const WebCore::PermissionDescriptor& descriptor, WebCore::PermissionState state)
{
    auto& entries = m_cachedPermissionEntries.ensure(origin, [&]() {
        return Vector<PermissionEntry> { };
    }).iterator->value;
    for (auto& entry : entries) {
        if (entry.first == descriptor) {
            if (entry.second == state)
                return;

            entry.second = state;
            permissionChanged(origin, descriptor, state);
            return;
        }
    }
}

void WebPermissionController::tryProcessingRequests()
{
    if (m_requests.isEmpty() || !m_page)
        return;

    while (!m_requests.isEmpty()) {
        auto& currentRequest = m_requests.first();
        if (currentRequest.isWaitingForReply)
            return;

        // Cache may have updated.
        auto cachedResult = queryCache(currentRequest.origin, currentRequest.descriptor);
        if (cachedResult != WebCore::PermissionState::Prompt) {
            m_requests.takeFirst().completionHandler(cachedResult);
            continue;
        }

        currentRequest.isWaitingForReply = true;
        m_page->sendWithAsyncReply(Messages::WebPageProxy::QueryPermission(currentRequest.origin, currentRequest.descriptor), [this, weakThis = WeakPtr { *this }](auto state, bool shouldCache) {
            if (!weakThis)
                return;

            auto takenRequest = m_requests.takeFirst();
            takenRequest.completionHandler(state);
            if (shouldCache && state)
                updateCache(takenRequest.origin, takenRequest.descriptor, *state);

            tryProcessingRequests();
        });
    }
}

void WebPermissionController::addObserver(WebCore::PermissionObserver& observer)
{
    m_observers.add(observer);
}

void WebPermissionController::removeObserver(WebCore::PermissionObserver& observer)
{
    m_observers.remove(observer);
}

void WebPermissionController::permissionChanged(const WebCore::ClientOrigin& origin, const WebCore::PermissionDescriptor& descriptor, WebCore::PermissionState state)
{
    for (auto& observer : m_observers) {
        if (observer.origin() == origin && observer.descriptor() == descriptor)
            observer.stateChanged(state);
    }
}

} // namespace WebKit
