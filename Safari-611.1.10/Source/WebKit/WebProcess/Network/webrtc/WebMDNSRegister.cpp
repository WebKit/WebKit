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
#include "WebMDNSRegister.h"

#if ENABLE(WEB_RTC)

#include "NetworkMDNSRegisterMessages.h"
#include "NetworkProcessConnection.h"
#include "WebProcess.h"
#include <WebCore/Document.h>

namespace WebKit {
using namespace WebCore;

void WebMDNSRegister::finishedRegisteringMDNSName(MDNSRegisterIdentifier identifier, LibWebRTCProvider::MDNSNameOrError&& result)
{
    auto pendingRegistration = m_pendingRegistrations.take(identifier);
    if (!pendingRegistration.callback)
        return;

    if (result.has_value()) {
        auto iterator = m_registeringDocuments.find(pendingRegistration.documentIdentifier);
        if (iterator == m_registeringDocuments.end()) {
            pendingRegistration.callback(makeUnexpected(WebCore::MDNSRegisterError::DNSSD));
            return;
        }
        iterator->value.add(pendingRegistration.ipAddress, result.value());
    }

    pendingRegistration.callback(WTFMove(result));
}

void WebMDNSRegister::unregisterMDNSNames(DocumentIdentifier identifier)
{
    if (m_registeringDocuments.take(identifier).isEmpty())
        return;

    auto& connection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
    connection.send(Messages::NetworkMDNSRegister::UnregisterMDNSNames { identifier }, 0);
}

void WebMDNSRegister::registerMDNSName(DocumentIdentifier identifier, const String& ipAddress, CompletionHandler<void(LibWebRTCProvider::MDNSNameOrError&&)>&& callback)
{
    auto& map = m_registeringDocuments.ensure(identifier, [] {
        return HashMap<String, String> { };
    }).iterator->value;

    auto iterator = map.find(ipAddress);
    if (iterator != map.end()) {
        callback(iterator->value);
        return;
    }

    auto requestIdentifier = MDNSRegisterIdentifier::generate();
    m_pendingRegistrations.add(requestIdentifier, PendingRegistration { WTFMove(callback), identifier, ipAddress });

    auto& connection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
    if (!connection.send(Messages::NetworkMDNSRegister::RegisterMDNSName { requestIdentifier, identifier, ipAddress }, 0))
        finishedRegisteringMDNSName(requestIdentifier, makeUnexpected(MDNSRegisterError::Internal));
}

} // namespace WebKit

#endif // ENABLE(WEB_RTC)
