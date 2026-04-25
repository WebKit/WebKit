// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#include "CSSProperty.h"
#include "CSSSubstitutionValue.h"
#include "CSSValue.h"

namespace WebCore {

// Longhand placeholder for a shorthand value containing substitution functions.
// Each longhand of the shorthand gets one of these; they share the CSSSubstitutionValue.
// During style resolution, the shared value is resolved and parsed as the shorthand
// to extract individual longhand values.
class CSSShorthandSubstitutionValue final : public CSSValue {
public:
    static Ref<CSSShorthandSubstitutionValue> create(CSSPropertyID shorthandPropertyId, Ref<CSSSubstitutionValue>&&);

    CSSSubstitutionValue& shorthandValue() const { return m_shorthandValue; }
    CSSPropertyID shorthandPropertyId() const { return m_shorthandPropertyId; }

    bool equals(const CSSShorthandSubstitutionValue& other) const { return m_shorthandValue.ptr() == other.m_shorthandValue.ptr(); }
    static String customCSSText(const CSS::SerializationContext&) { return emptyString(); }

    IterationStatus customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (func(m_shorthandValue.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        return IterationStatus::Continue;
    }

private:
    friend class Style::SubstitutionResolver;

    CSSShorthandSubstitutionValue(CSSPropertyID shorthandPropertyId, Ref<CSSSubstitutionValue>&&);

    const CSSPropertyID m_shorthandPropertyId;
    const Ref<CSSSubstitutionValue> m_shorthandValue;

    mutable Vector<CSSProperty> m_cachedPropertyValues;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSShorthandSubstitutionValue, isShorthandSubstitutionValue())
