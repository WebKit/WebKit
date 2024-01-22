/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#include "DeprecatedCSSOMValue.h"

#include "DeprecatedCSSOMPrimitiveValue.h"
#include "DeprecatedCSSOMValueList.h"

namespace WebCore {

void DeprecatedCSSOMValue::operator delete(DeprecatedCSSOMValue* value, std::destroying_delete_t)
{
    auto destroyAndFree = [&](auto& value) {
        std::destroy_at(&value);
        std::decay_t<decltype(value)>::freeAfterDestruction(&value);
    };

    switch (value->classType()) {
    case ClassType::Complex:
        destroyAndFree(uncheckedDowncast<DeprecatedCSSOMComplexValue>(*value));
        break;
    case ClassType::Primitive:
        destroyAndFree(uncheckedDowncast<DeprecatedCSSOMPrimitiveValue>(*value));
        break;
    case ClassType::List:
        destroyAndFree(uncheckedDowncast<DeprecatedCSSOMValueList>(*value));
        break;
    }
}

unsigned short DeprecatedCSSOMValue::cssValueType() const
{
    switch (classType()) {
    case ClassType::Complex:
        return uncheckedDowncast<DeprecatedCSSOMComplexValue>(*this).cssValueType();
    case ClassType::Primitive:
        return uncheckedDowncast<DeprecatedCSSOMPrimitiveValue>(*this).cssValueType();
    case ClassType::List:
        return CSS_VALUE_LIST;
    }
    ASSERT_NOT_REACHED();
    return CSS_CUSTOM;
}

String DeprecatedCSSOMValue::cssText() const
{
    switch (classType()) {
    case ClassType::Complex:
        return uncheckedDowncast<DeprecatedCSSOMComplexValue>(*this).cssText();
    case ClassType::Primitive:
        return uncheckedDowncast<DeprecatedCSSOMPrimitiveValue>(*this).cssText();
    case ClassType::List:
        return uncheckedDowncast<DeprecatedCSSOMValueList>(*this).cssText();
    }
    ASSERT_NOT_REACHED();
    return emptyString();
}

unsigned short DeprecatedCSSOMComplexValue::cssValueType() const
{
    // These values are exposed in the DOM, but constants for them are not.
    constexpr unsigned short CSS_INITIAL = 4;
    constexpr unsigned short CSS_UNSET = 5;
    constexpr unsigned short CSS_REVERT = 6;
    switch (valueID(m_value.get())) {
    case CSSValueInherit:
        return CSS_INHERIT;
    case CSSValueInitial:
        return CSS_INITIAL;
    case CSSValueUnset:
        return CSS_UNSET;
    case CSSValueRevert:
        return CSS_REVERT;
    default:
        return CSS_CUSTOM;
    }
}

}
