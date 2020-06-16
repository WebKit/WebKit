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

#pragma once

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDMInstance.h"
#include "CDMInstanceSession.h"
#include "MediaPlayerPrivate.h"
#include "SharedBuffer.h"
#include <wtf/Condition.h>
#include <wtf/VectorHash.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class Key : public RefCounted<Key> {
public:
    using KeyStatus = CDMInstanceSession::KeyStatus;

    static RefPtr<Key> create(KeyStatus status, Vector<uint8_t>&& keyID, Vector<uint8_t>&& keyValue)
    {
        return adoptRef(*new Key(status, WTFMove(keyID), WTFMove(keyValue)));
    }
    Ref<SharedBuffer> idAsSharedBuffer() const { return SharedBuffer::create(m_id.data(), m_id.size()); }

    const Vector<uint8_t>& id() const { return m_id; }
    const Vector<uint8_t>& value() const { return m_value; }
    Vector<uint8_t>& value() { return m_value; }
    KeyStatus status() const { return m_status; }

    String idAsString() const;
    String valueAsString() const;

    // Two keys are equal if they have the same ID, ignoring key value and status.
    friend bool operator==(const Key &k1, const Key &k2) { return k1.m_id == k2.m_id; }
    friend bool operator==(const Key &k, const Vector<uint8_t>& keyID) { return k.m_id == keyID; }
    friend bool operator==(const Vector<uint8_t>& keyID, const Key &k) { return k == keyID; }
    friend bool operator<(const Key& k1, const Key& k2)
    {
        // Key IDs are compared as follows: For key IDs A of length m and
        // B of length n, assigned such that m <= n, let A < B if and only
        // if the m octets of A are less in lexicographical order than the
        // first m octets of B or those octets are equal and m < n.
        // 6.1 https://www.w3.org/TR/encrypted-media/
        int isDifference = memcmp(k1.m_id.data(), k2.m_id.data(), std::min(k1.m_id.size(), k2.m_id.size()));
        if (isDifference)
            return isDifference < 0;
        // The keys are equal to the shared length, the shorter string
        // is therefore less than the longer one in a lexicographical
        // ordering.
        return k1.m_id.size() < k2.m_id.size();
    }

private:
    void addSessionReference() { ASSERT(isMainThread()); m_numSessionReferences++; }
    void removeSessionReference() { ASSERT(isMainThread()); m_numSessionReferences--; }
    unsigned numSessionReferences() const { ASSERT(isMainThread()); return m_numSessionReferences; }
    friend class KeyStore;

    Key(KeyStatus status, Vector<uint8_t>&& keyID, Vector<uint8_t>&& keyValue)
        : m_status(status), m_id(WTFMove(keyID)), m_value(WTFMove(keyValue)) { }

    KeyStatus m_status;
    Vector<uint8_t> m_id;
    Vector<uint8_t> m_value;
    unsigned m_numSessionReferences { 0 };
};

class KeyStore {
public:
    using KeyStatusVector = CDMInstanceSession::KeyStatusVector;

    bool containsKeyID(const Vector<uint8_t>& keyID) const;
    void merge(const KeyStore& other);
    void removeAllKeysFrom(const KeyStore& other);
    void removeAllKeys() { m_keys.clear(); }
    bool addKeys(Vector<RefPtr<Key>>&&);
    bool add(RefPtr<Key>&&);
    bool remove(const RefPtr<Key>&);
    bool hasKeys() const { return m_keys.size(); }
    unsigned numKeys() const { return m_keys.size(); }
    const Vector<uint8_t>& keyValue(const Vector<uint8_t>& keyID) const;
    KeyStatusVector allKeysAsReleased() const;
    KeyStatusVector convertToJSKeyStatusVector() const;

    auto begin() { return m_keys.begin(); }
    auto begin() const { return m_keys.begin(); }
    auto end() { return m_keys.end(); }
    auto end() const { return m_keys.end(); }
    auto rbegin() { return m_keys.rbegin(); }
    auto rbegin() const { return m_keys.rbegin(); }
    auto rend() { return m_keys.rend(); }
    auto rend() const { return m_keys.rend(); }

private:
    Vector<RefPtr<Key>> m_keys;
};

class CDMInstanceProxy;

// Handle to a "real" CDM, not the JavaScript facade. This can be used
// from background threads (i.e. decryptors).
class CDMProxy : public ThreadSafeRefCounted<CDMProxy> {
public:
    static constexpr Seconds MaxKeyWaitTimeSeconds = 5_s;

    virtual ~CDMProxy() = default;

    void updateKeyStore(const KeyStore& newKeyStore);
    void setInstance(CDMInstanceProxy*);

protected:
    Vector<uint8_t> keyValue(const Vector<uint8_t>& keyID) const;
    bool keyAvailable(const Vector<uint8_t>& keyID) const;
    bool keyAvailableUnlocked(const Vector<uint8_t>& keyID) const;
    Optional<Vector<uint8_t>> tryWaitForKey(const Vector<uint8_t>& keyID) const;
    Optional<Vector<uint8_t>> getOrWaitForKey(const Vector<uint8_t>& keyID) const;
    void startedWaitingForKey() const;
    void stoppedWaitingForKey() const;

private:
    mutable Lock m_instanceMutex;
    CDMInstanceProxy* m_instance;

    mutable Lock m_keysMutex;
    mutable Condition m_keysCondition;
    // FIXME: Duplicated key stores in the instance and the proxy are probably not needed, but simplified
    // the initial implementation in terms of threading invariants.
    KeyStore m_keyStore;
};

class CDMProxyFactory {
public:
    virtual ~CDMProxyFactory()
    {
        unregisterFactory(*this);
    };

    WEBCORE_EXPORT static void registerFactory(CDMProxyFactory&);
    WEBCORE_EXPORT static void unregisterFactory(CDMProxyFactory&);
    WEBCORE_EXPORT static WARN_UNUSED_RETURN RefPtr<CDMProxy> createCDMProxyForKeySystem(const String&);

protected:
    virtual RefPtr<CDMProxy> createCDMProxy(const String&) = 0;
    virtual bool supportsKeySystem(const String&) = 0;

private:
    // Platform-specific function that's called when the list of
    // registered CDMProxyFactory objects is queried for the first time.
    static Vector<CDMProxyFactory*> platformRegisterFactories();
    WEBCORE_EXPORT static Vector<CDMProxyFactory*>& registeredFactories();
};

class CDMInstanceSessionProxy : public CDMInstanceSession, public CanMakeWeakPtr<CDMInstanceSessionProxy, WeakPtrFactoryInitialization::Eager> {
};

// Base class for common session management code and for communicating messages
// from "real CDM" state changes to JS.
class CDMInstanceProxy : public CDMInstance {
public:
    explicit CDMInstanceProxy(const String& keySystem)
    {
        ASSERT(isMainThread());
        m_cdmProxy = CDMProxyFactory::createCDMProxyForKeySystem(keySystem);
        if (m_cdmProxy)
            m_cdmProxy->setInstance(this);
    }
    virtual ~CDMInstanceProxy() = default;

    // Main-thread only.
    void mergeKeysFrom(const KeyStore&);
    void removeAllKeysFrom(const KeyStore&);

    // Media player query methods - main thread only.
    RefPtr<CDMProxy> proxy() const { ASSERT(isMainThread()); return m_cdmProxy; }
    virtual bool isWaitingForKey() const { ASSERT(isMainThread()); return m_numDecryptorsWaitingForKey > 0; }
    void setPlayer(MediaPlayer* player) { ASSERT(isMainThread()); m_player = player; }

    // Proxy methods - must be thread-safe.
    void startedWaitingForKey();
    void stoppedWaitingForKey();

protected:
    void trackSession(const CDMInstanceSessionProxy&);

private:
    RefPtr<CDMProxy> m_cdmProxy;
    // FIXME: WeakPtr for the m_player? This is accessed from background and main threads, it's
    // concerning we could be accessing it in the middle of a shutdown on the main-thread, eh?
    // As a CDMProxy, we ***should*** be turned off before this pointer ever goes bad.
    MediaPlayer* m_player { nullptr }; // FIXME: MainThread<T>?

    std::atomic<int> m_numDecryptorsWaitingForKey { 0 };
    WeakHashSet<CDMInstanceSessionProxy> m_sessions;

    KeyStore m_keyStore;
};

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
