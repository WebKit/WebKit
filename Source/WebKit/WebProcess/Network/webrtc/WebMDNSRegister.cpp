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

#include "LibWebRTCNetwork.h"
#include "NetworkMDNSRegisterMessages.h"
#include "NetworkProcessConnection.h"
#include "WebProcess.h"
#include <WebCore/Document.h>

namespace WebKit {
using namespace WebCore;

WebMDNSRegister::WebMDNSRegister(LibWebRTCNetwork& libWebRTCNetwork)
    : m_libWebRTCNetwork(libWebRTCNetwork)
{
}

void WebMDNSRegister::ref() const
{
    m_libWebRTCNetwork->ref();
}

void WebMDNSRegister::deref() const
{
    m_libWebRTCNetwork->deref();
}

void WebMDNSRegister::finishedRegisteringMDNSName(WebCore::ScriptExecutionContextIdentifier documentIdentifier, const String& ipAddress, String&& name, std::optional<MDNSRegisterError> error, CompletionHandler<void(const String&, std::optional<MDNSRegisterError>)>&& completionHandler)
{
    if (!error) {
        auto iterator = m_registeringDocuments.find(documentIdentifier);
        if (iterator == m_registeringDocuments.end())
            return completionHandler(name, WebCore::MDNSRegisterError::DNSSD);
        iterator->value.add(ipAddress, name);
    }

    completionHandler(name, error);
}

void WebMDNSRegister::unregisterMDNSNames(ScriptExecutionContextIdentifier identifier)
{
    if (m_registeringDocuments.take(identifier).isEmpty())
        return;

    Ref connection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
    connection->send(Messages::NetworkMDNSRegister::UnregisterMDNSNames { identifier }, 0);
}

void WebMDNSRegister::registerMDNSName(ScriptExecutionContextIdentifier identifier, const String& ipAddress, CompletionHandler<void(const String&, std::optional<MDNSRegisterError>)>&& callback)
{
    auto& map = m_registeringDocuments.ensure(identifier, [] {
        return HashMap<String, String> { };
    }).iterator->value;

    auto iterator = map.find(ipAddress);
    if (iterator != map.end()) {
        callback(iterator->value, { });
        return;
    }

    auto& connection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
    connection.sendWithAsyncReply(Messages::NetworkMDNSRegister::RegisterMDNSName { identifier, ipAddress }, [weakThis = WeakPtr { *this }, callback = WTFMove(callback), identifier, ipAddress] (String&& mdnsName, std::optional<MDNSRegisterError> error) mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->finishedRegisteringMDNSName(identifier, ipAddress, WTFMove(mdnsName), error, WTFMove(callback));
        else
            callback({ }, MDNSRegisterError::Internal);
    });
}

} // namespace WebKit

#endif // ENABLE(WEB_RTC)
