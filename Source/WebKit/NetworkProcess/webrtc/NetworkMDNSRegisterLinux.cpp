/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Comcast Inc.
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
#include "NetworkMDNSRegister.h"

#if ENABLE(WEB_RTC) && USE(GLIB) && ENABLE(MDNS_SERVICE)

#include "NetworkConnectionToWebProcess.h"
#include <WebCore/MDNSRegisterError.h>
#include <wtf/UUID.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

void NetworkMDNSRegister::start(const String& address)
{
    if (m_mdnsService)
        return;
    m_mdnsService.reset(mdns_service_start(address.ascii().data()));
}

struct MDNSResolveAddressData {
    NetworkMDNSRegister::ResolveCallback callback;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(MDNSResolveAddressData);

void NetworkMDNSRegister::resolveAddress(const String& address, NetworkMDNSRegister::ResolveCallback&& callback)
{
    if (!m_mdnsService) [[unlikely]] {
        callOnMainRunLoopAndWait([callback = WTF::move(callback)] mutable {
            callback(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::OperationError, "MDNS resolution, failed, MDNS service is stopped"_s }));
        });
        return;
    }

    auto data = createMDNSResolveAddressData();
    data->callback = WTF::move(callback);

    mdns_service_query_hostname(m_mdnsService.get(), data, [](void* userData, const char* hostname, const char* address) {
        auto data = reinterpret_cast<MDNSResolveAddressData*>(userData);
        callOnMainRunLoopAndWait([callback = WTF::move(data->callback), address = String::fromUTF8(address)] mutable {
            callback(WTF::move(address));
        });
        destroyMDNSResolveAddressData(data);
    }, [](void* userData, const char* hostname) mutable {
        auto data = reinterpret_cast<MDNSResolveAddressData*>(userData);
        callOnMainRunLoopAndWait([callback = WTF::move(data->callback)] mutable {
            callback(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::TimeoutError, "MDNS resolution timed out"_s }));
        });
        destroyMDNSResolveAddressData(data);
    }, address.ascii().data());
}

void NetworkMDNSRegister::registerMDNSName(WebCore::ScriptExecutionContextIdentifier documentIdentifier, const String& ipAddress, CompletionHandler<void(const String&, std::optional<WebCore::MDNSRegisterError>)>&& completionHandler)
{
    auto name = makeString(WTF::UUID::createVersion4(), ".local"_s);

    if (ipAddress == "0.0.0.0"_s || ipAddress == "::"_s) {
        completionHandler(name, WebCore::MDNSRegisterError::BadParameter);
        return;
    }

    m_registeredNames.add(name);
    m_perDocumentRegisteredNames.ensure(documentIdentifier, [] {
        return Vector<String>();
    }).iterator->value.append(name);

    mdns_service_register_hostname(m_mdnsService.get(), name.ascii().data(), ipAddress.ascii().data());
    completionHandler(name, { });
}

} // namespace WebKit

#endif // ENABLE(WEB_RTC) && USE(GLIB)  && ENABLE(MDNS_SERVICE)
