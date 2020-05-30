/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
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
#include "ParserArena.h"

#include "CatchScope.h"
#include "JSBigInt.h"
#include "JSCInlines.h"
#include "Nodes.h"

namespace JSC {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(IdentifierArena);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ParserArena);

ParserArena::ParserArena()
    : m_freeableMemory(nullptr)
    , m_freeablePoolEnd(nullptr)
{
}

inline void* ParserArena::freeablePool()
{
    ASSERT(m_freeablePoolEnd);
    return m_freeablePoolEnd - freeablePoolSize;
}

inline void ParserArena::deallocateObjects()
{
    size_t size = m_deletableObjects.size();
    for (size_t i = 0; i < size; ++i)
        m_deletableObjects[i]->~ParserArenaDeletable();

    if (m_freeablePoolEnd)
        ParserArenaMalloc::free(freeablePool());

    size = m_freeablePools.size();
    for (size_t i = 0; i < size; ++i)
        ParserArenaMalloc::free(m_freeablePools[i]);
}

ParserArena::~ParserArena()
{
    deallocateObjects();
}

void ParserArena::allocateFreeablePool()
{
    if (m_freeablePoolEnd)
        m_freeablePools.append(freeablePool());

    char* pool = static_cast<char*>(ParserArenaMalloc::malloc(freeablePoolSize));
    m_freeableMemory = pool;
    m_freeablePoolEnd = pool + freeablePoolSize;
    ASSERT(freeablePool() == pool);
}

const Identifier& IdentifierArena::makeBigIntDecimalIdentifier(VM& vm, const Identifier& identifier, uint8_t radix)
{
    if (radix == 10)
        return identifier;

    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSValue bigInt = JSBigInt::parseInt(nullptr, vm, identifier.string(), radix, JSBigInt::ErrorParseMode::ThrowExceptions, JSBigInt::ParseIntSign::Unsigned);
    scope.assertNoException();

    // FIXME: We are allocating a JSBigInt just to be able to use
    // JSBigInt::tryGetString when radix is not 10.
    // This creates some GC pressure, but since these identifiers
    // will only be created when BigInt literal is used as a property name,
    // it wont be much problematic, given such cases are very rare.
    // There is a lot of optimizations we can apply here when necessary.
    // https://bugs.webkit.org/show_bug.cgi?id=207627
    JSBigInt* heapBigInt;
#if USE(BIGINT32)
    if (bigInt.isBigInt32()) {
        heapBigInt = JSBigInt::tryCreateFrom(vm, bigInt.bigInt32AsInt32());
        RELEASE_ASSERT(heapBigInt);
    } else
#endif
        heapBigInt = bigInt.asHeapBigInt();

    m_identifiers.append(Identifier::fromString(vm, JSBigInt::tryGetString(vm, heapBigInt, 10)));
    return m_identifiers.last();
}

}
