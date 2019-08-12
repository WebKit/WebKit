/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
    static const constexpr SignatureArgCount s_retCount = 1;

    Signature() = delete;
    Signature(const Signature&) = delete;
    Signature(SignatureArgCount argCount)
        : m_argCount(argCount)
    {
    }

    Type* storage(SignatureArgCount i)
    {
        return i + reinterpret_cast<Type*>(reinterpret_cast<char*>(this) + sizeof(Signature));
    }
    Type* storage(SignatureArgCount i) const { return const_cast<Signature*>(this)->storage(i); }
    static size_t allocatedSize(Checked<SignatureArgCount> argCount)
    {
        return (sizeof(Signature) + (s_retCount + argCount) * sizeof(Type)).unsafeGet();
    }

public:
    Type& returnType() { return *storage(0); }
    Type returnType() const { return *storage(0); }
    SignatureArgCount returnCount() const { return s_retCount; }
    SignatureArgCount argumentCount() const { return m_argCount; }
    Type& argument(SignatureArgCount i)
    {
        ASSERT(i < argumentCount());
        return *storage(returnCount() + i);
    }
    Type argument(SignatureArgCount i) const { return const_cast<Signature*>(this)->argument(i); }
    SignatureIndex index() const { return bitwise_cast<SignatureIndex>(this); }

    WTF::String toString() const;
    void dump(WTF::PrintStream& out) const;
    bool operator==(const Signature& rhs) const
    {
        if (argumentCount() != rhs.argumentCount())
            return false;
        if (returnType() != rhs.returnType())
            return false;
        for (unsigned i = 0; i < argumentCount(); ++i) {
            if (argument(i) != rhs.argument(i))
                return false;
        }
        return true;
    }
    unsigned hash() const;

    static RefPtr<Signature> tryCreate(SignatureArgCount);

    // Signatures are uniqued and, for call_indirect, validated at runtime. Tables can create invalid SignatureIndex values which cause call_indirect to fail. We use 0 as the invalidIndex so that the codegen can easily test for it and trap, and we add a token invalid entry in SignatureInformation.
    static const constexpr SignatureIndex invalidIndex = 0;

private:
    friend class SignatureInformation;
    SignatureArgCount m_argCount;
    // Return Type and arguments are stored here.
};

struct SignatureHash {
    RefPtr<Signature> key { nullptr };
    SignatureHash() = default;
    explicit SignatureHash(Ref<Signature>&& key)
        : key(WTFMove(key))
    {
    }
    explicit SignatureHash(WTF::HashTableDeletedValueType)
        : key(WTF::HashTableDeletedValue)
    {
    }
    bool operator==(const SignatureHash& rhs) const { return equal(*this, rhs); }
    static bool equal(const SignatureHash& lhs, const SignatureHash& rhs) { return lhs.key == rhs.key || (lhs.key && rhs.key && *lhs.key == *rhs.key); }
    static unsigned hash(const SignatureHash& signature) { return signature.key ? signature.key->hash() : 0; }
    static const bool safeToCompareToEmptyOrDeleted = false;
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
    static const bool emptyValueIsZero = true;
};

} // namespace WTF


namespace JSC { namespace Wasm {

// Signature information is held globally and shared by the entire process to allow all signatures to be unique. This is required when wasm calls another wasm instance, and must work when modules are shared between multiple VMs.
// Note: signatures are never removed because that would require accounting for all WebAssembly.Module and which signatures they use. The maximum number of signatures is bounded, and isn't worth the counting overhead. We could clear everything when we reach zero outstanding WebAssembly.Module. https://bugs.webkit.org/show_bug.cgi?id=166037
class SignatureInformation {
    WTF_MAKE_NONCOPYABLE(SignatureInformation);
    WTF_MAKE_FAST_ALLOCATED;

    SignatureInformation();

public:
    static SignatureInformation& singleton();

    static Ref<Signature> WARN_UNUSED_RETURN adopt(Ref<Signature>&&);
    static const Signature& WARN_UNUSED_RETURN get(SignatureIndex);
    static SignatureIndex WARN_UNUSED_RETURN get(const Signature&);
    static void tryCleanup();

private:
    HashSet<Wasm::SignatureHash> m_signatureSet;
    Lock m_lock;

    JS_EXPORT_PRIVATE static SignatureInformation* theOne;
    JS_EXPORT_PRIVATE static std::once_flag signatureInformationFlag;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
