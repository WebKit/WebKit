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

#include "MessageSenderInlines.h"
#include "NetworkConnectionToWebProcessMessages.h"
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
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SecurityOriginData.h>
#include <optional>

#if ENABLE(WEB_PUSH_NOTIFICATIONS)
#include "NetworkProcessConnection.h"
#include "NotificationManagerMessageHandlerMessages.h"
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/PushPermissionState.h>
#endif

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

void WebPermissionController::query(WebCore::ClientOrigin&& origin, WebCore::PermissionDescriptor descriptor, const WeakPtr<WebCore::Page>& page, WebCore::PermissionQuerySource source, CompletionHandler<void(std::optional<WebCore::PermissionState>)>&& completionHandler)
{
#if ENABLE(WEB_PUSH_NOTIFICATIONS)
    if (WebCore::DeprecatedGlobalSettings::builtInNotificationsEnabled() && (descriptor.name == WebCore::PermissionName::Notifications || descriptor.name == WebCore::PermissionName::Push)) {
        Ref connection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
        auto notificationPermissionHandler = [completionHandler = WTFMove(completionHandler)](WebCore::PushPermissionState pushPermissionState) mutable {
            auto state = [pushPermissionState]() -> WebCore::PermissionState {
                switch (pushPermissionState) {
                case WebCore::PushPermissionState::Granted: return WebCore::PermissionState::Granted;
                case WebCore::PushPermissionState::Denied: return WebCore::PermissionState::Denied;
                case WebCore::PushPermissionState::Prompt: return WebCore::PermissionState::Prompt;
                default: RELEASE_ASSERT_NOT_REACHED();
                }
            }();
            completionHandler(state);
        };
        connection->sendWithAsyncReply(Messages::NotificationManagerMessageHandler::GetPermissionState(origin.clientOrigin), WTFMove(notificationPermissionHandler), WebProcess::singleton().sessionID().toUInt64());
        return;
    }
#endif

    std::optional<WebPageProxyIdentifier> proxyIdentifier;
    if (source == WebCore::PermissionQuerySource::Window || source == WebCore::PermissionQuerySource::DedicatedWorker) {
        ASSERT(page);
        proxyIdentifier = WebPage::fromCorePage(*page)->webPageProxyIdentifier();
    }

    if (descriptor.name == WebCore::PermissionName::StorageAccess) {
        Ref networkProcess = WebProcess::singleton().ensureNetworkProcessConnection().connection();
        networkProcess->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::QueryStorageAccessPermission(WebCore::RegistrableDomain { origin.clientOrigin }, WebCore::RegistrableDomain { origin.topOrigin }, proxyIdentifier), WTFMove(completionHandler));
        return;
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebPermissionControllerProxy::Query(origin, descriptor, proxyIdentifier, source), WTFMove(completionHandler));
}

void WebPermissionController::addObserver(WebCore::PermissionObserver& observer)
{
    m_observers.add(observer);
}

void WebPermissionController::removeObserver(WebCore::PermissionObserver& observer)
{
    m_observers.remove(observer);
}

template<typename ObserverFilter>
void WebPermissionController::notifyObserversIfNeeded(WebCore::PermissionName permissionName, ObserverFilter&& filter)
{
    ASSERT(isMainRunLoop());

    for (auto& observer : m_observers) {
        if (!filter(observer))
            continue;

        auto source = observer.source();
        if (!observer.page() && (source == WebCore::PermissionQuerySource::Window || source == WebCore::PermissionQuerySource::DedicatedWorker))
            continue;

        query(WebCore::ClientOrigin { observer.origin() }, WebCore::PermissionDescriptor { permissionName }, observer.page(), source, [observer = WeakPtr { observer }](auto newState) {
            if (observer && newState != observer->currentState())
                observer->stateChanged(*newState);
        });
    }
}

void WebPermissionController::storageAccessPermissionChanged(const WebCore::RegistrableDomain& topFrameDomain, const WebCore::RegistrableDomain& subFrameDomain)
{
    notifyObserversIfNeeded(WebCore::PermissionName::StorageAccess, [&](const WebCore::PermissionObserver& observer) {
        return observer.descriptor().name == WebCore::PermissionName::StorageAccess
            && WebCore::RegistrableDomain(observer.origin().topOrigin) == topFrameDomain
            && WebCore::RegistrableDomain(observer.origin().clientOrigin) == subFrameDomain;
    });
}

void WebPermissionController::permissionChanged(WebCore::PermissionName permissionName, const WebCore::SecurityOriginData& topOrigin)
{
    notifyObserversIfNeeded(permissionName, [&](const WebCore::PermissionObserver& observer) {
        return permissionName != WebCore::PermissionName::StorageAccess
            && observer.descriptor().name == permissionName
            && observer.origin().topOrigin == topOrigin;
    });
}

void WebPermissionController::addChangeListener(WebCore::PermissionName permissionName, const WebCore::RegistrableDomain& topFrameDomain, const WebCore::RegistrableDomain& subFrameDomain)
{
    if (permissionName == WebCore::PermissionName::StorageAccess) {
        Ref networkProcess = WebProcess::singleton().ensureNetworkProcessConnection().connection();
        networkProcess->send(Messages::NetworkConnectionToWebProcess::SubscribeToStorageAccessPermissionChanges(topFrameDomain, subFrameDomain), 0);
    }
}

void WebPermissionController::removeChangeListener(WebCore::PermissionName permissionName, const WebCore::RegistrableDomain& topFrameDomain, const WebCore::RegistrableDomain& subFrameDomain)
{
    if (permissionName == WebCore::PermissionName::StorageAccess) {
        Ref networkProcess = WebProcess::singleton().ensureNetworkProcessConnection().connection();
        networkProcess->send(Messages::NetworkConnectionToWebProcess::UnsubscribeFromStorageAccessPermissionChanges(topFrameDomain, subFrameDomain), 0);
    }
}

} // namespace WebKit
