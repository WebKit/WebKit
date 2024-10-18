/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "AuthenticatorTransportService.h"

#if ENABLE(WEB_AUTHN)

#include "CcidService.h"
#include "HidService.h"
#include "LocalService.h"
#include "MockCcidService.h"
#include "MockHidService.h"
#include "MockLocalService.h"
#include "MockNfcService.h"
#include "NfcService.h"
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AuthenticatorTransportService);

Ref<AuthenticatorTransportService> AuthenticatorTransportService::create(WebCore::AuthenticatorTransport transport, AuthenticatorTransportServiceObserver& observer)
{
    switch (transport) {
    case WebCore::AuthenticatorTransport::Internal:
        return LocalService::create(observer);
    case WebCore::AuthenticatorTransport::Usb:
        return HidService::create(observer);
    case WebCore::AuthenticatorTransport::Nfc:
        return NfcService::create(observer);
    case WebCore::AuthenticatorTransport::SmartCard:
        return CcidService::create(observer);
    default:
        ASSERT_NOT_REACHED();
        return LocalService::create(observer);
    }
}

Ref<AuthenticatorTransportService> AuthenticatorTransportService::createMock(WebCore::AuthenticatorTransport transport, AuthenticatorTransportServiceObserver& observer, const WebCore::MockWebAuthenticationConfiguration& configuration)
{
    switch (transport) {
    case WebCore::AuthenticatorTransport::Internal:
        return MockLocalService::create(observer, configuration);
    case WebCore::AuthenticatorTransport::Usb:
        return MockHidService::create(observer, configuration);
    case WebCore::AuthenticatorTransport::Nfc:
        return MockNfcService::create(observer, configuration);
    case WebCore::AuthenticatorTransport::SmartCard:
        return MockCcidService::create(observer, configuration);
    default:
        ASSERT_NOT_REACHED();
        return MockLocalService::create(observer, configuration);
    }
}

AuthenticatorTransportService::AuthenticatorTransportService(AuthenticatorTransportServiceObserver& observer)
    : m_observer(observer)
{
}

void AuthenticatorTransportService::startDiscovery()
{
    RunLoop::main().dispatch([weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;
        weakThis->startDiscoveryInternal();
    });
}

void AuthenticatorTransportService::restartDiscovery()
{
    RunLoop::main().dispatch([weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;
        weakThis->restartDiscoveryInternal();
    });
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
