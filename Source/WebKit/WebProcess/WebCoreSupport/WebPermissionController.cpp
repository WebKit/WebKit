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
#include "WebPermissionControllerMessages.h"
#include "WebPermissionControllerProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/Document.h>
#include <WebCore/Page.h>
#include <WebCore/PermissionObserver.h>
#include <WebCore/PermissionQuerySource.h>
#include <WebCore/PermissionState.h>
#include <WebCore/Permissions.h>
#include <WebCore/SecurityOriginData.h>
#include <optional>

namespace WebKit {

Ref<WebPermissionController> WebPermissionController::create(WebProcess& process)
{
    return adoptRef(*new WebPermissionController(process));
}

WebPermissionController::WebPermissionController(WebProcess& process)
{
    process.addMessageReceiver(Messages::WebPermissionController::messageReceiverName(), *this);
}

WebPermissionController::~WebPermissionController()
{
    WebProcess::singleton().removeMessageReceiver(Messages::WebPermissionController::messageReceiverName());
}

void WebPermissionController::query(WebCore::ClientOrigin&& origin, WebCore::PermissionDescriptor descriptor, WebCore::Page* page, WebCore::PermissionQuerySource source, CompletionHandler<void(std::optional<WebCore::PermissionState>)>&& completionHandler)
{
    std::optional<WebPageProxyIdentifier> proxyIdentifier;
    if (source == WebCore::PermissionQuerySource::Window || source == WebCore::PermissionQuerySource::DedicatedWorker) {
        ASSERT(page);
        proxyIdentifier = WebPage::fromCorePage(*page).webPageProxyIdentifier();
    }

    m_requests.append(PermissionRequest { WTFMove(origin), descriptor, proxyIdentifier, source, WTFMove(completionHandler) });
    tryProcessingRequests();
}

void WebPermissionController::tryProcessingRequests()
{
    while (!m_requests.isEmpty()) {
        auto& currentRequest = m_requests.first();
        if (currentRequest.isWaitingForReply)
            return;

        currentRequest.isWaitingForReply = true;
        WebProcess::singleton().sendWithAsyncReply(Messages::WebPermissionControllerProxy::Query(currentRequest.origin, currentRequest.descriptor, currentRequest.identifier, currentRequest.source), [this, weakThis = WeakPtr { *this }](auto state) {
            RefPtr protectedThis { weakThis.get() };
            if (!protectedThis)
                return;

            m_requests.takeFirst().completionHandler(state);
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

void WebPermissionController::permissionChanged(WebCore::PermissionName permissionName, const WebCore::SecurityOriginData& topOrigin)
{
    for (auto& observer : m_observers) {
        if (observer.descriptor().name != permissionName || observer.origin().topOrigin != topOrigin)
            return;

        auto* context = observer.context();
        if (!context)
            continue;

        auto source = WebCore::Permissions::sourceFromContext(*context);
        // FIXME: Add support for workers.
        ASSERT(source && *source == WebCore::PermissionQuerySource::Window);

        auto* page = downcast<WebCore::Document>(context)->page();
        if (!page)
            continue;

        query(WebCore::ClientOrigin { observer.origin() }, WebCore::PermissionDescriptor { permissionName }, page, *source, [observer = WeakPtr { observer }](auto newState) {
            if (observer && newState != observer->currentState())
                observer->stateChanged(*newState);
        });
    }
}

} // namespace WebKit
