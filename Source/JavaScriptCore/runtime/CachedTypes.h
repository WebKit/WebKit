/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "JSCast.h"
#include "VariableEnvironment.h"
#include <wtf/HashMap.h>
#include <wtf/MallocPtr.h>

namespace JSC {

class CachedBytecode;
class SourceCodeKey;
class UnlinkedCodeBlock;
class UnlinkedFunctionCodeBlock;

class Decoder : public RefCounted<Decoder> {
    WTF_MAKE_NONCOPYABLE(Decoder);

public:
    static Ref<Decoder> create(VM&, const void*, size_t);

    ~Decoder();

    VM& vm() { return m_vm; }

    ptrdiff_t offsetOf(const void*);
    void cacheOffset(ptrdiff_t, void*);
    WTF::Optional<void*> cachedPtrForOffset(ptrdiff_t);
    const void* ptrForOffsetFromBase(ptrdiff_t);
    CompactVariableMap::Handle handleForEnvironment(CompactVariableEnvironment*) const;
    void setHandleForEnvironment(CompactVariableEnvironment*, const CompactVariableMap::Handle&);

    template<typename Functor>
    void addFinalizer(const Functor&);

private:
    Decoder(VM&, const void*, size_t);

    VM& m_vm;
    const uint8_t* m_baseAddress;
#ifndef NDEBUG
    size_t m_size;
#endif
    HashMap<ptrdiff_t, void*> m_offsetToPtrMap;
    Vector<std::function<void()>> m_finalizers;
    HashMap<CompactVariableEnvironment*, CompactVariableMap::Handle> m_environmentToHandleMap;
};

enum class SourceCodeType;

std::pair<MallocPtr<uint8_t>, size_t> encodeCodeBlock(VM&, const SourceCodeKey&, const UnlinkedCodeBlock*);

UnlinkedCodeBlock* decodeCodeBlockImpl(VM&, const SourceCodeKey&, const void*, size_t);

void decodeFunctionCodeBlock(Decoder&, int32_t cachedFunctionCodeBlockOffset, WriteBarrier<UnlinkedFunctionCodeBlock>&, const JSCell*);

template<typename UnlinkedCodeBlockType>
UnlinkedCodeBlockType* decodeCodeBlock(VM& vm, const SourceCodeKey& key, const void* buffer, size_t size)
{
    return jsCast<UnlinkedCodeBlockType*>(decodeCodeBlockImpl(vm, key, buffer, size));
}

bool isCachedBytecodeStillValid(VM&, const CachedBytecode&, const SourceCodeKey&, SourceCodeType);

} // namespace JSC
