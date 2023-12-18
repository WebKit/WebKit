/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ServiceWorkerRegistrationKey.h"

#include "ClientOrigin.h"
#include "RegistrableDomain.h"
#include "SecurityOrigin.h"
#include <wtf/URLHash.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

ServiceWorkerRegistrationKey::ServiceWorkerRegistrationKey(SecurityOriginData&& topOrigin, URL&& scope)
    : m_topOrigin(WTFMove(topOrigin))
    , m_scope(WTFMove(scope))
{
    ASSERT(!m_scope.hasFragmentIdentifier());
}

ServiceWorkerRegistrationKey ServiceWorkerRegistrationKey::emptyKey()
{
    return { };
}

ServiceWorkerRegistrationKey ServiceWorkerRegistrationKey::isolatedCopy() const &
{
    return { m_topOrigin.isolatedCopy(), m_scope.isolatedCopy() };
}

ServiceWorkerRegistrationKey ServiceWorkerRegistrationKey::isolatedCopy() &&
{
    return { WTFMove(m_topOrigin).isolatedCopy(), WTFMove(m_scope).isolatedCopy() };
}

bool ServiceWorkerRegistrationKey::isMatching(const SecurityOriginData& topOrigin, const URL& clientURL) const
{
    return originIsMatching(topOrigin, clientURL) && clientURL.string().startsWith(m_scope.string());
}

bool ServiceWorkerRegistrationKey::originIsMatching(const SecurityOriginData& topOrigin, const URL& clientURL) const
{
    if (topOrigin != m_topOrigin)
        return false;

    return protocolHostAndPortAreEqual(clientURL, m_scope);
}

bool ServiceWorkerRegistrationKey::relatesToOrigin(const SecurityOriginData& securityOrigin) const
{
    if (m_topOrigin == securityOrigin)
        return true;

    return SecurityOriginData::fromURL(m_scope) == securityOrigin;
}

RegistrableDomain ServiceWorkerRegistrationKey::firstPartyForCookies() const
{
    return RegistrableDomain::uncheckedCreateFromHost(m_topOrigin.host());
}

static const char separatorCharacter = '_';

String ServiceWorkerRegistrationKey::toDatabaseKey() const
{
    if (m_topOrigin.port())
        return makeString(m_topOrigin.protocol(), separatorCharacter, m_topOrigin.host(), separatorCharacter, String::number(m_topOrigin.port().value()), separatorCharacter, m_scope.string());
    return makeString(m_topOrigin.protocol(), separatorCharacter, m_topOrigin.host(), separatorCharacter, separatorCharacter, m_scope.string());
}

std::optional<ServiceWorkerRegistrationKey> ServiceWorkerRegistrationKey::fromDatabaseKey(const String& key)
{
    auto first = key.find(separatorCharacter, 0);
    if (first == notFound)
        return std::nullopt;

    auto second = key.find(separatorCharacter, first + 1);
    if (second == notFound)
        return std::nullopt;

    auto third = key.find(separatorCharacter, second + 1);
    if (third == notFound)
        return std::nullopt;

    std::optional<uint16_t> shortPort;

    // If there's a gap between third and second, we expect to have a port to decode
    if (third - second > 1) {
        shortPort = parseInteger<uint16_t>(StringView { key }.substring(second + 1, third - second - 1));
        if (!shortPort)
            return std::nullopt;
    }

    auto scheme = StringView(key).left(first);
    auto host = StringView(key).substring(first + 1, second - first - 1);

    URL topOriginURL { makeString(scheme, "://", host) };
    if (!topOriginURL.isValid())
        return std::nullopt;

    URL scope { key.substring(third + 1) };
    if (!scope.isValid())
        return std::nullopt;

    SecurityOriginData topOrigin { scheme.toString(), host.toString(), shortPort };
    return ServiceWorkerRegistrationKey { WTFMove(topOrigin), WTFMove(scope) };
}

ClientOrigin ServiceWorkerRegistrationKey::clientOrigin() const
{
    return ClientOrigin { m_topOrigin, SecurityOriginData::fromURL(m_scope) };
}

#if !LOG_DISABLED
String ServiceWorkerRegistrationKey::loggingString() const
{
    return makeString(m_topOrigin.debugString(), "-", m_scope.string());
}
#endif

} // namespace WebCore
