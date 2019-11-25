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

#include "CSSParserMode.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSValue;
class CSSValuePool;
class StyleSheetContents;
struct CSSParserContext;

class CSSParserFastPaths {
public:
    // Parses simple values like '10px' or 'green', but makes no guarantees
    // about handling any property completely.
    static RefPtr<CSSValue> maybeParseValue(CSSPropertyID, const String&, const CSSParserContext&);

    // Properties handled here shouldn't be explicitly handled in CSSPropertyParser
    static bool isKeywordPropertyID(CSSPropertyID);
    static bool isValidKeywordPropertyAndValue(CSSPropertyID, CSSValueID, const CSSParserContext&);

    static RefPtr<CSSValue> parseColor(const String&, CSSParserMode, CSSValuePool&);
};

} // namespace WebCore
