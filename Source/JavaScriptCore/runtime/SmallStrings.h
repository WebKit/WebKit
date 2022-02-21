/*
 * Copyright (C) 2008-2021 Apple Inc. All Rights Reserved.
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

#include "CollectionScope.h"
#include "TypeofType.h"
#include <wtf/Noncopyable.h>

#define JSC_COMMON_STRINGS_EACH_NAME(macro) \
    macro(default) \
    macro(boolean) \
    macro(false) \
    macro(function) \
    macro(number) \
    macro(null) \
    macro(object) \
    macro(undefined) \
    macro(string) \
    macro(symbol) \
    macro(bigint) \
    macro(true)

namespace WTF {
class StringImpl;
}

namespace JSC {

class VM;
class JSString;

static constexpr unsigned maxSingleCharacterString = 0xFF;

class SmallStrings {
    WTF_MAKE_NONCOPYABLE(SmallStrings);
public:
    SmallStrings();
    ~SmallStrings();

    JSString* emptyString()
    {
        return m_emptyString;
    }

    JSString* singleCharacterString(unsigned char character)
    {
        return m_singleCharacterStrings[character];
    }

    JS_EXPORT_PRIVATE Ref<AtomStringImpl> singleCharacterStringRep(unsigned char character);

    void setIsInitialized(bool isInitialized) { m_isInitialized = isInitialized; }

    JSString** singleCharacterStrings() { return &m_singleCharacterStrings[0]; }

    void initializeCommonStrings(VM&);
    template<typename Visitor> void visitStrongReferences(Visitor&);

#define JSC_COMMON_STRINGS_ACCESSOR_DEFINITION(name) \
    JSString* name##String() const                   \
    {                                                \
        return m_##name;                             \
    }
    JSC_COMMON_STRINGS_EACH_NAME(JSC_COMMON_STRINGS_ACCESSOR_DEFINITION)
#undef JSC_COMMON_STRINGS_ACCESSOR_DEFINITION
    
    JSString* typeString(TypeofType type) const
    {
        switch (type) {
        case TypeofType::Undefined:
            return undefinedString();
        case TypeofType::Boolean:
            return booleanString();
        case TypeofType::Number:
            return numberString();
        case TypeofType::String:
            return stringString();
        case TypeofType::Symbol:
            return symbolString();
        case TypeofType::Object:
            return objectString();
        case TypeofType::Function:
            return functionString();
        case TypeofType::BigInt:
            return bigintString();
        }
        
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }

    JSString* objectStringStart() const { return m_objectStringStart; }
    JSString* nullObjectString() const { return m_nullObjectString; }
    JSString* undefinedObjectString() const { return m_undefinedObjectString; }
    JSString* boundPrefixString() const { return m_boundPrefixString; }
    JSString* notEqualString() const { return m_notEqualString; }
    JSString* timedOutString() const { return m_timedOutString; }
    JSString* okString() const { return m_okString; }
    JSString* sentinelString() const { return m_sentinelString; }

    bool needsToBeVisited(CollectionScope scope) const
    {
        if (scope == CollectionScope::Full)
            return true;
        return m_needsToBeVisited;
    }

private:
    static constexpr unsigned singleCharacterStringCount = maxSingleCharacterString + 1;

    void initialize(VM*, JSString*&, const char* value);

    JSString* m_emptyString { nullptr };
#define JSC_COMMON_STRINGS_ATTRIBUTE_DECLARATION(name) JSString* m_##name { nullptr };
    JSC_COMMON_STRINGS_EACH_NAME(JSC_COMMON_STRINGS_ATTRIBUTE_DECLARATION)
#undef JSC_COMMON_STRINGS_ATTRIBUTE_DECLARATION
    JSString* m_objectStringStart { nullptr };
    JSString* m_nullObjectString { nullptr };
    JSString* m_undefinedObjectString { nullptr };
    JSString* m_boundPrefixString { nullptr };
    JSString* m_notEqualString { nullptr };
    JSString* m_timedOutString { nullptr };
    JSString* m_okString { nullptr };
    JSString* m_sentinelString { nullptr };
    JSString* m_singleCharacterStrings[singleCharacterStringCount] { nullptr };
    bool m_needsToBeVisited { true };
    bool m_isInitialized { false };
};

} // namespace JSC
