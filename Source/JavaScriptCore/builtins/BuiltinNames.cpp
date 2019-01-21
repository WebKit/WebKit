/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include "BuiltinNames.h"

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable:4307)
#endif

namespace JSC {
namespace Symbols {

#define INITIALIZE_BUILTIN_STATIC_SYMBOLS(name) SymbolImpl::StaticSymbolImpl name##Symbol { "Symbol." #name };
JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(INITIALIZE_BUILTIN_STATIC_SYMBOLS)
#undef INITIALIZE_BUILTIN_STATIC_SYMBOLS

#define INITIALIZE_BUILTIN_PRIVATE_NAMES(name) SymbolImpl::StaticSymbolImpl name##PrivateName { "PrivateSymbol." #name, SymbolImpl::s_flagIsPrivate };
JSC_FOREACH_BUILTIN_FUNCTION_NAME(INITIALIZE_BUILTIN_PRIVATE_NAMES)
JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALIZE_BUILTIN_PRIVATE_NAMES)
#undef INITIALIZE_BUILTIN_PRIVATE_NAMES

SymbolImpl::StaticSymbolImpl dollarVMPrivateName { "PrivateSymbol.$vm", SymbolImpl::s_flagIsPrivate };
SymbolImpl::StaticSymbolImpl polyProtoPrivateName { "PrivateSymbol.PolyProto", SymbolImpl::s_flagIsPrivate };

} // namespace Symbols

// We treat the dollarVM name as a special case below for $vm (because CommonIdentifiers does not
// yet support the $ character).
BuiltinNames::BuiltinNames(VM* vm, CommonIdentifiers* commonIdentifiers)
    : m_emptyIdentifier(commonIdentifiers->emptyIdentifier)
    JSC_FOREACH_BUILTIN_FUNCTION_NAME(INITIALIZE_BUILTIN_NAMES_IN_JSC)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALIZE_BUILTIN_NAMES_IN_JSC)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(INITIALIZE_BUILTIN_SYMBOLS)
    , m_dollarVMName(Identifier::fromString(vm, "$vm"))
    , m_dollarVMPrivateName(Identifier::fromUid(vm, &static_cast<SymbolImpl&>(Symbols::dollarVMPrivateName)))
    , m_polyProtoPrivateName(Identifier::fromUid(vm, &static_cast<SymbolImpl&>(Symbols::polyProtoPrivateName)))
{
    JSC_FOREACH_BUILTIN_FUNCTION_NAME(INITIALIZE_PRIVATE_TO_PUBLIC_ENTRY)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALIZE_PRIVATE_TO_PUBLIC_ENTRY)
    JSC_FOREACH_BUILTIN_FUNCTION_NAME(INITIALIZE_PUBLIC_TO_PRIVATE_ENTRY)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALIZE_PUBLIC_TO_PRIVATE_ENTRY)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(INITIALIZE_SYMBOL_PUBLIC_TO_PRIVATE_ENTRY)
    m_privateToPublicMap.add(m_dollarVMPrivateName.impl(), &m_dollarVMName);
    m_publicToPrivateMap.add(m_dollarVMName.impl(), &m_dollarVMPrivateName);
}

} // namespace JSC

#if COMPILER(MSVC)
#pragma warning(pop)
#endif
