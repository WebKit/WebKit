/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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
#include "SmallStrings.h"

#include "JSCJSValueInlines.h"
#include <wtf/text/StringImpl.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

SmallStrings::SmallStrings()
{
    static_assert(singleCharacterStringCount == sizeof(m_singleCharacterStrings) / sizeof(m_singleCharacterStrings[0]), "characters count is in sync with class usage");

    for (unsigned i = 0; i < singleCharacterStringCount; ++i)
        m_singleCharacterStrings[i] = nullptr;
}

void SmallStrings::initializeCommonStrings(VM& vm)
{
    ASSERT(!m_emptyString);
    m_emptyString = JSString::createEmptyString(vm);
    ASSERT(m_needsToBeVisited);

    for (unsigned i = 0; i < singleCharacterStringCount; ++i) {
        ASSERT(!m_singleCharacterStrings[i]);
        std::array<const Latin1Character, 1> string = { static_cast<Latin1Character>(i) };
        m_singleCharacterStrings[i] = JSString::createHasOtherOwner(vm, AtomStringImpl::add(string).releaseNonNull());
        ASSERT(m_needsToBeVisited);
    }

#define JSC_COMMON_STRINGS_ATTRIBUTE_INITIALIZE(name) initialize(&vm, m_##name, #name ## _s);
    JSC_COMMON_STRINGS_EACH_NAME(JSC_COMMON_STRINGS_ATTRIBUTE_INITIALIZE)
#undef JSC_COMMON_STRINGS_ATTRIBUTE_INITIALIZE
    initialize(&vm, m_objectStringStart, "[object "_s);
    initialize(&vm, m_objectNullString, "[object Null]"_s);
    initialize(&vm, m_objectUndefinedString, "[object Undefined]"_s);
    initialize(&vm, m_objectObjectString, "[object Object]"_s);
    initialize(&vm, m_objectArrayString, "[object Array]"_s);
    initialize(&vm, m_objectFunctionString, "[object Function]"_s);
    initialize(&vm, m_objectArgumentsString, "[object Arguments]"_s);
    initialize(&vm, m_objectDateString, "[object Date]"_s);
    initialize(&vm, m_objectRegExpString, "[object RegExp]"_s);
    initialize(&vm, m_objectErrorString, "[object Error]"_s);
    initialize(&vm, m_objectBooleanString, "[object Boolean]"_s);
    initialize(&vm, m_objectNumberString, "[object Number]"_s);
    initialize(&vm, m_objectStringString, "[object String]"_s);
    initialize(&vm, m_boundPrefixString, "bound "_s);
    initialize(&vm, m_notEqualString, "not-equal"_s);
    initialize(&vm, m_timedOutString, "timed-out"_s);
    initialize(&vm, m_okString, "ok"_s);
    initialize(&vm, m_sentinelString, "$"_s);
    initialize(&vm, m_fulfilledString, "fulfilled"_s);
    initialize(&vm, m_rejectedString, "rejected"_s);

    setIsInitialized(true);
}

template<typename Visitor>
void SmallStrings::visitStrongReferences(Visitor& visitor)
{
    m_needsToBeVisited = false;
    visitor.appendUnbarriered(m_emptyString);
    for (unsigned i = 0; i <= maxSingleCharacterString; ++i)
        visitor.appendUnbarriered(m_singleCharacterStrings[i]);
#define JSC_COMMON_STRINGS_ATTRIBUTE_VISIT(name) visitor.appendUnbarriered(m_##name);
    JSC_COMMON_STRINGS_EACH_NAME(JSC_COMMON_STRINGS_ATTRIBUTE_VISIT)
#undef JSC_COMMON_STRINGS_ATTRIBUTE_VISIT
    visitor.appendUnbarriered(m_objectStringStart);
    visitor.appendUnbarriered(m_objectNullString);
    visitor.appendUnbarriered(m_objectUndefinedString);
    visitor.appendUnbarriered(m_objectObjectString);
    visitor.appendUnbarriered(m_objectArrayString);
    visitor.appendUnbarriered(m_objectFunctionString);
    visitor.appendUnbarriered(m_objectArgumentsString);
    visitor.appendUnbarriered(m_objectDateString);
    visitor.appendUnbarriered(m_objectRegExpString);
    visitor.appendUnbarriered(m_objectErrorString);
    visitor.appendUnbarriered(m_objectBooleanString);
    visitor.appendUnbarriered(m_objectNumberString);
    visitor.appendUnbarriered(m_objectStringString);
    visitor.appendUnbarriered(m_boundPrefixString);
    visitor.appendUnbarriered(m_notEqualString);
    visitor.appendUnbarriered(m_timedOutString);
    visitor.appendUnbarriered(m_okString);
    visitor.appendUnbarriered(m_sentinelString);
    visitor.appendUnbarriered(m_fulfilledString);
    visitor.appendUnbarriered(m_rejectedString);
}

template void SmallStrings::visitStrongReferences(AbstractSlotVisitor&);
template void SmallStrings::visitStrongReferences(SlotVisitor&);

SmallStrings::~SmallStrings() = default;

Ref<AtomStringImpl> SmallStrings::singleCharacterStringRep(unsigned char character)
{
    if (m_isInitialized) [[likely]]
        return *static_cast<AtomStringImpl*>(const_cast<StringImpl*>(m_singleCharacterStrings[character]->tryGetValueImpl()));
    std::array<const Latin1Character, 1> string = { static_cast<Latin1Character>(character) };
    return AtomStringImpl::add(string).releaseNonNull();
}

AtomStringImpl* SmallStrings::existingSingleCharacterStringRep(unsigned char character)
{
    if (!m_isInitialized) [[unlikely]]
        return nullptr;
    return static_cast<AtomStringImpl*>(const_cast<StringImpl*>(m_singleCharacterStrings[character]->tryGetValueImpl()));
}

void SmallStrings::initialize(VM* vm, JSString*& string, ASCIILiteral value)
{
    string = JSString::create(*vm, AtomStringImpl::add(value));
    ASSERT(m_needsToBeVisited);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
