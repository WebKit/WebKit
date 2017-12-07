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

#if ENABLE(SERVICE_WORKER)

#include "URLHash.h"

namespace WebCore {

ServiceWorkerRegistrationKey::ServiceWorkerRegistrationKey(SecurityOriginData&& topOrigin, URL&& scope)
    : m_topOrigin(WTFMove(topOrigin))
    , m_scope(WTFMove(scope))
{
    ASSERT(!m_scope.hasFragment());
}

ServiceWorkerRegistrationKey ServiceWorkerRegistrationKey::emptyKey()
{
    return { };
}

unsigned ServiceWorkerRegistrationKey::hash() const
{
    unsigned hashes[2];
    hashes[0] = SecurityOriginDataHash::hash(m_topOrigin);
    hashes[1] = StringHash::hash(m_scope);

    return StringHasher::hashMemory(hashes, sizeof(hashes));
}

bool ServiceWorkerRegistrationKey::operator==(const ServiceWorkerRegistrationKey& other) const
{
    return m_topOrigin == other.m_topOrigin && m_scope == other.m_scope;
}

ServiceWorkerRegistrationKey ServiceWorkerRegistrationKey::isolatedCopy() const
{
    return { m_topOrigin.isolatedCopy(), m_scope.isolatedCopy() };
}

bool ServiceWorkerRegistrationKey::isMatching(const SecurityOriginData& topOrigin, const URL& clientURL) const
{
    return originIsMatching(topOrigin, clientURL) && clientURL.string().startsWith(m_scope);
}

bool ServiceWorkerRegistrationKey::originIsMatching(const SecurityOriginData& topOrigin, const URL& clientURL) const
{
    if (topOrigin != m_topOrigin)
        return false;

    return protocolHostAndPortAreEqual(clientURL, m_scope);
}

static const char separatorCharacter = '_';

String ServiceWorkerRegistrationKey::toDatabaseKey() const
{
    return makeString(m_topOrigin.protocol, separatorCharacter, m_topOrigin.host, separatorCharacter, m_topOrigin.port.value_or(0), separatorCharacter, m_scope.path());
}

#ifndef NDEBUG
String ServiceWorkerRegistrationKey::loggingString() const
{
    return makeString(m_topOrigin.debugString(), "-", m_scope.string());
}
#endif

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
