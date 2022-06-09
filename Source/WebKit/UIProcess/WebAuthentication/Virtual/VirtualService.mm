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

#import "LocalAuthenticator.h"
#import "VirtualLocalConnection.h"
#import <wtf/text/WTFString.h>

namespace WebKit {

VirtualService::VirtualService(Observer& observer, const Vector<VirtualAuthenticatorConfiguration>& configurations)
    : AuthenticatorTransportService(observer), m_configurations(configurations)
{
}

UniqueRef<AuthenticatorTransportService> VirtualService::createVirtual(WebCore::AuthenticatorTransport transport, Observer& observer,  const Vector<VirtualAuthenticatorConfiguration>& configs)
{
    return makeUniqueRef<VirtualService>(observer, configs);
}

void VirtualService::startDiscoveryInternal()
{

    for (auto& config : m_configurations) {
        if (!observer())
            return;
        switch (config.transport) {
        case WebCore::AuthenticatorTransport::Internal:
            observer()->authenticatorAdded(LocalAuthenticator::create(makeUniqueRef<VirtualLocalConnection>(config)));
                break;
        default:
            UNIMPLEMENTED();
        }
    }
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
