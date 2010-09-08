/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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
#include "CSSPreloadScanner.h"

#include "CSSHelper.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "HTMLToken.h"

namespace WebCore {

static inline bool isWhitespace(UChar c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

CSSPreloadScanner::CSSPreloadScanner(Document* document)
    : m_document(document)
{
    reset();
}

void CSSPreloadScanner::reset()
{
    m_state = Initial;
    m_rule.clear();
    m_ruleValue.clear();
}

void CSSPreloadScanner::scan(const HTMLToken& token, bool scanningBody)
{
    m_scanningBody = scanningBody;

    const HTMLToken::DataVector& characters = token.characters();
    for (HTMLToken::DataVector::const_iterator iter = characters.begin();
         iter != characters.end(); ++iter) {
        tokenize(*iter);
    }
}

inline void CSSPreloadScanner::tokenize(UChar c)
{
    // We are just interested in @import rules, no need for real tokenization here
    // Searching for other types of resources is probably low payoff.
    switch (m_state) {
    case Initial:
        if (c == '@')
            m_state = RuleStart;
        else if (c == '/')
            m_state = MaybeComment;
        break;
    case MaybeComment:
        if (c == '*')
            m_state = Comment;
        else
            m_state = Initial;
        break;
    case Comment:
        if (c == '*')
            m_state = MaybeCommentEnd;
        break;
    case MaybeCommentEnd:
        if (c == '/')
            m_state = Initial;
        else if (c == '*')
            ;
        else
            m_state = Comment;
        break;
    case RuleStart:
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            m_rule.clear();
            m_ruleValue.clear();
            m_rule.append(c);
            m_state = Rule;
        } else
            m_state = Initial;
        break;
    case Rule:
        if (isWhitespace(c))
            m_state = AfterRule;
        else if (c == ';')
            m_state = Initial;
        else
            m_rule.append(c);
        break;
    case AfterRule:
        if (isWhitespace(c))
            ;
        else if (c == ';')
            m_state = Initial;
        else {
            m_state = RuleValue;
            m_ruleValue.append(c);
        }
        break;
    case RuleValue:
        if (isWhitespace(c))
            m_state = AfterRuleValue;
        else if (c == ';') {
            emitRule();
            m_state = Initial;
        } else 
            m_ruleValue.append(c);
        break;
    case AfterRuleValue:
        if (isWhitespace(c))
            ;
        else if (c == ';') {
            emitRule();
            m_state = Initial;
        } else {
            // FIXME: media rules
            m_state = Initial;
        }
        break;
    }
}

void CSSPreloadScanner::emitRule()
{
    String rule(m_rule.data(), m_rule.size());
    if (equalIgnoringCase(rule, "import") && !m_ruleValue.isEmpty()) {
        String value(m_ruleValue.data(), m_ruleValue.size());
        String url = deprecatedParseURL(value);
        if (!url.isEmpty())
            m_document->cachedResourceLoader()->preload(CachedResource::CSSStyleSheet, url, String(), m_scanningBody);
    }
    m_rule.clear();
    m_ruleValue.clear();
}

}
