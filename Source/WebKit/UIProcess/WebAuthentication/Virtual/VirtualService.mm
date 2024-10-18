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

#import "config.h"
#import "VirtualService.h"

#if ENABLE(WEB_AUTHN)

#import "CtapAuthenticator.h"
#import "CtapHidDriver.h"
#import "LocalAuthenticator.h"
#import "VirtualAuthenticatorManager.h"
#import "VirtualHidConnection.h"
#import "VirtualLocalConnection.h"
#import <WebCore/FidoConstants.h>
#import <WebCore/WebAuthenticationConstants.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/UniqueRef.h>
#import <wtf/text/WTFString.h>

namespace WebKit {
using namespace fido;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(VirtualService);

Ref<VirtualService> VirtualService::create(AuthenticatorTransportServiceObserver& observer, Vector<std::pair<String, VirtualAuthenticatorConfiguration>>& configuration)
{
    return adoptRef(*new VirtualService(observer, configuration));
}

VirtualService::VirtualService(AuthenticatorTransportServiceObserver& observer, Vector<std::pair<String, VirtualAuthenticatorConfiguration>>& authenticators)
    : AuthenticatorTransportService(observer), m_authenticators(authenticators)
{
}

Ref<AuthenticatorTransportService> VirtualService::createVirtual(WebCore::AuthenticatorTransport transport, AuthenticatorTransportServiceObserver& observer, Vector<std::pair<String, VirtualAuthenticatorConfiguration>>& authenticators)
{
    return VirtualService::create(observer, authenticators);
}

static AuthenticatorGetInfoResponse authenticatorInfoForConfig(const VirtualAuthenticatorConfiguration& config)
{
    AuthenticatorGetInfoResponse infoResponse({ ProtocolVersion::kCtap }, Vector<uint8_t>(aaguidLength, 0u));
    AuthenticatorSupportedOptions options;
    infoResponse.setOptions(WTFMove(options));
    return infoResponse;
}

void VirtualService::startDiscoveryInternal()
{

    for (auto& authenticator : m_authenticators) {
        if (!observer())
            return;
        auto config = authenticator.second;
        auto authenticatorId = authenticator.first;
        switch (config.transport) {
        case WebCore::AuthenticatorTransport::Nfc:
        case WebCore::AuthenticatorTransport::Ble:
        case WebCore::AuthenticatorTransport::Usb:
            observer()->authenticatorAdded(CtapAuthenticator::create(CtapHidDriver::create(VirtualHidConnection::create(authenticatorId, config, WeakPtr { static_cast<VirtualAuthenticatorManager *>(observer()) })), authenticatorInfoForConfig(config)));
            break;
        case WebCore::AuthenticatorTransport::Internal:
            observer()->authenticatorAdded(LocalAuthenticator::create(VirtualLocalConnection::create(config)));
            break;
        default:
            UNIMPLEMENTED();
        }
    }
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
