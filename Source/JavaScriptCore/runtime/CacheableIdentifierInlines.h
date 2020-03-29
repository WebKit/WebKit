/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "CacheableIdentifier.h"

#include "Identifier.h"
#include "JSCJSValueInlines.h"
#include "JSCell.h"
#include "VM.h"
#include <wtf/text/UniquedStringImpl.h>

namespace JSC {

inline CacheableIdentifier CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(CodeBlock* codeBlock, const Identifier& i)
{
    return createFromIdentifierOwnedByCodeBlock(codeBlock, i.impl());
}

inline CacheableIdentifier CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(CodeBlock* codeBlock, UniquedStringImpl* uid)
{
    UNUSED_PARAM(codeBlock);
#if ASSERT_ENABLED
    bool found = false;
    for (unsigned index = 0; index < codeBlock->numberOfIdentifiers(); ++index) {
        const Identifier& identifier = codeBlock->identifier(index);
        if (identifier.impl() == uid) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif
    return CacheableIdentifier(uid);
}

inline CacheableIdentifier CacheableIdentifier::createFromImmortalIdentifier(UniquedStringImpl* uid)
{
    return CacheableIdentifier(uid);
}

inline CacheableIdentifier CacheableIdentifier::createFromCell(JSCell* i)
{
    return CacheableIdentifier(i);
}

inline CacheableIdentifier::CacheableIdentifier(UniquedStringImpl* uid)
{
    setUidBits(uid);
}

inline CacheableIdentifier::CacheableIdentifier(JSCell* identifier)
{
    ASSERT(isCacheableIdentifierCell(identifier));
    setCellBits(identifier);
}

inline JSCell* CacheableIdentifier::cell() const
{
    ASSERT(isCell());
    return bitwise_cast<JSCell*>(m_bits);
}

inline UniquedStringImpl* CacheableIdentifier::uid() const
{
    if (!m_bits)
        return nullptr;
    if (isUid())
        return bitwise_cast<UniquedStringImpl*>(m_bits & ~s_uidTag);
    if (isSymbolCell())
        return &jsCast<Symbol*>(cell())->uid();
    ASSERT(isStringCell());
    JSString* string = jsCast<JSString*>(cell());
    return bitwise_cast<UniquedStringImpl*>(string->getValueImpl());
}

inline bool CacheableIdentifier::isCacheableIdentifierCell(JSCell* cell)
{
    if (cell->isSymbol())
        return true;
    if (!cell->isString())
        return false;
    JSString* string = jsCast<JSString*>(cell);
    if (const StringImpl* impl = string->tryGetValueImpl())
        return impl->isAtom();
    return false;
}

inline bool CacheableIdentifier::isCacheableIdentifierCell(JSValue value)
{
    if (!value.isCell())
        return false;
    return isCacheableIdentifierCell(value.asCell());
}

inline bool CacheableIdentifier::isSymbolCell() const
{
    return isCell() && cell()->isSymbol();
}

inline bool CacheableIdentifier::isStringCell() const
{
    return isCell() && cell()->isString();
}

inline void CacheableIdentifier::setCellBits(JSCell* cell)
{
    RELEASE_ASSERT(isCacheableIdentifierCell(cell));
    m_bits = bitwise_cast<uintptr_t>(cell);
}

inline void CacheableIdentifier::setUidBits(UniquedStringImpl* uid)
{
    m_bits = bitwise_cast<uintptr_t>(uid) | s_uidTag;
}

inline void CacheableIdentifier::visitAggregate(SlotVisitor& visitor) const
{
    if (m_bits && isCell())
        visitor.appendUnbarriered(cell());
}


inline bool CacheableIdentifier::operator==(const CacheableIdentifier& other) const
{
    return uid() == other.uid();
}

inline bool CacheableIdentifier::operator!=(const CacheableIdentifier& other) const
{
    return uid() != other.uid();
}

inline bool CacheableIdentifier::operator==(const Identifier& other) const
{
    return uid() == other.impl();
}

} // namespace JSC
