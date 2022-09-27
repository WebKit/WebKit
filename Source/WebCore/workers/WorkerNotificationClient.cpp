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

#include "config.h"
#include "WorkerNotificationClient.h"

#if ENABLE(NOTIFICATIONS)

#include "NotificationData.h"
#include "NotificationResources.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
#include <wtf/threads/BinarySemaphore.h>

namespace WebCore {

Ref<WorkerNotificationClient> WorkerNotificationClient::create(WorkerGlobalScope& workerScope)
{
    return adoptRef(*new WorkerNotificationClient(workerScope));
}

WorkerNotificationClient::WorkerNotificationClient(WorkerGlobalScope& workerScope)
    : m_workerScopeIdentifier(workerScope.identifier())
    , m_workerScope(workerScope)
{
}

bool WorkerNotificationClient::show(ScriptExecutionContext& workerContext, NotificationData&& notification, RefPtr<NotificationResources>&& resources, CompletionHandler<void()>&& completionHandler)
{
    auto callbackID = workerContext.addNotificationCallback(WTFMove(completionHandler));
    postToMainThread([protectedThis = Ref { *this }, notification = WTFMove(notification).isolatedCopy(), resources = WTFMove(resources), callbackID](auto* client, auto& context) mutable {
        if (!client) {
            protectedThis->postToWorkerThread([callbackID](auto& workerContext) {
                if (auto callback = workerContext.takeNotificationCallback(callbackID))
                    callback();
            });
            return;
        }
        client->show(context, WTFMove(notification), WTFMove(resources), [protectedThis = WTFMove(protectedThis), callbackID]() mutable {
            protectedThis->postToWorkerThread([callbackID](auto& workerContext) {
                if (auto callback = workerContext.takeNotificationCallback(callbackID))
                    callback();
            });
        });
    });
    return true;
}

void WorkerNotificationClient::cancel(NotificationData&& notification)
{
    postToMainThread([notification = WTFMove(notification).isolatedCopy()](auto* client, auto&) mutable {
        if (client)
            client->cancel(WTFMove(notification));
    });
}

void WorkerNotificationClient::notificationObjectDestroyed(NotificationData&& notification)
{
    postToMainThread([notification = WTFMove(notification).isolatedCopy()](auto* client, auto&) mutable {
        if (client)
            client->notificationObjectDestroyed(WTFMove(notification));
    });
}

void WorkerNotificationClient::notificationControllerDestroyed()
{
}

void WorkerNotificationClient::requestPermission(ScriptExecutionContext&, PermissionHandler&& completionHandler)
{
    // Workers cannot request permission at the moment.
    ASSERT_NOT_REACHED();
    completionHandler(Permission::Default);
}

auto WorkerNotificationClient::checkPermission(ScriptExecutionContext*) -> Permission
{
    Permission permission { Permission::Default };
    BinarySemaphore semaphore;
    postToMainThread([&permission, &semaphore](auto* client, auto& context) {
        if (client)
            permission = client->checkPermission(&context);
        semaphore.signal();
    });
    semaphore.wait();
    return permission;
}

void WorkerNotificationClient::postToMainThread(Function<void(NotificationClient*, ScriptExecutionContext& context)>&& task)
{
    m_workerScope.thread().workerLoaderProxy().postTaskToLoader([task = WTFMove(task)](auto& context) mutable {
        task(context.notificationClient(), context);
    });
}

void WorkerNotificationClient::postToWorkerThread(Function<void(ScriptExecutionContext&)>&& task)
{
    ScriptExecutionContext::postTaskTo(m_workerScopeIdentifier, WTFMove(task));
}

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS)
