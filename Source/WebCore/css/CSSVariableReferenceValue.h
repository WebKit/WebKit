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

#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSValue.h"
#include "CSSVariableData.h"
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSParserTokenRange;

namespace Style {
class BuilderState;
}

class CSSVariableReferenceValue : public CSSValue {
public:
    static Ref<CSSVariableReferenceValue> create(const CSSParserTokenRange& range)
    {
        return adoptRef(*new CSSVariableReferenceValue(CSSVariableData::create(range)));
    }

    bool equals(const CSSVariableReferenceValue& other) const { return m_data.get() == other.m_data.get(); }
    String customCSSText() const;

    RefPtr<CSSVariableData> resolveVariableReferences(Style::BuilderState&) const;

private:
    CSSVariableReferenceValue(Ref<CSSVariableData>&& data)
        : CSSValue(VariableReferenceClass)
        , m_data(WTFMove(data))
    {
    }

    Ref<CSSVariableData> m_data;
    mutable String m_stringValue;
    mutable bool m_serialized { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSVariableReferenceValue, isVariableReferenceValue())
