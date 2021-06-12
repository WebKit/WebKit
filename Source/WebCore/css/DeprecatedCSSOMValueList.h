/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "CSSValueList.h"
#include "DeprecatedCSSOMValue.h"

namespace WebCore {
    
class DeprecatedCSSOMValueList : public DeprecatedCSSOMValue {
public:
    static Ref<DeprecatedCSSOMValueList> create(const CSSValueList& value, CSSStyleDeclaration& owner)
    {
        return adoptRef(*new DeprecatedCSSOMValueList(value, owner));
    }
    
    String cssText() const;
    
    size_t length() const { return m_values.size(); }
    DeprecatedCSSOMValue* item(size_t index) { return index < m_values.size() ? m_values[index].ptr() : nullptr; }

private:
    DeprecatedCSSOMValueList(const CSSValueList& value, CSSStyleDeclaration& owner)
        : DeprecatedCSSOMValue(DeprecatedValueListClass, owner)
    {
        m_valueSeparator = value.separator();
        m_values.reserveInitialCapacity(value.length());
        for (unsigned i = 0, size = value.length(); i < size; ++i)
            m_values.uncheckedAppend(value.itemWithoutBoundsCheck(i)->createDeprecatedCSSOMWrapper(owner));
    }
    
    Vector<Ref<DeprecatedCSSOMValue>, 4> m_values;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSSOM_VALUE(DeprecatedCSSOMValueList, isValueList())
