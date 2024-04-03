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
#include "ServiceWorkerRegistrationPushAPI.h"

#include "PushManager.h"
#include "ScriptExecutionContext.h"
#include "ServiceWorkerRegistration.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

ServiceWorkerRegistrationPushAPI::ServiceWorkerRegistrationPushAPI(ServiceWorkerRegistration& serviceWorkerRegistration)
    : m_serviceWorkerRegistration(serviceWorkerRegistration)
{
}

ServiceWorkerRegistrationPushAPI::~ServiceWorkerRegistrationPushAPI() = default;

PushManager& ServiceWorkerRegistrationPushAPI::pushManager(ServiceWorkerRegistration& serviceWorkerRegistration)
{
    return ServiceWorkerRegistrationPushAPI::from(serviceWorkerRegistration)->pushManager();
}

PushManager& ServiceWorkerRegistrationPushAPI::pushManager()
{
    if (!m_pushManager)
        m_pushManager = makeUniqueWithoutRefCountedCheck<PushManager>(m_serviceWorkerRegistration);

    return *m_pushManager;
}

ServiceWorkerRegistrationPushAPI* ServiceWorkerRegistrationPushAPI::from(ServiceWorkerRegistration& serviceWorkerRegistration)
{
    auto* supplement = static_cast<ServiceWorkerRegistrationPushAPI*>(Supplement<ServiceWorkerRegistration>::from(&serviceWorkerRegistration, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<ServiceWorkerRegistrationPushAPI>(serviceWorkerRegistration);
        supplement = newSupplement.get();
        provideTo(&serviceWorkerRegistration, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

ASCIILiteral ServiceWorkerRegistrationPushAPI::supplementName()
{
    return "ServiceWorkerRegistrationPushAPI"_s;
}

}
