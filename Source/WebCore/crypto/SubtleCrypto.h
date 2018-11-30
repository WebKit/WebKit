/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEB_CRYPTO)

#include "ContextDestructionObserver.h"
#include "CryptoKeyFormat.h"
#include <JavaScriptCore/Strong.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Variant.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>

namespace JSC {
class ArrayBufferView;
class ArrayBuffer;
class ExecState;
}

namespace WebCore {

struct JsonWebKey;

class BufferSource;
class CryptoKey;
class DeferredPromise;

enum class CryptoKeyUsage;

class SubtleCrypto : public ContextDestructionObserver, public RefCounted<SubtleCrypto>, public CanMakeWeakPtr<SubtleCrypto> {
public:
    static Ref<SubtleCrypto> create(ScriptExecutionContext* context) { return adoptRef(*new SubtleCrypto(context)); }
    ~SubtleCrypto();

    using KeyFormat = CryptoKeyFormat;

    using AlgorithmIdentifier = Variant<JSC::Strong<JSC::JSObject>, String>;
    using KeyDataVariant = Variant<RefPtr<JSC::ArrayBufferView>, RefPtr<JSC::ArrayBuffer>, JsonWebKey>;

    void encrypt(JSC::ExecState&, AlgorithmIdentifier&&, CryptoKey&, BufferSource&& data, Ref<DeferredPromise>&&);
    void decrypt(JSC::ExecState&, AlgorithmIdentifier&&, CryptoKey&, BufferSource&& data, Ref<DeferredPromise>&&);
    void sign(JSC::ExecState&, AlgorithmIdentifier&&, CryptoKey&, BufferSource&& data, Ref<DeferredPromise>&&);
    void verify(JSC::ExecState&, AlgorithmIdentifier&&, CryptoKey&, BufferSource&& signature, BufferSource&& data, Ref<DeferredPromise>&&);
    void digest(JSC::ExecState&, AlgorithmIdentifier&&, BufferSource&& data, Ref<DeferredPromise>&&);
    void generateKey(JSC::ExecState&, AlgorithmIdentifier&&, bool extractable, Vector<CryptoKeyUsage>&& keyUsages, Ref<DeferredPromise>&&);
    void deriveKey(JSC::ExecState&, AlgorithmIdentifier&&, CryptoKey& baseKey, AlgorithmIdentifier&& derivedKeyType, bool extractable, Vector<CryptoKeyUsage>&&, Ref<DeferredPromise>&&);
    void deriveBits(JSC::ExecState&, AlgorithmIdentifier&&, CryptoKey& baseKey, unsigned length, Ref<DeferredPromise>&&);
    void importKey(JSC::ExecState&, KeyFormat, KeyDataVariant&&, AlgorithmIdentifier&&, bool extractable, Vector<CryptoKeyUsage>&&, Ref<DeferredPromise>&&);
    void exportKey(KeyFormat, CryptoKey&, Ref<DeferredPromise>&&);
    void wrapKey(JSC::ExecState&, KeyFormat, CryptoKey&, CryptoKey& wrappingKey, AlgorithmIdentifier&& wrapAlgorithm, Ref<DeferredPromise>&&);
    void unwrapKey(JSC::ExecState&, KeyFormat, BufferSource&& wrappedKey, CryptoKey& unwrappingKey, AlgorithmIdentifier&& unwrapAlgorithm, AlgorithmIdentifier&& unwrappedKeyAlgorithm, bool extractable, Vector<CryptoKeyUsage>&&, Ref<DeferredPromise>&&);

private:
    explicit SubtleCrypto(ScriptExecutionContext*);

    inline friend RefPtr<DeferredPromise> getPromise(DeferredPromise*, WeakPtr<SubtleCrypto>);

    Ref<WorkQueue> m_workQueue;
    HashMap<DeferredPromise*, Ref<DeferredPromise>> m_pendingPromises;
};

}

#endif
