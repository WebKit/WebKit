/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#include "IdentifierInlines.h"
#include <wtf/TZoneMallocInlines.h>

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable:4307)
#endif

namespace JSC {
namespace Symbols {

#define INITIALIZE_BUILTIN_STATIC_SYMBOLS(name) SymbolImpl::StaticSymbolImpl name##Symbol { "Symbol." #name };
JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(INITIALIZE_BUILTIN_STATIC_SYMBOLS)
#undef INITIALIZE_BUILTIN_STATIC_SYMBOLS

SymbolImpl::StaticSymbolImpl intlLegacyConstructedSymbol { "IntlLegacyConstructedSymbol" };

#define INITIALIZE_BUILTIN_PRIVATE_NAMES(name) SymbolImpl::StaticSymbolImpl name##PrivateName { #name, SymbolImpl::s_flagIsPrivate };
JSC_FOREACH_BUILTIN_FUNCTION_NAME(INITIALIZE_BUILTIN_PRIVATE_NAMES)
JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALIZE_BUILTIN_PRIVATE_NAMES)
#undef INITIALIZE_BUILTIN_PRIVATE_NAMES

SymbolImpl::StaticSymbolImpl dollarVMPrivateName { "$vm", SymbolImpl::s_flagIsPrivate };
SymbolImpl::StaticSymbolImpl polyProtoPrivateName { "PolyProto", SymbolImpl::s_flagIsPrivate };

} // namespace Symbols

#define INITIALIZE_BUILTIN_NAMES_IN_JSC(name) , m_##name(JSC::Identifier::fromString(vm, #name ""_s))
#define INITIALIZE_BUILTIN_SYMBOLS_IN_JSC(name) \
    , m_##name##Symbol(JSC::Identifier::fromUid(vm, &static_cast<SymbolImpl&>(JSC::Symbols::name##Symbol))) \
    , m_##name##SymbolPrivateIdentifier(JSC::Identifier::fromString(vm, #name ""_s))

#define INITIALIZE_PUBLIC_TO_PRIVATE_ENTRY(name) \
    do { \
        SymbolImpl* symbol = &static_cast<SymbolImpl&>(JSC::Symbols::name##PrivateName); \
        checkPublicToPrivateMapConsistency(symbol); \
        m_privateNameSet.add(symbol); \
    } while (0);

#define INITIALIZE_WELL_KNOWN_SYMBOL_PUBLIC_TO_PRIVATE_ENTRY(name) \
    do { \
        SymbolImpl* symbol = static_cast<SymbolImpl*>(m_##name##Symbol.impl()); \
        m_wellKnownSymbolsMap.add(m_##name##SymbolPrivateIdentifier.impl(), symbol); \
    } while (0);

WTF_MAKE_TZONE_ALLOCATED_IMPL(BuiltinNames);

// We treat the dollarVM name as a special case below for $vm (because CommonIdentifiers does not
// yet support the $ character).
BuiltinNames::BuiltinNames(VM& vm, CommonIdentifiers* commonIdentifiers)
    : m_emptyIdentifier(commonIdentifiers->emptyIdentifier)
    JSC_FOREACH_BUILTIN_FUNCTION_NAME(INITIALIZE_BUILTIN_NAMES_IN_JSC)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALIZE_BUILTIN_NAMES_IN_JSC)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(INITIALIZE_BUILTIN_SYMBOLS_IN_JSC)
    , m_intlLegacyConstructedSymbol(JSC::Identifier::fromUid(vm, &static_cast<SymbolImpl&>(Symbols::intlLegacyConstructedSymbol)))
    , m_dollarVMName(Identifier::fromString(vm, "$vm"_s))
    , m_dollarVMPrivateName(Identifier::fromUid(vm, &static_cast<SymbolImpl&>(Symbols::dollarVMPrivateName)))
    , m_polyProtoPrivateName(Identifier::fromUid(vm, &static_cast<SymbolImpl&>(Symbols::polyProtoPrivateName)))
{
    JSC_FOREACH_BUILTIN_FUNCTION_NAME(INITIALIZE_PUBLIC_TO_PRIVATE_ENTRY)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALIZE_PUBLIC_TO_PRIVATE_ENTRY)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(INITIALIZE_WELL_KNOWN_SYMBOL_PUBLIC_TO_PRIVATE_ENTRY)
    m_privateNameSet.add(static_cast<SymbolImpl*>(m_dollarVMPrivateName.impl()));
}

#undef INITIALIZE_BUILTIN_NAMES_IN_JSC
#undef INITIALIZE_BUILTIN_SYMBOLS_IN_JSC
#undef INITIALIZE_PUBLIC_TO_PRIVATE_ENTRY
#undef INITIALIZE_WELL_KNOWN_SYMBOL_PUBLIC_TO_PRIVATE_ENTRY


using LCharBuffer = WTF::HashTranslatorCharBuffer<LChar>;
using UCharBuffer = WTF::HashTranslatorCharBuffer<UChar>;

template<typename CharacterType>
struct CharBufferSeacher {
    using Buffer = WTF::HashTranslatorCharBuffer<CharacterType>;
    static unsigned hash(const Buffer& buf)
    {
        return buf.hash;
    }

    static bool equal(const String& str, const Buffer& buf)
    {
        return WTF::equal(str.impl(), buf.characters, buf.length);
    }
};

template<typename CharacterType>
static PrivateSymbolImpl* lookUpPrivateNameImpl(const BuiltinNames::PrivateNameSet& set, const WTF::HashTranslatorCharBuffer<CharacterType>& buffer)
{
    auto iterator = set.find<CharBufferSeacher<CharacterType>>(buffer);
    if (iterator == set.end())
        return nullptr;
    StringImpl* impl = iterator->impl();
    ASSERT(impl->isSymbol());
    SymbolImpl* symbol = static_cast<SymbolImpl*>(impl);
    ASSERT(symbol->isPrivate());
    return static_cast<PrivateSymbolImpl*>(symbol);
}

template<typename CharacterType>
static SymbolImpl* lookUpWellKnownSymbolImpl(const BuiltinNames::WellKnownSymbolMap& map, const WTF::HashTranslatorCharBuffer<CharacterType>& buffer)
{
    auto iterator = map.find<CharBufferSeacher<CharacterType>>(buffer);
    if (iterator == map.end())
        return nullptr;
    return iterator->value;
}

PrivateSymbolImpl* BuiltinNames::lookUpPrivateName(const LChar* characters, unsigned length) const
{
    LCharBuffer buffer { characters, length };
    return lookUpPrivateNameImpl(m_privateNameSet, buffer);
}

PrivateSymbolImpl* BuiltinNames::lookUpPrivateName(const UChar* characters, unsigned length) const
{
    UCharBuffer buffer { characters, length };
    return lookUpPrivateNameImpl(m_privateNameSet, buffer);
}

PrivateSymbolImpl* BuiltinNames::lookUpPrivateName(const String& string) const
{
    if (string.is8Bit()) {
        LCharBuffer buffer { string.characters8(), string.length(), string.hash() };
        return lookUpPrivateNameImpl(m_privateNameSet, buffer);
    }
    UCharBuffer buffer { string.characters16(), string.length(), string.hash() };
    return lookUpPrivateNameImpl(m_privateNameSet, buffer);
}

SymbolImpl* BuiltinNames::lookUpWellKnownSymbol(const LChar* characters, unsigned length) const
{
    LCharBuffer buffer { characters, length };
    return lookUpWellKnownSymbolImpl(m_wellKnownSymbolsMap, buffer);
}

SymbolImpl* BuiltinNames::lookUpWellKnownSymbol(const UChar* characters, unsigned length) const
{
    UCharBuffer buffer { characters, length };
    return lookUpWellKnownSymbolImpl(m_wellKnownSymbolsMap, buffer);
}

SymbolImpl* BuiltinNames::lookUpWellKnownSymbol(const String& string) const
{
    if (string.is8Bit()) {
        LCharBuffer buffer { string.characters8(), string.length(), string.hash() };
        return lookUpWellKnownSymbolImpl(m_wellKnownSymbolsMap, buffer);
    }
    UCharBuffer buffer { string.characters16(), string.length(), string.hash() };
    return lookUpWellKnownSymbolImpl(m_wellKnownSymbolsMap, buffer);
}

} // namespace JSC

#if COMPILER(MSVC)
#pragma warning(pop)
#endif
