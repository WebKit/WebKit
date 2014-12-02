/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef IdentifierInlines_h
#define IdentifierInlines_h

#include "CallFrame.h"
#include "Identifier.h"

namespace JSC  {

inline Identifier::Identifier(ExecState* exec, AtomicStringImpl* string)
    : m_string(string)
{
#ifndef NDEBUG
    checkCurrentAtomicStringTable(exec);
    if (string)
        ASSERT_WITH_MESSAGE(!string->length() || AtomicString::isInAtomicStringTable(string), "The atomic string comes from an other thread!");
#else
    UNUSED_PARAM(exec);
#endif
}

inline Identifier::Identifier(ExecState* exec, const AtomicString& string)
    : m_string(string.string())
{
#ifndef NDEBUG
    checkCurrentAtomicStringTable(exec);
    if (!string.isNull())
        ASSERT_WITH_MESSAGE(!string.length() || AtomicString::isInAtomicStringTable(string.impl()), "The atomic string comes from an other thread!");
#else
    UNUSED_PARAM(exec);
#endif
}

inline PassRef<StringImpl> Identifier::add(ExecState* exec, StringImpl* r)
{
#ifndef NDEBUG
    checkCurrentAtomicStringTable(exec);
#endif
    return *AtomicString::addWithStringTableProvider(*exec, r);
}
inline PassRef<StringImpl> Identifier::add(VM* vm, StringImpl* r)
{
#ifndef NDEBUG
    checkCurrentAtomicStringTable(vm);
#endif
    return *AtomicString::addWithStringTableProvider(*vm, r);
}

} // namespace JSC

#endif // IdentifierInlines_h
