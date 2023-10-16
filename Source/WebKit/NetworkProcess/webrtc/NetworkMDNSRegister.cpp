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
#include "NetworkMDNSRegister.h"

#if ENABLE(WEB_RTC)

#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include <WebCore/MDNSRegisterError.h>
#include <pal/SessionID.h>
#include <wtf/UUID.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebKit {

#define MDNS_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - NetworkMDNSRegister::" fmt, this, ##__VA_ARGS__)
#define MDNS_RELEASE_LOG_IN_CALLBACK(sessionID, fmt, ...) RELEASE_LOG(Network, "NetworkMDNSRegister callback - " fmt, ##__VA_ARGS__)

NetworkMDNSRegister::NetworkMDNSRegister(NetworkConnectionToWebProcess& connection)
    : m_connection(connection)
{
}

NetworkMDNSRegister::~NetworkMDNSRegister()
{
#if ENABLE_MDNS
    for (auto& value : m_services.values())
        DNSServiceRefDeallocate(value);
#endif
}

#if ENABLE_MDNS
void NetworkMDNSRegister::unregisterMDNSNames(WebCore::ScriptExecutionContextIdentifier documentIdentifier)
{
    auto iterator = m_services.find(documentIdentifier);
    if (iterator == m_services.end())
        return;
    DNSServiceRefDeallocate(iterator->value);
    m_services.remove(iterator);
}

struct PendingRegistrationRequest {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    PendingRegistrationRequest(Ref<NetworkConnectionToWebProcess>&& connection, String&& name, PAL::SessionID sessionID, CompletionHandler<void(const String&, std::optional<WebCore::MDNSRegisterError>)>&& completionHandler)
        : connection(WTFMove(connection))
        , name(WTFMove(name))
        , sessionID(sessionID)
        , completionHandler(WTFMove(completionHandler))
    {
    }

    Ref<NetworkConnectionToWebProcess> connection;
    String name;
    PAL::SessionID sessionID;
    DNSRecordRef record;
    CompletionHandler<void(const String&, std::optional<WebCore::MDNSRegisterError>)> completionHandler;
};

void NetworkMDNSRegister::closeAndForgetService(DNSServiceRef service)
{
    DNSServiceRefDeallocate(service);

    m_services.removeIf([service] (auto& iterator) {
        return iterator.value == service;
    });
}

static void registerMDNSNameCallback(DNSServiceRef service, DNSRecordRef record, DNSServiceFlags, DNSServiceErrorType errorCode, void* context)
{
    std::unique_ptr<PendingRegistrationRequest> request { reinterpret_cast<PendingRegistrationRequest*>(context) };

    MDNS_RELEASE_LOG_IN_CALLBACK(request->sessionID, "registerMDNSNameCallback with error %d", errorCode);

    if (errorCode) {
        request->connection->mdnsRegister().closeAndForgetService(service);
        request->completionHandler(request->name, WebCore::MDNSRegisterError::DNSSD);
        return;
    }
    request->completionHandler(request->name, { });
}

void NetworkMDNSRegister::registerMDNSName(WebCore::ScriptExecutionContextIdentifier documentIdentifier, const String& ipAddress, CompletionHandler<void(const String&, std::optional<WebCore::MDNSRegisterError>)>&& completionHandler)
{
    auto name = makeString(WTF::UUID::createVersion4(), ".local"_s);

    DNSServiceRef service;
    auto iterator = m_services.find(documentIdentifier);
    if (iterator == m_services.end()) {
        auto error = DNSServiceCreateConnection(&service);
        if (error) {
            MDNS_RELEASE_LOG("registerMDNSName DNSServiceCreateConnection error %d", error);
            return completionHandler(name, WebCore::MDNSRegisterError::DNSSD);
        }
        error = DNSServiceSetDispatchQueue(service, dispatch_get_main_queue());
        if (error) {
            MDNS_RELEASE_LOG("registerMDNSName DNSServiceCreateConnection error %d", error);
            return completionHandler(name, WebCore::MDNSRegisterError::DNSSD);
        }
        ASSERT(service);
        m_services.add(documentIdentifier, service);
    } else
        service = iterator->value;

    auto ip = inet_addr(ipAddress.utf8().data());

    // FIXME: Add IPv6 support.
    if (ip == ( in_addr_t)(-1)) {
        MDNS_RELEASE_LOG("registerMDNSName inet_addr error");
        return completionHandler(name, WebCore::MDNSRegisterError::BadParameter);
    }

    auto* pendingRequest = new PendingRegistrationRequest(m_connection, WTFMove(name), sessionID(), WTFMove(completionHandler));
    auto* record = &pendingRequest->record;
    auto error = DNSServiceRegisterRecord(service,
        record,
#if HAVE(MDNS_FAST_REGISTRATION)
        kDNSServiceFlagsKnownUnique,
#else
        kDNSServiceFlagsUnique,
#endif
        0,
        pendingRequest->name.utf8().data(),
        kDNSServiceType_A,
        kDNSServiceClass_IN,
        4,
        &ip,
        0,
        registerMDNSNameCallback,
        pendingRequest);
    if (error) {
        MDNS_RELEASE_LOG("registerMDNSName DNSServiceRegisterRecord error %d", error);

        DNSServiceRefDeallocate(service);
        m_services.remove(documentIdentifier);

        pendingRequest->completionHandler(pendingRequest->name, WebCore::MDNSRegisterError::DNSSD);
        delete pendingRequest;
        return;
    }
}

#else // ENABLE_MDNS

void NetworkMDNSRegister::unregisterMDNSNames(WebCore::ScriptExecutionContextIdentifier)
{
}

void NetworkMDNSRegister::registerMDNSName(WebCore::ScriptExecutionContextIdentifier documentIdentifier, const String& ipAddress, CompletionHandler<void(const String&, std::optional<WebCore::MDNSRegisterError>)>&& completionHandler)
{
    MDNS_RELEASE_LOG("registerMDNSName not implemented");
    auto name = makeString(WTF::UUID::createVersion4(), ".local"_s);

    completionHandler(name, WebCore::MDNSRegisterError::NotImplemented);
}

#endif // ENABLE_MDNS

PAL::SessionID NetworkMDNSRegister::sessionID() const
{
    return m_connection.sessionID();
}

} // namespace WebKit

#undef MDNS_RELEASE_LOG
#undef MDNS_RELEASE_LOG_IN_CALLBACK

#endif // ENABLE(WEB_RTC)
