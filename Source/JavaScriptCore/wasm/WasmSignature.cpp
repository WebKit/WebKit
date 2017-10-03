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

#include "config.h"
#include "WasmSignature.h"

#if ENABLE(WEBASSEMBLY)

#include <wtf/FastMalloc.h>
#include <wtf/HashFunctions.h>
#include <wtf/PrintStream.h>
#include <wtf/text/WTFString.h>

namespace JSC { namespace Wasm {

namespace {
namespace WasmSignatureInternal {
static const bool verbose = false;
}
}

String Signature::toString() const
{
    String result(makeString(returnType()));
    result.append(" (");
    for (SignatureArgCount arg = 0; arg < argumentCount(); ++arg) {
        if (arg)
            result.append(", ");
        result.append(makeString(argument(arg)));
    }
    result.append(')');
    return result;
}

void Signature::dump(PrintStream& out) const
{
    out.print(toString());
}

unsigned Signature::hash() const
{
    unsigned accumulator = 0xa1bcedd8u;
    for (uint32_t i = 0; i < argumentCount(); ++i)
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(argument(i))));
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(returnType())));
    return accumulator;
}

RefPtr<Signature> Signature::tryCreate(SignatureArgCount argumentCount)
{
    // We use WTF_MAKE_FAST_ALLOCATED for this class.
    auto result = tryFastMalloc(allocatedSize(argumentCount));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    Signature* signature = new (NotNull, memory) Signature(argumentCount);
    return adoptRef(signature);
}

SignatureInformation::SignatureInformation()
{
}

SignatureInformation& SignatureInformation::singleton()
{
    static SignatureInformation* theOne;
    static std::once_flag signatureInformationFlag;
    std::call_once(signatureInformationFlag, [] () {
        theOne = new SignatureInformation;
    });

    return *theOne;
}

std::pair<SignatureIndex, Ref<Signature>> SignatureInformation::adopt(Ref<Signature>&& signature)
{
    SignatureInformation& info = singleton();
    LockHolder lock(info.m_lock);

    SignatureIndex nextValue = info.m_nextIndex;
    auto addResult = info.m_signatureMap.add(SignatureHash { signature.ptr() }, nextValue);
    if (addResult.isNewEntry) {
        ++info.m_nextIndex;
        RELEASE_ASSERT(info.m_nextIndex > nextValue); // crash on overflow.
        ASSERT(nextValue == addResult.iterator->value);
        if (WasmSignatureInternal::verbose)
            dataLogLn("Adopt new signature ", signature.get(), " with index ", addResult.iterator->value, " hash: ", signature->hash());

        auto addResult = info.m_indexMap.add(nextValue, signature.copyRef());
        RELEASE_ASSERT(addResult.isNewEntry);
        ASSERT(info.m_indexMap.size() == info.m_signatureMap.size());
        return std::make_pair(nextValue, WTFMove(signature));
    }
    if (WasmSignatureInternal::verbose)
        dataLogLn("Existing signature ", signature.get(), " with index ", addResult.iterator->value, " hash: ", signature->hash());
    ASSERT(addResult.iterator->value != Signature::invalidIndex);
    ASSERT(info.m_indexMap.contains(addResult.iterator->value));
    return std::make_pair(addResult.iterator->value, Ref<Signature>(*info.m_indexMap.get(addResult.iterator->value)));
}

const Signature& SignatureInformation::get(SignatureIndex index)
{
    ASSERT(index != Signature::invalidIndex);
    SignatureInformation& info = singleton();
    LockHolder lock(info.m_lock);

    return *info.m_indexMap.get(index);
}

SignatureIndex SignatureInformation::get(const Signature& signature)
{
    SignatureInformation& info = singleton();
    LockHolder lock(info.m_lock);

    auto result = info.m_signatureMap.get(SignatureHash { &signature });
    ASSERT(result != Signature::invalidIndex);
    return result;
}

void SignatureInformation::tryCleanup()
{
    SignatureInformation& info = singleton();
    LockHolder lock(info.m_lock);

    Vector<std::pair<SignatureIndex, Signature*>> toRemove;
    for (const auto& pair : info.m_indexMap) {
        const Ref<Signature>& signature = pair.value;
        if (signature->refCount() == 1) {
            // We're the only owner.
            toRemove.append(std::make_pair(pair.key, signature.ptr()));
        }
    }
    for (const auto& pair : toRemove) {
        bool removed = info.m_signatureMap.remove(SignatureHash { pair.second });
        ASSERT_UNUSED(removed, removed);
        removed = info.m_indexMap.remove(pair.first);
        ASSERT_UNUSED(removed, removed);
    }
    if (info.m_signatureMap.isEmpty()) {
        ASSERT(info.m_indexMap.isEmpty());
        info.m_nextIndex = Signature::firstValidIndex;
    }
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
