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

NetworkMDNSRegister::~NetworkMDNSRegister() = default;

#if ENABLE_MDNS
struct NetworkMDNSRegister::DNSServiceDeallocator {
    void operator()(DNSServiceRef service) const { DNSServiceRefDeallocate(service); }
};

void NetworkMDNSRegister::unregisterMDNSNames(WebCore::ScriptExecutionContextIdentifier documentIdentifier)
{
    m_services.remove(documentIdentifier);
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
    CompletionHandler<void(const String&, std::optional<WebCore::MDNSRegisterError>)> completionHandler;
};

void NetworkMDNSRegister::closeAndForgetService(DNSServiceRef service)
{
    m_services.removeIf([service] (auto& iterator) {
        return iterator.value.get() == service;
    });
}

struct PendingRegistrationRequestIdentifierType { };
using PendingRegistrationRequestIdentifier = ObjectIdentifier<PendingRegistrationRequestIdentifierType>;
static HashMap<PendingRegistrationRequestIdentifier, std::unique_ptr<PendingRegistrationRequest>>& pendingRegistrationRequestMap()
{
    static NeverDestroyed<HashMap<PendingRegistrationRequestIdentifier, std::unique_ptr<PendingRegistrationRequest>>> map;
    return map.get();
}

static void registerMDNSNameCallback(DNSServiceRef service, DNSRecordRef record, DNSServiceFlags, DNSServiceErrorType errorCode, void* context)
{
    auto request = pendingRegistrationRequestMap().take(PendingRegistrationRequestIdentifier(reinterpret_cast<uintptr_t>(context)));
    if (!request)
        return;

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
        m_services.add(documentIdentifier, std::unique_ptr<_DNSServiceRef_t, DNSServiceDeallocator>(service));
    } else
        service = iterator->value.get();

    auto ip = inet_addr(ipAddress.utf8().data());

    // FIXME: Add IPv6 support.
    if (ip == ( in_addr_t)(-1)) {
        MDNS_RELEASE_LOG("registerMDNSName inet_addr error");
        return completionHandler(name, WebCore::MDNSRegisterError::BadParameter);
    }

    auto identifier = PendingRegistrationRequestIdentifier::generate();
    Ref connection = m_connection.get();
    auto pendingRequest = makeUnique<PendingRegistrationRequest>(connection.get(), WTFMove(name), sessionID(), WTFMove(completionHandler));
    auto addResult = pendingRegistrationRequestMap().add(identifier, WTFMove(pendingRequest));
    DNSRecordRef record { nullptr };
    auto error = DNSServiceRegisterRecord(service,
        &record,
#if HAVE(MDNS_FAST_REGISTRATION)
        kDNSServiceFlagsKnownUnique,
#else
        kDNSServiceFlagsUnique,
#endif
        0,
        addResult.iterator->value->name.utf8().data(),
        kDNSServiceType_A,
        kDNSServiceClass_IN,
        4,
        &ip,
        0,
        registerMDNSNameCallback,
        reinterpret_cast<void*>(identifier.toUInt64()));
    if (error) {
        MDNS_RELEASE_LOG("registerMDNSName DNSServiceRegisterRecord error %d", error);
        m_services.remove(documentIdentifier);
        if (auto pendingRequest = pendingRegistrationRequestMap().take(identifier))
            pendingRequest->completionHandler(pendingRequest->name, WebCore::MDNSRegisterError::DNSSD);
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
    return m_connection->sessionID();
}

} // namespace WebKit

#undef MDNS_RELEASE_LOG
#undef MDNS_RELEASE_LOG_IN_CALLBACK

#endif // ENABLE(WEB_RTC)
