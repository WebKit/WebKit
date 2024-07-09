/*
 * Copyright (C) 2020 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CDMProxy.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "Logging.h"
#include "MediaPlayer.h"
#include <wtf/HexNumber.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

Vector<CDMProxyFactory*>& CDMProxyFactory::registeredFactories()
{
    static NeverDestroyed factories = platformRegisterFactories();
    return factories;
}

void CDMProxyFactory::registerFactory(CDMProxyFactory& factory)
{
    ASSERT(!registeredFactories().contains(&factory));
    registeredFactories().append(&factory);
}

void CDMProxyFactory::unregisterFactory(CDMProxyFactory& factory)
{
    ASSERT(registeredFactories().contains(&factory));
    registeredFactories().removeAll(&factory);
}

RefPtr<CDMProxy> CDMProxyFactory::createCDMProxyForKeySystem(const String& keySystem)
{
    RefPtr<CDMProxy> cdmProxy;
    for (auto* factory : CDMProxyFactory::registeredFactories()) {
        if (factory->supportsKeySystem(keySystem)) {
            cdmProxy = factory->createCDMProxy(keySystem);
            break;
        }
    }
#if USE(GSTREAMER)
    ASSERT(cdmProxy);
#endif
    return cdmProxy;
}

#if !USE(GSTREAMER)
Vector<CDMProxyFactory*> CDMProxyFactory::platformRegisterFactories()
{
    Vector<CDMProxyFactory*> factories;
    return factories;
}
#endif

bool KeyHandle::takeValueIfDifferent(KeyHandleValueVariant&& value)
{
    if (m_value != value) {
        m_value = WTFMove(value);
        return true;
    }
    return false;
}

namespace {

static String vectorToHexString(const Vector<uint8_t>& vec)
{
    StringBuilder stringBuilder;
    for (auto byte : vec)
        stringBuilder.append(pad('0', 2, hex(byte)));
    return stringBuilder.toString();
}

} // namespace {}

String KeyHandle::idAsString() const
{
    return makeString('[', vectorToHexString(m_id), ']');
}

KeyStoreIDType keyStoreBaseNextID()
{
    static KeyStoreIDType nextID = 1;
    ASSERT(isMainThread());
    return nextID++;
}

void ReferenceAwareKeyStore::unrefAllKeysFrom(const KeyStore& otherStore)
{
    for (const auto& otherKey : otherStore.values()) {
        auto findingResult = m_keys.find(otherKey->id());
        if (findingResult == m_keys.end())
            continue;
        const RefPtr<ReferenceAwareKeyHandle>& key = findingResult->value;
        RELEASE_ASSERT(key);
        key->removeReference(otherStore.id());
        if (!key->hasReferences())
            remove(key);
    }
}

void ReferenceAwareKeyStore::merge(const KeyStore& otherStore)
{
    ASSERT(isMainThread());
    for (const auto& otherKey : otherStore.values()) {
        RefPtr<ReferenceAwareKeyHandle> key = keyHandle(otherKey->id());
        auto otherReferenceAwareKey = ReferenceAwareKeyHandle::createFrom(otherKey, otherStore.id());
        if (key)
            key->updateKeyFrom(WTFMove(otherReferenceAwareKey));
        else
            add(WTFMove(otherReferenceAwareKey));
    }
}

void CDMProxy::updateKeyStore(const KeyStore& newKeyStore)
{
    Locker locker { m_keysLock };
    m_keyStore.merge(newKeyStore);
    LOG(EME, "EME - CDMProxy - updating key store from a session update");
    m_keysCondition.notifyAll();
}

const CDMInstanceProxy* CDMProxy::instance() const
{
    Locker locker { m_instanceLock };
    return m_instance;
}

void CDMProxy::unrefAllKeysFrom(const KeyStore& keyStore)
{
    Locker locker { m_keysLock };
    m_keyStore.unrefAllKeysFrom(keyStore);
    LOG(EME, "EME - CDMProxy - removing from key store from a session closure");
    m_keysCondition.notifyAll();
}

void CDMProxy::setInstance(CDMInstanceProxy* instance)
{
    Locker locker { m_instanceLock };
    m_instance = instance;
}

RefPtr<KeyHandle> CDMProxy::keyHandle(const KeyIDType& keyID) const
{
    Locker locker { m_keysLock };
    ASSERT(m_keyStore.containsKeyID(keyID));
    return m_keyStore.keyHandle(keyID);
}

void CDMProxy::startedWaitingForKey() const
{
    Locker locker { m_instanceLock };
    LOG(EME, "EME - CDMProxy - started waiting for a key");
    ASSERT(m_instance);
    m_instance->startedWaitingForKey();
}

void CDMProxy::stoppedWaitingForKey() const
{
    Locker locker { m_instanceLock };
    LOG(EME, "EME - CDMProxy - stopped waiting for a key");
    ASSERT(m_instance);
    m_instance->stoppedWaitingForKey();
}

void CDMProxy::abortWaitingForKey() const
{
    LOG(EME, "EME - CDMProxy - abort waiting for key ID");
    m_keysCondition.notifyAll();
}

std::optional<Ref<KeyHandle>> CDMProxy::tryWaitForKeyHandle(const KeyIDType& keyID, WeakPtr<CDMProxyDecryptionClient>&& client) const
{
    startedWaitingForKey();
    // Unconditionally saying we have stopped waiting for a key means that decryptors only get
    // one shot at fetching a key. If MaxKeyWaitTimeSeconds expires, that's it, no more clear bytes
    // for you.
    auto stopWaitingForKeyOnReturn = makeScopeExit([this] {
        stoppedWaitingForKey();
    });
    LOG(EME, "EME - CDMProxy - trying to wait for key ID %s", vectorToHexString(keyID).ascii().data());
    bool wasKeyAvailable = false;
    {
        Locker locker { m_keysLock };

        m_keysCondition.waitFor(m_keysLock, CDMProxy::MaxKeyWaitTimeSeconds, [this, keyID, client = WTFMove(client), &wasKeyAvailable]() {
            assertIsHeld(m_keysLock);
            if (!client || client->isAborting())
                return true;
            wasKeyAvailable = isKeyAvailableUnlocked(keyID);
            return wasKeyAvailable;
        });
    }

    if (wasKeyAvailable) {
        RefPtr<KeyHandle> handle = keyHandle(keyID);
        LOG(EME, "EME - CDMProxy - successfully waited for key ID %s", vectorToHexString(keyID).ascii().data());
        return std::make_optional(handle.releaseNonNull());
    }

    LOG(EME, "EME - CDMProxy - key ID %s not available or operation aborted", vectorToHexString(keyID).ascii().data());
    return std::nullopt;
}

bool CDMProxy::isKeyAvailableUnlocked(const KeyIDType& keyID) const
{
    return m_keyStore.containsKeyID(keyID);
}

bool CDMProxy::isKeyAvailable(const KeyIDType& keyID) const
{
    Locker locker { m_keysLock };
    return isKeyAvailableUnlocked(keyID);
}

std::optional<Ref<KeyHandle>> CDMProxy::getOrWaitForKeyHandle(const KeyIDType& keyID, WeakPtr<CDMProxyDecryptionClient>&& client) const
{
    if (!isKeyAvailable(keyID)) {
        LOG(EME, "EME - CDMProxy key cache does not contain key ID %s", vectorToHexString(keyID).ascii().data());
        return tryWaitForKeyHandle(keyID, WTFMove(client));
    }

    RefPtr<KeyHandle> handle = keyHandle(keyID);
    return std::make_optional(handle.releaseNonNull());
}

std::optional<KeyHandleValueVariant> CDMProxy::getOrWaitForKeyValue(const KeyIDType& keyID, WeakPtr<CDMProxyDecryptionClient>&& client) const
{
    if (auto keyHandle = getOrWaitForKeyHandle(keyID, WTFMove(client)))
        return std::make_optional((*keyHandle)->value());
    return std::nullopt;
}

void CDMInstanceProxy::startedWaitingForKey()
{
    ASSERT(!isMainThread());
    if (!m_player.get())
        return;

    bool wasWaitingForKey = m_numDecryptorsWaitingForKey > 0;
    m_numDecryptorsWaitingForKey++;

    callOnMainThread([weakPlayer = m_player, wasWaitingForKey] {
        auto player = weakPlayer.get();
        if (player && !wasWaitingForKey)
            player->waitingForKeyChanged();
    });
}

void CDMInstanceProxy::stoppedWaitingForKey()
{
    ASSERT(!isMainThread());
    if (!m_player.get())
        return;

    ASSERT(m_numDecryptorsWaitingForKey > 0);
    m_numDecryptorsWaitingForKey--;
    bool isNobodyWaitingForKey = !m_numDecryptorsWaitingForKey;

    callOnMainThread([weakPlayer = m_player, isNobodyWaitingForKey] {
        auto player = weakPlayer.get();
        if (player && isNobodyWaitingForKey)
            player->waitingForKeyChanged();
    });
}

void CDMInstanceProxy::mergeKeysFrom(const KeyStore& keyStore)
{
    // FIXME: Notify JS when appropriate.
    ASSERT(isMainThread());
    if (m_cdmProxy) {
        LOG(EME, "EME - CDMInstanceProxy - merging keys into proxy instance and notifying CDMProxy of changes");
        m_cdmProxy->updateKeyStore(keyStore);
    }
}

void CDMInstanceProxy::unrefAllKeysFrom(const KeyStore& keyStore)
{
    ASSERT(isMainThread());
    if (m_cdmProxy) {
        LOG(EME, "EME - CDMInstanceProxy - removing keys from proxy instance and notifying CDMProxy of changes");
        m_cdmProxy->unrefAllKeysFrom(keyStore);
    }
}

CDMInstanceSessionProxy::CDMInstanceSessionProxy(CDMInstanceProxy& instance)
    : m_instance(instance)
{
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
