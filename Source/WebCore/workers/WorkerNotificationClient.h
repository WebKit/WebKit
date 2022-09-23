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

#if ENABLE(NOTIFICATIONS)

#include "NotificationClient.h"
#include "ScriptExecutionContextIdentifier.h"
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class WorkerGlobalScope;

class WorkerNotificationClient : public NotificationClient, public ThreadSafeRefCounted<WorkerNotificationClient> {
public:
    static Ref<WorkerNotificationClient> create(WorkerGlobalScope&);

    // NotificationClient.
    bool show(ScriptExecutionContext&, NotificationData&&, RefPtr<NotificationResources>&&, CompletionHandler<void()>&&) final;
    void cancel(NotificationData&&) final;
    void notificationObjectDestroyed(NotificationData&&) final;
    void notificationControllerDestroyed() final;
    void requestPermission(ScriptExecutionContext&, PermissionHandler&&) final;
    Permission checkPermission(ScriptExecutionContext*) final;

private:
    explicit WorkerNotificationClient(WorkerGlobalScope&);

    void postToMainThread(Function<void(NotificationClient*, ScriptExecutionContext& context)>&&);
    void postToWorkerThread(Function<void(ScriptExecutionContext&)>&&);

    ScriptExecutionContextIdentifier m_workerScopeIdentifier;
    WorkerGlobalScope& m_workerScope;
};

} // namespace WebCore

#endif
