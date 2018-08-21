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
#include "WebMDNSRegisterMessages.h"
#include <pal/SessionID.h>
#include <wtf/UUID.h>

namespace WebKit {
using namespace WebCore;

#define RELEASE_LOG_IF_ALLOWED(sessionID, fmt, ...) RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), Network, "%p - NetworkMDNSRegister::" fmt, this, ##__VA_ARGS__)
#define RELEASE_LOG_IF_ALLOWED_IN_CALLBACK(sessionID, fmt, ...) RELEASE_LOG_IF(sessionID.isAlwaysOnLoggingAllowed(), Network, "NetworkMDNSRegister callback - " fmt, ##__VA_ARGS__)

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
void NetworkMDNSRegister::unregisterMDNSNames(WebCore::DocumentIdentifier documentIdentifier)
{
    auto iterator = m_services.find(documentIdentifier);
    if (iterator == m_services.end())
        return;
    DNSServiceRefDeallocate(iterator->value);
}

struct PendingRegistrationRequest {
    PendingRegistrationRequest(Ref<IPC::Connection> connection, uint64_t requestIdentifier, String&& name, PAL::SessionID sessionID)
        : connection(WTFMove(connection))
        , requestIdentifier(requestIdentifier)
        , name(WTFMove(name))
        , sessionID(sessionID)
    {
    }

    Ref<IPC::Connection> connection;
    uint64_t requestIdentifier { 0 };
    String name;
    PAL::SessionID sessionID;
    DNSRecordRef record;
};


static uintptr_t pendingRegistrationRequestCount = 1;
static HashMap<uintptr_t, std::unique_ptr<PendingRegistrationRequest>>& pendingRegistrationRequests()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashMap<uintptr_t, std::unique_ptr<PendingRegistrationRequest>>> map;
    return map;
}

static void registerMDNSNameCallback(DNSServiceRef, DNSRecordRef record, DNSServiceFlags, DNSServiceErrorType errorCode, void *context)
{
    auto request = pendingRegistrationRequests().take(reinterpret_cast<uintptr_t>(context));
    if (!request)
        return;

    RELEASE_LOG_IF_ALLOWED_IN_CALLBACK(request->sessionID, "registerMDNSNameCallback with error %d", errorCode);

    if (errorCode) {
        request->connection->send(Messages::WebMDNSRegister::FinishedRegisteringMDNSName { request->requestIdentifier, makeUnexpected(MDNSRegisterError::DNSSD) }, 0);
        return;
    }
    request->connection->send(Messages::WebMDNSRegister::FinishedRegisteringMDNSName { request->requestIdentifier, request->name }, 0);
}

void NetworkMDNSRegister::registerMDNSName(uint64_t requestIdentifier, PAL::SessionID sessionID, DocumentIdentifier documentIdentifier, const String& ipAddress)
{
    DNSServiceRef service;
    auto iterator = m_services.find(documentIdentifier);
    if (iterator == m_services.end()) {
        auto error = DNSServiceCreateConnection(&service);
        if (error) {
            RELEASE_LOG_IF_ALLOWED(sessionID, "registerMDNSName DNSServiceCreateConnection error %d", error);
            m_connection.connection().send(Messages::WebMDNSRegister::FinishedRegisteringMDNSName { requestIdentifier, makeUnexpected(MDNSRegisterError::DNSSD) }, 0);
            return;
        }
        error = DNSServiceSetDispatchQueue(service, dispatch_get_main_queue());
        if (error) {
            RELEASE_LOG_IF_ALLOWED(sessionID, "registerMDNSName DNSServiceCreateConnection error %d", error);
            m_connection.connection().send(Messages::WebMDNSRegister::FinishedRegisteringMDNSName { requestIdentifier, makeUnexpected(MDNSRegisterError::DNSSD) }, 0);
            return;
        }
        ASSERT(service);
        m_services.add(documentIdentifier, service);
    } else
        service = iterator->value;

    String baseName = createCanonicalUUIDString();
    String name = makeString(baseName, String::number(pendingRegistrationRequestCount), ".local");

    auto ip = inet_addr(ipAddress.utf8().data());

    if (ip == ( in_addr_t)(-1)) {
        RELEASE_LOG_IF_ALLOWED(sessionID, "registerMDNSName inet_addr error");
        m_connection.connection().send(Messages::WebMDNSRegister::FinishedRegisteringMDNSName { requestIdentifier, makeUnexpected(MDNSRegisterError::BadParameter) }, 0);
        return;
    }

    auto pendingRequest = std::make_unique<PendingRegistrationRequest>(makeRef(m_connection.connection()), requestIdentifier, WTFMove(name), sessionID);
    auto* record = &pendingRequest->record;
    auto error = DNSServiceRegisterRecord(service,
        record,
        kDNSServiceFlagsUnique,
        0,
        pendingRequest->name.utf8().data(),
        kDNSServiceType_A,
        kDNSServiceClass_IN,
        4,
        &ip,
        0,
        registerMDNSNameCallback,
        reinterpret_cast<void*>(pendingRegistrationRequestCount));
    if (error) {
        RELEASE_LOG_IF_ALLOWED(sessionID, "registerMDNSName DNSServiceRegisterRecord error %d", error);
        m_connection.connection().send(Messages::WebMDNSRegister::FinishedRegisteringMDNSName { requestIdentifier, makeUnexpected(MDNSRegisterError::DNSSD) }, 0);
        return;
    }
    pendingRegistrationRequests().add(pendingRegistrationRequestCount++, WTFMove(pendingRequest));
}

struct PendingResolutionRequest {
    PendingResolutionRequest(Ref<IPC::Connection> connection, uint64_t requestIdentifier, PAL::SessionID sessionID)
        : connection(WTFMove(connection))
        , requestIdentifier(requestIdentifier)
        , timeoutTimer(*this, &PendingResolutionRequest::timeout)
        , sessionID(sessionID)
    {
        timeoutTimer.startOneShot(500_ms);
    }

    ~PendingResolutionRequest()
    {
        if (service)
            DNSServiceRefDeallocate(service);
        if (operationService)
            DNSServiceRefDeallocate(operationService);
    }

    void timeout()
    {
        connection->send(Messages::WebMDNSRegister::FinishedResolvingMDNSName { requestIdentifier, makeUnexpected(MDNSRegisterError::Timeout) }, 0);
        requestIdentifier = 0;
    }

    Ref<IPC::Connection> connection;
    uint64_t requestIdentifier { 0 };
    DNSServiceRef service { nullptr };
    DNSServiceRef operationService { nullptr };
    Timer timeoutTimer;
    PAL::SessionID sessionID;
};

static void resolveMDNSNameCallback(DNSServiceRef, DNSServiceFlags, uint32_t, DNSServiceErrorType errorCode, const char *hostname, const struct sockaddr *socketAddress, uint32_t ttl, void *context)
{
    std::unique_ptr<PendingResolutionRequest> request { static_cast<PendingResolutionRequest*>(context) };

    if (!request->requestIdentifier)
        return;

    if (errorCode) {
        RELEASE_LOG_IF_ALLOWED_IN_CALLBACK(request->sessionID, "resolveMDNSNameCallback MDNS error %d", errorCode);
        request->connection->send(Messages::WebMDNSRegister::FinishedResolvingMDNSName { request->requestIdentifier, makeUnexpected(MDNSRegisterError::DNSSD) }, 0);
        return;
    }

    const void* address = socketAddress->sa_family == AF_INET6 ? (const void*) &((const struct sockaddr_in6*)socketAddress)->sin6_addr : (const void*)&((const struct sockaddr_in*)socketAddress)->sin_addr;

    char buffer[INET6_ADDRSTRLEN] = { 0 };
    if (!inet_ntop(socketAddress->sa_family, address, buffer, sizeof(buffer))) {
        RELEASE_LOG_IF_ALLOWED_IN_CALLBACK(request->sessionID, "resolveMDNSNameCallback inet_ntop error");
        request->connection->send(Messages::WebMDNSRegister::FinishedResolvingMDNSName { request->requestIdentifier, makeUnexpected(MDNSRegisterError::DNSSD) }, 0);
        return;
    }

    request->connection->send(Messages::WebMDNSRegister::FinishedResolvingMDNSName { request->requestIdentifier, String { buffer } }, 0);
}

void NetworkMDNSRegister::resolveMDNSName(uint64_t requestIdentifier, PAL::SessionID sessionID, const String& name)
{
    UNUSED_PARAM(sessionID);
    auto pendingRequest = std::make_unique<PendingResolutionRequest>(makeRef(m_connection.connection()), requestIdentifier, sessionID);

    if (auto error = DNSServiceCreateConnection(&pendingRequest->service)) {
        RELEASE_LOG_IF_ALLOWED(sessionID, "resolveMDNSName DNSServiceCreateConnection error %d", error);
        m_connection.connection().send(Messages::WebMDNSRegister::FinishedResolvingMDNSName { requestIdentifier, makeUnexpected(MDNSRegisterError::DNSSD) }, 0);
        return;
    }

    DNSServiceRef service;
    auto error = DNSServiceGetAddrInfo(&service,
        kDNSServiceFlagsUnique,
        0,
        kDNSServiceProtocol_IPv4,
        name.utf8().data(),
        resolveMDNSNameCallback,
        pendingRequest.get());
    pendingRequest->operationService = service;
    if (error) {
        RELEASE_LOG_IF_ALLOWED(sessionID, "resolveMDNSName DNSServiceGetAddrInfo error %d", error);
        m_connection.connection().send(Messages::WebMDNSRegister::FinishedResolvingMDNSName { requestIdentifier, makeUnexpected(MDNSRegisterError::DNSSD) }, 0);
        return;
    }
    pendingRequest.release();

    error = DNSServiceSetDispatchQueue(service, dispatch_get_main_queue());
    if (error) {
        RELEASE_LOG_IF_ALLOWED(sessionID, "resolveMDNSName DNSServiceSetDispatchQueue error %d", error);
        m_connection.connection().send(Messages::WebMDNSRegister::FinishedResolvingMDNSName { requestIdentifier, makeUnexpected(MDNSRegisterError::DNSSD) }, 0);
    }
}
#else
void NetworkMDNSRegister::unregisterMDNSNames(WebCore::DocumentIdentifier)
{
}

void NetworkMDNSRegister::registerMDNSName(uint64_t requestIdentifier, PAL::SessionID sessionID, DocumentIdentifier documentIdentifier, const String& ipAddress)
{
    RELEASE_LOG_IF_ALLOWED(sessionID, "registerMDNSName not implemented");
    m_connection.connection().send(Messages::WebMDNSRegister::FinishedRegisteringMDNSName { requestIdentifier, makeUnexpected(MDNSRegisterError::NotImplemented) }, 0);
}

void NetworkMDNSRegister::resolveMDNSName(uint64_t requestIdentifier, PAL::SessionID sessionID, const String& name)
{
    RELEASE_LOG_IF_ALLOWED(sessionID, "resolveMDNSName not implemented");
    m_connection.connection().send(Messages::WebMDNSRegister::FinishedResolvingMDNSName { requestIdentifier, makeUnexpected(MDNSRegisterError::NotImplemented) }, 0);
}

#endif

} // namespace WebKit

#undef RELEASE_LOG_IF_ALLOWED
#undef RELEASE_LOG_IF_ALLOWED_IN_CALLBACK

#endif // ENABLE(WEB_RTC)
