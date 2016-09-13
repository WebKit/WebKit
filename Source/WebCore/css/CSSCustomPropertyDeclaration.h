// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "CSSValue.h"
#include "CSSVariableData.h"
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class CSSCustomPropertyDeclaration : public CSSValue {
public:
    static Ref<CSSCustomPropertyDeclaration> create(const AtomicString& name, Ref<CSSVariableData>&& value)
    {
        return adoptRef(*new CSSCustomPropertyDeclaration(name, WTFMove(value)));
    }

    static Ref<CSSCustomPropertyDeclaration> create(const AtomicString& name, CSSValueID id)
    {
        return adoptRef(*new CSSCustomPropertyDeclaration(name, id));
    }

    const AtomicString& name() const { return m_name; }
    CSSVariableData* value() const { return m_value.get(); }
    CSSValueID id() const { return m_valueId; }
    String customCSSText() const;

    bool equals(const CSSCustomPropertyDeclaration& other) const { return this == &other; }

private:
    CSSCustomPropertyDeclaration(const AtomicString& name, CSSValueID id)
        : CSSValue(CustomPropertyDeclarationClass)
        , m_name(name)
        , m_value(nullptr)
        , m_valueId(id)
    {
        ASSERT(id == CSSValueInherit || id == CSSValueInitial || id == CSSValueUnset || id == CSSValueRevert);
    }

    CSSCustomPropertyDeclaration(const AtomicString& name, Ref<CSSVariableData>&& value)
        : CSSValue(CustomPropertyDeclarationClass)
        , m_name(name)
        , m_value(WTFMove(value))
        , m_valueId(CSSValueInternalVariableValue)
    {
    }

    const AtomicString m_name;
    RefPtr<CSSVariableData> m_value;
    CSSValueID m_valueId;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCustomPropertyDeclaration, isCustomPropertyDeclaration())
