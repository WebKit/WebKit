/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmRegistry.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithm.h"
#include <mutex>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

CryptoAlgorithmRegistry& CryptoAlgorithmRegistry::shared()
{
    static NeverDestroyed<CryptoAlgorithmRegistry> registry;

    return registry;
}

static std::mutex& registryMutex()
{
    static std::once_flag onceFlag;
    static LazyNeverDestroyed<std::mutex> mutex;

    std::call_once(onceFlag, []{
        mutex.construct();
    });

    return mutex;
}

CryptoAlgorithmRegistry::CryptoAlgorithmRegistry()
{
    platformRegisterAlgorithms();
}

bool CryptoAlgorithmRegistry::getIdentifierForName(const String& name, CryptoAlgorithmIdentifier& result)
{
    if (name.isEmpty())
        return false;

    std::lock_guard<std::mutex> lock(registryMutex());

    auto iter = m_nameToIdentifierMap.find(name.isolatedCopy());
    if (iter == m_nameToIdentifierMap.end())
        return false;

    result = iter->value;
    return true;
}

String CryptoAlgorithmRegistry::nameForIdentifier(CryptoAlgorithmIdentifier identifier)
{
    std::lock_guard<std::mutex> lock(registryMutex());

    return m_identifierToNameMap.get(static_cast<unsigned>(identifier)).isolatedCopy();
}

std::unique_ptr<CryptoAlgorithm> CryptoAlgorithmRegistry::create(CryptoAlgorithmIdentifier identifier)
{
    std::lock_guard<std::mutex> lock(registryMutex());

    auto iter = m_identifierToConstructorMap.find(static_cast<unsigned>(identifier));
    if (iter == m_identifierToConstructorMap.end())
        return nullptr;

    return iter->value();
}

void CryptoAlgorithmRegistry::registerAlgorithm(const String& name, CryptoAlgorithmIdentifier identifier, CryptoAlgorithmConstructor constructor)
{
    std::lock_guard<std::mutex> lock(registryMutex());

    bool added = m_nameToIdentifierMap.add(name, identifier).isNewEntry;
    ASSERT_UNUSED(added, added);

    added = m_identifierToNameMap.add(static_cast<unsigned>(identifier), name).isNewEntry;
    ASSERT_UNUSED(added, added);

    added = m_identifierToConstructorMap.add(static_cast<unsigned>(identifier), constructor).isNewEntry;
    ASSERT_UNUSED(added, added);
}


} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
