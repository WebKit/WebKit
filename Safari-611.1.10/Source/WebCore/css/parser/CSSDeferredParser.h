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

#include "CSSParserContext.h"
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class CSSParserTokenRange;
class ImmutableStyleProperties;
class StyleRuleKeyframes;
class StyleRuleBase;
class StyleSheetContents;

class CSSDeferredParser : public RefCounted<CSSDeferredParser> {
public:
    static Ref<CSSDeferredParser> create(const CSSParserContext& parserContext, const String& sheetText, StyleSheetContents& styleSheet)
    {
        return adoptRef(*new CSSDeferredParser(parserContext, sheetText, styleSheet));
    }

    CSSParserMode mode() const { return m_context.mode; }

    const CSSParserContext& context() const { return m_context; }
    StyleSheetContents* styleSheet() const;

    Ref<ImmutableStyleProperties> parseDeclaration(const CSSParserTokenRange&);
    void parseRuleList(const CSSParserTokenRange&, Vector<RefPtr<StyleRuleBase>>&);
    void parseKeyframeList(const CSSParserTokenRange&, StyleRuleKeyframes&);

    void adoptTokenizerEscapedStrings(Vector<String>&& strings) { m_escapedStrings = WTFMove(strings); }

private:
    CSSDeferredParser(const CSSParserContext&, const String&, StyleSheetContents&);
    
    Vector<String> m_escapedStrings;
    CSSParserContext m_context;
    
    String m_sheetText;

    WeakPtr<StyleSheetContents> m_styleSheet;
};

} // namespace WebCore
