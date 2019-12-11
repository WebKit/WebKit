/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "B3Type.h"
#include "WasmOps.h"
#include <cstdint>
#include <cstring>
#include <wtf/CheckedArithmetic.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace WTF {
class PrintStream;
}

namespace JSC {

namespace Wasm {

using SignatureArgCount = uint32_t;
using SignatureIndex = uint64_t;

class Signature : public ThreadSafeRefCounted<Signature> {
    WTF_MAKE_FAST_ALLOCATED;

    Signature() = delete;
    Signature(const Signature&) = delete;
    Signature(SignatureArgCount retCount, SignatureArgCount argCount)
        : m_retCount(retCount)
        , m_argCount(argCount)
    {
    }

    Type* storage(SignatureArgCount i)
    {
        return i + reinterpret_cast<Type*>(reinterpret_cast<char*>(this) + sizeof(Signature));
    }
    Type* storage(SignatureArgCount i) const { return const_cast<Signature*>(this)->storage(i); }
    static size_t allocatedSize(Checked<SignatureArgCount> retCount, Checked<SignatureArgCount> argCount)
    {
        return (sizeof(Signature) + (retCount + argCount) * sizeof(Type)).unsafeGet();
    }

public:
    SignatureArgCount returnCount() const { return m_retCount; }
    SignatureArgCount argumentCount() const { return m_argCount; }

    Type returnType(SignatureArgCount i) const { ASSERT(i < returnCount()); return const_cast<Signature*>(this)->getReturnType(i); }
    bool returnsVoid() const { return !returnCount(); }
    Type argument(SignatureArgCount i) const { return const_cast<Signature*>(this)->getArgument(i); }
    SignatureIndex index() const { return bitwise_cast<SignatureIndex>(this); }

    WTF::String toString() const;
    void dump(WTF::PrintStream& out) const;
    bool operator==(const Signature& rhs) const { return this == &rhs; }
    unsigned hash() const;

    // Signatures are uniqued and, for call_indirect, validated at runtime. Tables can create invalid SignatureIndex values which cause call_indirect to fail. We use 0 as the invalidIndex so that the codegen can easily test for it and trap, and we add a token invalid entry in SignatureInformation.
    static const constexpr SignatureIndex invalidIndex = 0;

private:
    friend class SignatureInformation;
    friend struct ParameterTypes;

    static RefPtr<Signature> tryCreate(SignatureArgCount returnCount, SignatureArgCount argumentCount);
    Type& getReturnType(SignatureArgCount i) { ASSERT(i < returnCount()); return *storage(i); }
    Type& getArgument(SignatureArgCount i) { ASSERT(i < argumentCount()); return *storage(returnCount() + i); }

    SignatureArgCount m_retCount;
    SignatureArgCount m_argCount;
    // Return Type and arguments are stored here.
};

struct SignatureHash {
    RefPtr<Signature> key { nullptr };
    SignatureHash() = default;
    explicit SignatureHash(Ref<Signature>&& key)
        : key(WTFMove(key))
    { }
    explicit SignatureHash(WTF::HashTableDeletedValueType)
        : key(WTF::HashTableDeletedValue)
    { }
    bool operator==(const SignatureHash& rhs) const { return equal(*this, rhs); }
    static bool equal(const SignatureHash& lhs, const SignatureHash& rhs) { return lhs.key == rhs.key; }
    static unsigned hash(const SignatureHash& signature) { return signature.key ? signature.key->hash() : 0; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
    bool isHashTableDeletedValue() const { return key.isHashTableDeletedValue(); }
};

} } // namespace JSC::Wasm


namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::Wasm::SignatureHash> {
    typedef JSC::Wasm::SignatureHash Hash;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::Wasm::SignatureHash> : SimpleClassHashTraits<JSC::Wasm::SignatureHash> {
    static constexpr bool emptyValueIsZero = true;
};

} // namespace WTF


namespace JSC { namespace Wasm {

// Signature information is held globally and shared by the entire process to allow all signatures to be unique. This is required when wasm calls another wasm instance, and must work when modules are shared between multiple VMs.
class SignatureInformation {
    WTF_MAKE_NONCOPYABLE(SignatureInformation);
    WTF_MAKE_FAST_ALLOCATED;

    SignatureInformation();

public:
    static SignatureInformation& singleton();

    static RefPtr<Signature> signatureFor(const Vector<Type, 1>& returnTypes, const Vector<Type>& argumentTypes);
    ALWAYS_INLINE const Signature* thunkFor(Type type) const { return thunkSignatures[linearizeType(type)]; }

    static const Signature& get(SignatureIndex);
    static SignatureIndex get(const Signature&);
    static void tryCleanup();

private:
    HashSet<Wasm::SignatureHash> m_signatureSet;
    const Signature* thunkSignatures[numTypes];
    Lock m_lock;

    JS_EXPORT_PRIVATE static SignatureInformation* theOne;
    JS_EXPORT_PRIVATE static std::once_flag signatureInformationFlag;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
