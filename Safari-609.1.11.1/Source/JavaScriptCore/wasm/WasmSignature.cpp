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

#include "config.h"
#include "WasmSignature.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmSignatureInlines.h"
#include <wtf/CommaPrinter.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashFunctions.h>
#include <wtf/PrintStream.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/WTFString.h>

namespace JSC { namespace Wasm {

namespace {
namespace WasmSignatureInternal {
static constexpr bool verbose = false;
}
}

SignatureInformation* SignatureInformation::theOne { nullptr };
std::once_flag SignatureInformation::signatureInformationFlag;

String Signature::toString() const
{
    return WTF::toString(*this);
}

void Signature::dump(PrintStream& out) const
{
    {
        out.print("(");
        CommaPrinter comma;
        for (SignatureArgCount arg = 0; arg < argumentCount(); ++arg)
            out.print(comma, makeString(argument(arg)));
        out.print(")");
    }

    {
        CommaPrinter comma;
        out.print(" -> [");
        for (SignatureArgCount ret = 0; ret < returnCount(); ++ret)
            out.print(comma, makeString(returnType(ret)));
        out.print("]");
    }
}

static unsigned computeHash(size_t returnCount, const Type* returnTypes, size_t argumentCount, const Type* argumentTypes)
{
    unsigned accumulator = 0xa1bcedd8u;
    for (uint32_t i = 0; i < argumentCount; ++i)
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(argumentTypes[i])));
    for (uint32_t i = 0; i < returnCount; ++i)
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(returnTypes[i])));
    return accumulator;
}

unsigned Signature::hash() const
{
    return computeHash(returnCount(), storage(0), argumentCount(), storage(returnCount()));
}

RefPtr<Signature> Signature::tryCreate(SignatureArgCount returnCount, SignatureArgCount argumentCount)
{
    // We use WTF_MAKE_FAST_ALLOCATED for this class.
    auto result = tryFastMalloc(allocatedSize(returnCount, argumentCount));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    Signature* signature = new (NotNull, memory) Signature(returnCount, argumentCount);
    return adoptRef(signature);
}

SignatureInformation::SignatureInformation()
{
#define MAKE_THUNK_SIGNATURE(type, enc, str, val)                          \
    do {                                                                   \
        if (type != Void) {                                                \
            RefPtr<Signature> sig = Signature::tryCreate(1, 0);            \
            sig->ref();                                                    \
            sig->getReturnType(0) = type;                                  \
            thunkSignatures[linearizeType(type)] = sig.get();              \
            m_signatureSet.add(SignatureHash { sig.releaseNonNull() });    \
        }                                                                  \
    } while (false);

    FOR_EACH_WASM_TYPE(MAKE_THUNK_SIGNATURE);

    // Make Void again because we don't use the one that has void in it.
    {
        RefPtr<Signature> sig = Signature::tryCreate(0, 0);
        sig->ref();
        thunkSignatures[linearizeType(Void)] = sig.get();
        m_signatureSet.add(SignatureHash { sig.releaseNonNull() });
    }
}



struct ParameterTypes {
    const Vector<Type, 1>& returnTypes;
    const Vector<Type>& argumentTypes;

    static unsigned hash(const ParameterTypes& params)
    {
        return computeHash(params.returnTypes.size(), params.returnTypes.data(), params.argumentTypes.size(), params.argumentTypes.data());
    }

    static bool equal(const SignatureHash& sig, const ParameterTypes& params)
    {
        if (sig.key->argumentCount() != params.argumentTypes.size())
            return false;
        if (sig.key->returnCount() != params.returnTypes.size())
            return false;

        for (unsigned i = 0; i < sig.key->argumentCount(); ++i) {
            if (sig.key->argument(i) != params.argumentTypes[i])
                return false;
        }

        for (unsigned i = 0; i < sig.key->returnCount(); ++i) {
            if (sig.key->returnType(i) != params.returnTypes[i])
                return false;
        }
        return true;
    }

    static void translate(SignatureHash& entry, const ParameterTypes& params, unsigned)
    {
        RefPtr<Signature> signature = Signature::tryCreate(params.returnTypes.size(), params.argumentTypes.size());
        RELEASE_ASSERT(signature);

        for (unsigned i = 0; i < params.returnTypes.size(); ++i)
            signature->getReturnType(i) = params.returnTypes[i];

        for (unsigned i = 0; i < params.argumentTypes.size(); ++i)
            signature->getArgument(i) = params.argumentTypes[i];

        entry.key = WTFMove(signature);
    }
};

RefPtr<Signature> SignatureInformation::signatureFor(const Vector<Type, 1>& results, const Vector<Type>& args)
{
    SignatureInformation& info = singleton();
    LockHolder lock(info.m_lock);

    auto addResult = info.m_signatureSet.template add<ParameterTypes>(ParameterTypes { results, args });
    return makeRef(*addResult.iterator->key);
}

void SignatureInformation::tryCleanup()
{
    SignatureInformation& info = singleton();
    LockHolder lock(info.m_lock);

    info.m_signatureSet.removeIf([&] (auto& hash) {
        const auto& signature = hash.key;
        return signature->refCount() == 1;
    });
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
