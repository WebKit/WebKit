/*
 * Copyright (C) 2020 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
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
#include <wtf/HexNumber.h>
#include <wtf/Scope.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

Vector<CDMProxyFactory*>& CDMProxyFactory::registeredFactories()
{
    static auto factories = makeNeverDestroyed(platformRegisterFactories());
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
    return makeString("[", vectorToHexString(m_id), "]");
}

bool KeyHandle::takeValueIfDifferent(KeyHandleValueVariant&& value)
{
    if (m_value != value) {
        m_value = WTFMove(value);
        return true;
    }
    return false;
}

bool KeyStore::containsKeyID(const KeyIDType& keyID) const
{
    return m_keys.findMatching([&](const RefPtr<KeyHandle>& storedKey) {
        return *storedKey == keyID;
    }) != WTF::notFound;
}

void KeyStore::merge(const KeyStore& other)
{
    ASSERT(isMainThread());
    LOG(EME, "EME - CDMProxy - merging %u new keys into a key store of %u keys", other.numKeys(), numKeys());
    for (const auto& key : other) {
        // NOTE: Do we care that we will not append a key if it matches a key ID
        // in the keystore and has different data. Should we overwrite? Which is "newer"?
        // Don't think we need this extra complexity.
        size_t keyWithMatchingKeyIDIndex = m_keys.findMatching([&] (const RefPtr<KeyHandle>& storedKey) { return *key == *storedKey; });
        if (keyWithMatchingKeyIDIndex == WTF::notFound)
            m_keys.append(key);
        else {
            LOG(EME, "EME - CDMProxy - keys with the same ID!");
            ASSERT(key->value() == m_keys[keyWithMatchingKeyIDIndex]->value());
        }
    }

#if !LOG_DISABLED
    LOG(EME, "EME - CDMProxy - key store now has %u keys", numKeys());
    for (const auto& key : m_keys)
        LOG(EME, "\tEME - CDMProxy - Key ID: %s", key->idAsString().ascii().data());
#endif // !LOG_DISABLED
}

CDMInstanceSession::KeyStatusVector KeyStore::allKeysAs(CDMInstanceSession::KeyStatus status) const
{
    CDMInstanceSession::KeyStatusVector keyStatusVector = convertToJSKeyStatusVector();
    for (auto& keyStatus : keyStatusVector)
        keyStatus.second = status;
    return keyStatusVector;
}

bool KeyStore::addKeys(Vector<RefPtr<KeyHandle>>&& newKeys)
{
    bool didKeyStoreChange = false;
    for (auto& key : newKeys) {
        if (add(WTFMove(key)))
            didKeyStoreChange = true;
    }
    return didKeyStoreChange;
}

bool KeyStore::add(RefPtr<KeyHandle>&& key)
{
    bool didStoreChange = false;
    size_t keyWithMatchingKeyIDIndex = m_keys.findMatching([&] (const RefPtr<KeyHandle>& storedKey) {
        return *key == *storedKey;
    });

    if (keyWithMatchingKeyIDIndex != WTF::notFound) {
        auto& keyWithMatchingKeyID = m_keys[keyWithMatchingKeyIDIndex];
        didStoreChange = keyWithMatchingKeyID == key;
        if (didStoreChange)
            keyWithMatchingKeyID = key;
    } else {
        LOG(EME, "EME - ClearKey - New key with ID %s getting added to key store", key->idAsString().ascii().data());      
        m_keys.append(key);
        didStoreChange = true;
    }

    if (didStoreChange) {
        // Sort the keys lexicographically.
        // NOTE: This is not as pathological as it may seem, for all
        // practical purposes the store has a maximum of 2 keys.
        std::sort(m_keys.begin(), m_keys.end(),
            [](const RefPtr<KeyHandle>& a, const RefPtr<KeyHandle>& b) {
                return *a < *b;
            });
    }

    key->addSessionReference();
    return didStoreChange;
}

void KeyStore::removeAllKeysFrom(const KeyStore& other)
{
    for (const auto& key : other)
        remove(key);
}

bool KeyStore::remove(const RefPtr<KeyHandle>& key)
{
    bool storeChanged = false;

    size_t keyWithMatchingKeyIDIndex = m_keys.find(key);
    LOG(EME, "EME - ClearKey - requested to remove key with ID %s and %u session references", key->idAsString().ascii().data(), key->numSessionReferences());

    if (keyWithMatchingKeyIDIndex != WTF::notFound) {
        auto& keyWithMatchingKeyID = m_keys[keyWithMatchingKeyIDIndex];
        keyWithMatchingKeyID->removeSessionReference();
        if (!keyWithMatchingKeyID->numSessionReferences()) {
            LOG(EME, "EME - ClearKey - remove key with ID %s", keyWithMatchingKeyID->idAsString().ascii().data());
            m_keys.remove(keyWithMatchingKeyIDIndex);
            storeChanged = true;
        }
    } else
        LOG(EME, "EME - ClearKey - attempt to remove key with ID %s ignored, does not exist", key->idAsString().ascii().data());

    return storeChanged;
}

const RefPtr<KeyHandle>& KeyStore::keyHandle(const KeyIDType& keyID) const
{
    for (const auto& key : m_keys) {
        if (*key == keyID)
            return key;
    }
    
    RELEASE_ASSERT(false && "key must exist to call this method");
    UNREACHABLE();
}

CDMInstanceSession::KeyStatusVector KeyStore::convertToJSKeyStatusVector() const
{
    CDMInstanceSession::KeyStatusVector keyStatusVector;
    keyStatusVector.reserveInitialCapacity(numKeys());
    for (const auto& key : m_keys)
        keyStatusVector.uncheckedAppend(std::pair<Ref<SharedBuffer>, CDMInstanceSession::KeyStatus> { key->idAsSharedBuffer(), key->status() });
    return keyStatusVector;
}

void CDMProxy::updateKeyStore(const KeyStore& newKeyStore)
{
    auto locker = holdLock(m_keysMutex);
    m_keyStore.merge(newKeyStore);
    LOG(EME, "EME - CDMProxy - updating key store from a session update");
    m_keysCondition.notifyAll();
}

void CDMProxy::setInstance(CDMInstanceProxy* instance)
{
    auto locker = holdLock(m_instanceMutex);
    m_instance = instance;
}

RefPtr<KeyHandle> CDMProxy::keyHandle(const KeyIDType& keyID) const
{
    auto locker = holdLock(m_keysMutex);
    ASSERT(m_keyStore.containsKeyID(keyID));
    return m_keyStore.keyHandle(keyID);
}

void CDMProxy::startedWaitingForKey() const
{
    auto locker = holdLock(m_instanceMutex);
    LOG(EME, "EME - CDMProxy - started waiting for a key");
    ASSERT(m_instance);
    m_instance->startedWaitingForKey();
}

void CDMProxy::stoppedWaitingForKey() const
{
    auto locker = holdLock(m_instanceMutex);
    LOG(EME, "EME - CDMProxy - stopped waiting for a key");
    ASSERT(m_instance);
    m_instance->stoppedWaitingForKey();
}

Optional<Ref<KeyHandle>> CDMProxy::tryWaitForKeyHandle(const KeyIDType& keyID) const
{
    startedWaitingForKey();
    // Unconditionally saying we have stopped waiting for a key means that decryptors only get
    // one shot at fetching a key. If MaxKeyWaitTimeSeconds expires, that's it, no more clear bytes
    // for you.
    auto stopWaitingForKeyOnReturn = makeScopeExit([this] {
        stoppedWaitingForKey();
    });
    LOG(EME, "EME - CDMProxy - trying to wait for key ID %s", vectorToHexString(keyID).ascii().data());
    bool wasKeyReceived = false;
    {
        auto locker = holdLock(m_keysMutex);
        wasKeyReceived = m_keysCondition.waitFor(m_keysMutex, CDMProxy::MaxKeyWaitTimeSeconds, [&, this, keyID]() {
            return keyAvailableUnlocked(keyID);
        });
    }

    if (wasKeyReceived) {
        LOG(EME, "EME - CDMProxy - successfully waited for key ID %s", vectorToHexString(keyID).ascii().data());
        RefPtr<KeyHandle> handle = keyHandle(keyID);
        return makeOptional(handle.releaseNonNull());
    }
    
    LOG(EME, "EME - CDMProxy - key ID %s not available", vectorToHexString(keyID).ascii().data());
    return WTF::nullopt;
}

bool CDMProxy::keyAvailableUnlocked(const KeyIDType& keyID) const
{
    return m_keyStore.containsKeyID(keyID);
}

bool CDMProxy::keyAvailable(const KeyIDType& keyID) const
{
    auto locker = holdLock(m_keysMutex);
    return keyAvailableUnlocked(keyID);
}

Optional<Ref<KeyHandle>> CDMProxy::getOrWaitForKeyHandle(const KeyIDType& keyID) const
{
    if (!keyAvailable(keyID)) {
        LOG(EME, "EME - CDMProxy key cache does not contain key ID %s", vectorToHexString(keyID).ascii().data());
        return tryWaitForKeyHandle(keyID);
    }

    RefPtr<KeyHandle> handle = keyHandle(keyID);
    return makeOptional(handle.releaseNonNull());
}

Optional<KeyHandleValueVariant> CDMProxy::getOrWaitForKeyValue(const KeyIDType& keyID) const
{
    if (auto keyHandle = getOrWaitForKeyHandle(keyID))
        return makeOptional((*keyHandle)->value());
    return WTF::nullopt;
}

void CDMInstanceProxy::startedWaitingForKey()
{
    ASSERT(!isMainThread());
    ASSERT(m_player);

    bool wasWaitingForKey = m_numDecryptorsWaitingForKey > 0;
    m_numDecryptorsWaitingForKey++;

    callOnMainThread([player = m_player, wasWaitingForKey] {
        if (player && !wasWaitingForKey)
            player->waitingForKeyChanged();
    });
}

void CDMInstanceProxy::stoppedWaitingForKey()
{
    ASSERT(!isMainThread());
    ASSERT(m_player);
    ASSERT(m_numDecryptorsWaitingForKey > 0);
    m_numDecryptorsWaitingForKey--;
    bool isNobodyWaitingForKey = !m_numDecryptorsWaitingForKey;

    callOnMainThread([player = m_player, isNobodyWaitingForKey] {
        if (player && isNobodyWaitingForKey)
            player->waitingForKeyChanged();
    });
}

void CDMInstanceProxy::mergeKeysFrom(const KeyStore& keyStore)
{
    // FIXME: Notify JS when appropriate.
    ASSERT(isMainThread());
    m_keyStore.merge(keyStore);
    if (m_cdmProxy) {
        LOG(EME, "EME - CDMInstanceProxy - merging keys into proxy instance and notifying CDMProxy of changes");
        m_cdmProxy->updateKeyStore(keyStore);
    }
}

void CDMInstanceProxy::removeAllKeysFrom(const KeyStore& keyStore)
{
    ASSERT(isMainThread());
    m_keyStore.removeAllKeysFrom(keyStore);
}

void CDMInstanceProxy::trackSession(const CDMInstanceSessionProxy& session)
{
    ASSERT(!m_sessions.contains(session));
    m_sessions.add(session);
}

CDMInstanceSessionProxy::CDMInstanceSessionProxy(CDMInstanceProxy& instance)
    : m_instance(makeWeakPtr(instance))
{
}

void CDMInstanceSessionProxy::removeFromInstanceProxy()
{
    if (!m_instance)
        return;
    m_instance->removeSession(*this);
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
