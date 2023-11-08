// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "CSSVariableReferenceValue.h"

namespace WebCore {

class CSSProperty;

class CSSPendingSubstitutionValue : public CSSValue {
public:
    static Ref<CSSPendingSubstitutionValue> create(CSSPropertyID shorthandPropertyId, Ref<CSSVariableReferenceValue>&& shorthandValue)
    {
        return adoptRef(*new CSSPendingSubstitutionValue(shorthandPropertyId, WTFMove(shorthandValue)));
    }

    CSSVariableReferenceValue& shorthandValue() const { return m_shorthandValue; }
    CSSPropertyID shorthandPropertyId() const { return m_shorthandPropertyId; }

    bool equals(const CSSPendingSubstitutionValue& other) const { return m_shorthandValue.ptr() == other.m_shorthandValue.ptr(); }
    static String customCSSText() { return emptyString(); }

    RefPtr<CSSValue> resolveValue(Style::BuilderState&, CSSPropertyID) const;

private:
    CSSPendingSubstitutionValue(CSSPropertyID shorthandPropertyId, Ref<CSSVariableReferenceValue>&& shorthandValue)
        : CSSValue(PendingSubstitutionValueClass)
        , m_shorthandPropertyId(shorthandPropertyId)
        , m_shorthandValue(WTFMove(shorthandValue))
    {
    }

    const CSSPropertyID m_shorthandPropertyId;
    Ref<CSSVariableReferenceValue> m_shorthandValue;

    mutable Vector<CSSProperty> m_cachedPropertyValues;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPendingSubstitutionValue, isPendingSubstitutionValue())
