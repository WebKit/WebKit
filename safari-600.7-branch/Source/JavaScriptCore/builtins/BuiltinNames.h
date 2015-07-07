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

#ifndef BuiltinNames_h
#define BuiltinNames_h

#include "CommonIdentifiers.h"
#include "JSCBuiltins.h"

namespace JSC {
    
#define INITIALISE_BUILTIN_NAMES(name) , m_##name(vm, #name), m_##name##PrivateName(Identifier::from(PrivateName()))
#define DECLARE_BUILTIN_NAMES(name) const Identifier m_##name; const Identifier m_##name##PrivateName;;
#define DECLARE_BUILTIN_IDENTIFIER_ACCESSOR(name) \
    const Identifier& name##PublicName() const { return m_##name; } \
    const Identifier& name##PrivateName() const { return m_##name##PrivateName; }

#define INITIALISE_PRIVATE_TO_PUBLIC_ENTRY(name) m_privateToPublicMap.add(m_##name##PrivateName.impl(), &m_##name);
#define INITIALISE_PUBLIC_TO_PRIVATE_ENTRY(name) m_publicToPrivateMap.add(m_##name.impl(), &m_##name##PrivateName);
    
class BuiltinNames {
    WTF_MAKE_NONCOPYABLE(BuiltinNames); WTF_MAKE_FAST_ALLOCATED;
    
public:
    BuiltinNames(VM* vm, CommonIdentifiers* commonIdentifiers)
        : m_emptyIdentifier(commonIdentifiers->emptyIdentifier)
        JSC_FOREACH_BUILTIN_FUNCTION_NAME(INITIALISE_BUILTIN_NAMES)
        JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALISE_BUILTIN_NAMES)
    {
        JSC_FOREACH_BUILTIN_FUNCTION_NAME(INITIALISE_PRIVATE_TO_PUBLIC_ENTRY)
        JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALISE_PRIVATE_TO_PUBLIC_ENTRY)
        JSC_FOREACH_BUILTIN_FUNCTION_NAME(INITIALISE_PUBLIC_TO_PRIVATE_ENTRY)
        JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALISE_PUBLIC_TO_PRIVATE_ENTRY)
    }

    const Identifier* getPrivateName(const Identifier&) const;
    const Identifier& getPublicName(const Identifier&) const;
    
    JSC_FOREACH_BUILTIN_FUNCTION_NAME(DECLARE_BUILTIN_IDENTIFIER_ACCESSOR)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(DECLARE_BUILTIN_IDENTIFIER_ACCESSOR)

private:
    Identifier m_emptyIdentifier;
    JSC_FOREACH_BUILTIN_FUNCTION_NAME(DECLARE_BUILTIN_NAMES)
    JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(DECLARE_BUILTIN_NAMES)
    typedef HashMap<RefPtr<StringImpl>, const Identifier*, IdentifierRepHash> BuiltinNamesMap;
    BuiltinNamesMap m_publicToPrivateMap;
    BuiltinNamesMap m_privateToPublicMap;
};

#undef DECLARE_BUILTIN_NAMES
#undef INITIALISE_BUILTIN_NAMES
#undef DECLARE_BUILTIN_IDENTIFIER_ACCESSOR


inline const Identifier* BuiltinNames::getPrivateName(const Identifier& ident) const
{
    auto iter = m_publicToPrivateMap.find(ident.impl());
    if (iter != m_publicToPrivateMap.end())
        return iter->value;
    return 0;
}

inline const Identifier& BuiltinNames::getPublicName(const Identifier& ident) const
{
    auto iter = m_privateToPublicMap.find(ident.impl());
    if (iter != m_privateToPublicMap.end())
        return *iter->value;
    return m_emptyIdentifier;
}

    
}

#endif
