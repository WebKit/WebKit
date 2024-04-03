// Copyright 2014 The Chromium Authors. All rights reserved.
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

#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "MediaQuery.h"
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSValue;
class Document;

class SizesAttributeParser {
public:
    SizesAttributeParser(const String&, const Document&);

    float length();

    static float defaultLength(const Document&);
    static float computeLength(double value, CSSUnitType, const Document&);

    auto& dynamicMediaQueryResults() const { return m_dynamicMediaQueryResults; }

private:
    bool parse(CSSParserTokenRange);
    float effectiveSize();
    bool calculateLengthInPixels(CSSParserTokenRange, float& result);
    bool mediaConditionMatches(const MQ::MediaQuery&);
    unsigned effectiveSizeDefaultValue();

    Ref<const Document> protectedDocument() const;

    WeakRef<const Document, WeakPtrImplWithEventTargetData> m_document;
    Vector<MQ::MediaQueryResult> m_dynamicMediaQueryResults;
    float m_length { 0 };
    bool m_lengthWasSet { false };
    bool m_isValid { false };
};

} // namespace WebCore
