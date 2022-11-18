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
#include "SharedBuffer.h"
#include <wtf/BoxPtr.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>

#if ENABLE(THUNDER)
#include "CDMOpenCDMTypes.h"
#endif

namespace WebCore {

class MediaPlayer;
class SharedBuffer;

using KeyIDType = Vector<uint8_t>;
using KeyHandleValueVariant = std::variant<
    Vector<uint8_t>
#if ENABLE(THUNDER)
    , BoxPtr<OpenCDMSession>
#endif
>;

class KeyHandle : public ThreadSafeRefCounted<KeyHandle> {
public:
    using KeyStatus = CDMInstanceSession::KeyStatus;

    static RefPtr<KeyHandle> create(KeyStatus status, KeyIDType&& keyID, KeyHandleValueVariant&& keyHandleValue)
    {
        return adoptRef(*new KeyHandle(status, WTFMove(keyID), WTFMove(keyHandleValue)));
    }
    Ref<SharedBuffer> idAsSharedBuffer() const { return SharedBuffer::create(m_id.data(), m_id.size()); }

    bool takeValueIfDifferent(KeyHandleValueVariant&&);

    const KeyIDType& id() const { return m_id; }
    const KeyHandleValueVariant& value() const { return m_value; }
    KeyHandleValueVariant& value() { return m_value; }
    KeyStatus status() const { return m_status; }
    void mergeKeyInto(RefPtr<KeyHandle>&& other)
    {
        m_status = other->m_status;
        m_value = other->m_value;
        m_numSessionReferences += other->m_numSessionReferences;
    }
    bool isStatusCurrentlyValid()
    {
        return m_status == CDMInstanceSession::KeyStatus::Usable || m_status == CDMInstanceSession::KeyStatus::OutputRestricted
            || m_status == CDMInstanceSession::KeyStatus::OutputDownscaled;
    }

    String idAsString() const;

    // Two keys are equal if they have the same ID, ignoring key value and status.
    friend bool operator==(const KeyHandle &k1, const KeyHandle &k2) { return k1.m_id == k2.m_id; }
    friend bool operator==(const KeyHandle &k, const KeyIDType& keyID) { return k.m_id == keyID; }
    friend bool operator==(const KeyIDType& keyID, const KeyHandle &k) { return k == keyID; }
    friend bool operator<(const KeyHandle& k1, const KeyHandle& k2)
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
    int numSessionReferences() const { ASSERT(isMainThread()); return m_numSessionReferences; }
    bool hasReferences() const { ASSERT(isMainThread()); return m_numSessionReferences > 0; }
    friend class KeyStore;
    friend class ReferenceAwareKeyStore;

    KeyHandle(KeyStatus status, KeyIDType&& keyID, KeyHandleValueVariant&& keyHandleValue)
        : m_status(status), m_id(WTFMove(keyID)), m_value(WTFMove(keyHandleValue)) { }

    KeyStatus m_status;
    KeyIDType m_id;
    KeyHandleValueVariant m_value;
    int m_numSessionReferences { 0 };
};

class KeyStore {
public:
    using KeyStatusVector = CDMInstanceSession::KeyStatusVector;

    KeyStore() = default;
    virtual ~KeyStore() = default;

    bool containsKeyID(const KeyIDType&) const;
    void merge(const KeyStore&);
    void unrefAllKeysFrom(const KeyStore&);
    void unrefAllKeys();
    bool addKeys(Vector<RefPtr<KeyHandle>>&&);
    bool add(RefPtr<KeyHandle>&&);
    bool unref(const RefPtr<KeyHandle>&);
    bool hasKeys() const { return m_keys.size(); }
    unsigned numKeys() const { return m_keys.size(); }
    const RefPtr<KeyHandle>& keyHandle(const KeyIDType&) const;
    KeyStatusVector allKeysAs(CDMInstanceSession::KeyStatus) const;
    KeyStatusVector convertToJSKeyStatusVector() const;
    bool isEmpty() const { return m_keys.isEmpty(); }
    virtual void addSessionReferenceTo(const RefPtr<KeyHandle>&) const { }
    virtual void removeSessionReferenceFrom(const RefPtr<KeyHandle>&) const { };

    auto begin() { return m_keys.begin(); }
    auto begin() const { return m_keys.begin(); }
    auto end() { return m_keys.end(); }
    auto end() const { return m_keys.end(); }
    auto rbegin() { return m_keys.rbegin(); }
    auto rbegin() const { return m_keys.rbegin(); }
    auto rend() { return m_keys.rend(); }
    auto rend() const { return m_keys.rend(); }

private:
    Vector<RefPtr<KeyHandle>> m_keys;
};

class ReferenceAwareKeyStore : public KeyStore {
public:
    virtual ~ReferenceAwareKeyStore() = default;
    void addSessionReferenceTo(const RefPtr<KeyHandle>& key) const final { key->addSessionReference(); }
    void removeSessionReferenceFrom(const RefPtr<KeyHandle>& key) const final { key->removeSessionReference(); }
};

class CDMInstanceProxy;
class CDMProxyDecryptionClient;

// Handle to a "real" CDM, not the JavaScript facade. This can be used
// from background threads (i.e. decryptors).
class CDMProxy : public ThreadSafeRefCounted<CDMProxy> {
public:
    static constexpr Seconds MaxKeyWaitTimeSeconds = 7_s;

    virtual ~CDMProxy() = default;

    void updateKeyStore(const KeyStore&);
    void unrefAllKeysFrom(const KeyStore&);
    void setInstance(CDMInstanceProxy*);
    void abortWaitingForKey() const;

protected:
    RefPtr<KeyHandle> keyHandle(const KeyIDType&) const;
    bool keyAvailable(const KeyIDType&) const;
    bool keyAvailableUnlocked(const KeyIDType&) const WTF_REQUIRES_LOCK(m_keysLock);
    std::optional<Ref<KeyHandle>> tryWaitForKeyHandle(const KeyIDType&, WeakPtr<CDMProxyDecryptionClient>&&) const;
    std::optional<Ref<KeyHandle>> getOrWaitForKeyHandle(const KeyIDType&, WeakPtr<CDMProxyDecryptionClient>&&) const;
    std::optional<KeyHandleValueVariant> getOrWaitForKeyValue(const KeyIDType&, WeakPtr<CDMProxyDecryptionClient>&&) const;
    void startedWaitingForKey() const;
    void stoppedWaitingForKey() const;
    const CDMInstanceProxy* instance() const;

private:
    mutable Lock m_instanceLock;
    CDMInstanceProxy* m_instance WTF_GUARDED_BY_LOCK(m_instanceLock);

    mutable Lock m_keysLock;
    mutable Condition m_keysCondition;
    // FIXME: Duplicated key stores in the instance and the proxy are probably not needed, but simplified
    // the initial implementation in terms of threading invariants.
    ReferenceAwareKeyStore m_keyStore WTF_GUARDED_BY_LOCK(m_keysLock);
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

class CDMInstanceProxy;

class CDMInstanceSessionProxy : public CDMInstanceSession, public CanMakeWeakPtr<CDMInstanceSessionProxy, WeakPtrFactoryInitialization::Eager> {
protected:
    CDMInstanceSessionProxy(CDMInstanceProxy&);
    const WeakPtr<CDMInstanceProxy>& cdmInstanceProxy() const { return m_instance; }

private:
    WeakPtr<CDMInstanceProxy> m_instance;
};

// Base class for common session management code and for communicating messages
// from "real CDM" state changes to JS.
class CDMInstanceProxy : public CDMInstance, public CanMakeWeakPtr<CDMInstanceProxy> {
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
    void unrefAllKeysFrom(const KeyStore&);

    // Media player query methods - main thread only.
    const RefPtr<CDMProxy>& proxy() const { ASSERT(isMainThread()); return m_cdmProxy; }
    virtual bool isWaitingForKey() const { ASSERT(isMainThread()); return m_numDecryptorsWaitingForKey > 0; }
    void setPlayer(WeakPtr<MediaPlayer>&& player) { ASSERT(isMainThread()); m_player = WTFMove(player); }

    // Proxy methods - must be thread-safe.
    void startedWaitingForKey();
    void stoppedWaitingForKey();

private:
    RefPtr<CDMProxy> m_cdmProxy;
    WeakPtr<MediaPlayer> m_player;

    std::atomic<int> m_numDecryptorsWaitingForKey { 0 };
};

class CDMProxyDecryptionClient : public CanMakeWeakPtr<CDMProxyDecryptionClient, WeakPtrFactoryInitialization::Eager> {
public:
    virtual bool isAborting() = 0;
    virtual ~CDMProxyDecryptionClient() = default;
};

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
