/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#include "CallFrame.h"
#include "Identifier.h"
#include "Symbol.h"
#include "VM.h"

namespace JSC  {

inline Identifier::Identifier(VM& vm, std::span<const LChar> string)
    : m_string(add(vm, string))
{
    ASSERT(m_string.impl()->isAtom());
}

inline Identifier::Identifier(VM& vm, std::span<const char16_t> string)
    : m_string(add(vm, string))
{
    ASSERT(m_string.impl()->isAtom());
}

ALWAYS_INLINE Identifier::Identifier(VM& vm, ASCIILiteral literal)
    : m_string(add(vm, literal))
{
    ASSERT(m_string.impl()->isAtom());
}

inline Identifier::Identifier(VM& vm, AtomStringImpl* string)
    : m_string(string)
{
#ifndef NDEBUG
    checkCurrentAtomStringTable(vm);
    if (string)
        ASSERT_WITH_MESSAGE(!string->length() || string->isSymbol() || AtomStringImpl::isInAtomStringTable(string), "The atomic string comes from an other thread!");
#else
    UNUSED_PARAM(vm);
#endif
}

inline Identifier::Identifier(VM& vm, const AtomString& string)
    : m_string(string)
{
#ifndef NDEBUG
    checkCurrentAtomStringTable(vm);
    if (!string.isNull())
        ASSERT_WITH_MESSAGE(!string.length() || string.impl()->isSymbol() || AtomStringImpl::isInAtomStringTable(string.impl()), "The atomic string comes from an other thread!");
#else
    UNUSED_PARAM(vm);
#endif
}

inline Identifier::Identifier(VM& vm, const String& string)
    : m_string(add(vm, string.impl()))
{
    ASSERT(m_string.impl()->isAtom());
}

inline Identifier::Identifier(VM& vm, StringImpl* rep)
    : m_string(add(vm, rep))
{
    ASSERT(m_string.impl()->isAtom());
}

inline Ref<AtomStringImpl> Identifier::add(VM& vm, ASCIILiteral literal)
{
    if (literal.length() == 1)
        return vm.smallStrings.singleCharacterStringRep(literal.characterAt(0));
    return AtomStringImpl::add(literal);
}


template <typename T>
Ref<AtomStringImpl> Identifier::add(VM& vm, std::span<const T> string)
{
    if (string.size() == 1) {
        T c = string.front();
        if (canUseSingleCharacterString(c))
            return vm.smallStrings.singleCharacterStringRep(c);
    }
    if (string.empty())
        return *static_cast<AtomStringImpl*>(StringImpl::empty());

    return *AtomStringImpl::add(string);
}

inline Ref<AtomStringImpl> Identifier::add(VM& vm, StringImpl* r)
{
#ifndef NDEBUG
    checkCurrentAtomStringTable(vm);
#endif
    return *AtomStringImpl::addWithStringTableProvider(vm, r);
}

inline Identifier Identifier::fromUid(VM& vm, UniquedStringImpl* uid)
{
    if (!uid || !uid->isSymbol())
        return Identifier(vm, uid);
    return static_cast<SymbolImpl&>(*uid);
}

inline Identifier Identifier::fromUid(const PrivateName& name)
{
    return name.uid();
}

inline Identifier Identifier::fromUid(SymbolImpl& symbol)
{
    return symbol;
}

ALWAYS_INLINE Identifier Identifier::fromString(VM& vm, ASCIILiteral s)
{
    return Identifier(vm, s);
}

inline Identifier Identifier::fromString(VM& vm, std::span<const LChar> s)
{
    return Identifier(vm, s);
}

inline Identifier Identifier::fromString(VM& vm, std::span<const char16_t> s)
{
    return Identifier(vm, s);
}

inline Identifier Identifier::fromString(VM& vm, const String& string)
{
    return Identifier(vm, string.impl());
}

inline Identifier Identifier::fromString(VM& vm, AtomStringImpl* atomStringImpl)
{
    return Identifier(vm, atomStringImpl);
}

inline Identifier Identifier::fromString(VM& vm, Ref<AtomStringImpl>&& atomStringImpl)
{
    return Identifier(vm, WTFMove(atomStringImpl));
}

inline Identifier Identifier::fromString(VM& vm, const AtomString& atomString)
{
    return Identifier(vm, atomString);
}

inline Identifier Identifier::fromString(VM& vm, SymbolImpl* symbolImpl)
{
    return Identifier(vm, symbolImpl);
}

inline JSValue identifierToJSValue(VM& vm, const Identifier& identifier)
{
    if (identifier.isSymbol())
        return Symbol::create(vm, static_cast<SymbolImpl&>(*identifier.impl()));
    return jsString(vm, identifier.string());
}

inline JSValue identifierToSafePublicJSValue(VM& vm, const Identifier& identifier) 
{
    if (identifier.isSymbol() && !identifier.isPrivateName())
        return Symbol::create(vm, static_cast<SymbolImpl&>(*identifier.impl()));
    return jsString(vm, identifier.string());
}

} // namespace JSC
