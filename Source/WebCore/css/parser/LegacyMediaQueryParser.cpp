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

#include "config.h"
#include "LegacyMediaQueryParser.h"

#include "CSSTokenizer.h"
#include "MediaList.h"
#include "MediaQueryParserContext.h"
#include <wtf/Vector.h>

namespace WebCore {

RefPtr<MediaQuerySet> LegacyMediaQueryParser::parseMediaQuerySet(const String& queryString, MediaQueryParserContext context)
{
    auto tokenizer = CSSTokenizer::tryCreate(queryString);
    if (UNLIKELY(!tokenizer))
        return nullptr;
    return parseMediaQuerySet(tokenizer->tokenRange(), context);
}

RefPtr<MediaQuerySet> LegacyMediaQueryParser::parseMediaQuerySet(CSSParserTokenRange range, MediaQueryParserContext context)
{
    return LegacyMediaQueryParser(MediaQuerySetParser, context).parseInternal(range);
}

RefPtr<MediaQuerySet> LegacyMediaQueryParser::parseMediaCondition(CSSParserTokenRange range, MediaQueryParserContext context)
{
    return LegacyMediaQueryParser(MediaConditionParser, context).parseInternal(range);
}

const LegacyMediaQueryParser::State LegacyMediaQueryParser::ReadRestrictor = &LegacyMediaQueryParser::readRestrictor;
const LegacyMediaQueryParser::State LegacyMediaQueryParser::ReadMediaNot = &LegacyMediaQueryParser::readMediaNot;
const LegacyMediaQueryParser::State LegacyMediaQueryParser::ReadMediaType = &LegacyMediaQueryParser::readMediaType;
const LegacyMediaQueryParser::State LegacyMediaQueryParser::ReadAnd = &LegacyMediaQueryParser::readAnd;
const LegacyMediaQueryParser::State LegacyMediaQueryParser::ReadFeatureStart = &LegacyMediaQueryParser::readFeatureStart;
const LegacyMediaQueryParser::State LegacyMediaQueryParser::ReadFeature = &LegacyMediaQueryParser::readFeature;
const LegacyMediaQueryParser::State LegacyMediaQueryParser::ReadFeatureColon = &LegacyMediaQueryParser::readFeatureColon;
const LegacyMediaQueryParser::State LegacyMediaQueryParser::ReadFeatureValue = &LegacyMediaQueryParser::readFeatureValue;
const LegacyMediaQueryParser::State LegacyMediaQueryParser::ReadFeatureEnd = &LegacyMediaQueryParser::readFeatureEnd;
const LegacyMediaQueryParser::State LegacyMediaQueryParser::SkipUntilComma = &LegacyMediaQueryParser::skipUntilComma;
const LegacyMediaQueryParser::State LegacyMediaQueryParser::SkipUntilBlockEnd = &LegacyMediaQueryParser::skipUntilBlockEnd;
const LegacyMediaQueryParser::State LegacyMediaQueryParser::Done = &LegacyMediaQueryParser::done;

LegacyMediaQueryParser::LegacyMediaQueryParser(ParserType parserType, MediaQueryParserContext context)
    : m_parserType(parserType)
    , m_mediaQueryData(context)
    , m_querySet(MediaQuerySet::create())
    
{
    switch (m_parserType) {
    case MediaQuerySetParser:
        m_state = &LegacyMediaQueryParser::readRestrictor;
        break;
    case MediaConditionParser:
        m_state = &LegacyMediaQueryParser::readMediaNot;
        break;
    }
}

LegacyMediaQueryParser::~LegacyMediaQueryParser() = default;

void LegacyMediaQueryParser::setStateAndRestrict(State state, LegacyMediaQuery::Restrictor restrictor)
{
    m_mediaQueryData.setRestrictor(restrictor);
    m_state = state;
}

// State machine member functions start here
void LegacyMediaQueryParser::readRestrictor(CSSParserTokenType type, const CSSParserToken& token, CSSParserTokenRange& range)
{
    readMediaType(type, token, range);
}

void LegacyMediaQueryParser::readMediaNot(CSSParserTokenType type, const CSSParserToken& token, CSSParserTokenRange& range)
{
    if (type == IdentToken && equalLettersIgnoringASCIICase(token.value(), "not"_s))
        setStateAndRestrict(ReadFeatureStart, LegacyMediaQuery::Not);
    else
        readFeatureStart(type, token, range);
}

static bool isRestrictorOrLogicalOperator(const CSSParserToken& token)
{
    // FIXME: it would be more efficient to use lower-case always for tokenValue.
    return equalLettersIgnoringASCIICase(token.value(), "not"_s)
        || equalLettersIgnoringASCIICase(token.value(), "and"_s)
        || equalLettersIgnoringASCIICase(token.value(), "or"_s)
        || equalLettersIgnoringASCIICase(token.value(), "only"_s);
}

void LegacyMediaQueryParser::readMediaType(CSSParserTokenType type, const CSSParserToken& token, CSSParserTokenRange& range)
{
    if (type == LeftParenthesisToken) {
        if (m_mediaQueryData.restrictor() != LegacyMediaQuery::None)
            m_state = SkipUntilComma;
        else
            m_state = ReadFeature;
    } else if (type == IdentToken) {
        if (m_state == ReadRestrictor && equalLettersIgnoringASCIICase(token.value(), "not"_s))
            setStateAndRestrict(ReadMediaType, LegacyMediaQuery::Not);
        else if (m_state == ReadRestrictor && equalLettersIgnoringASCIICase(token.value(), "only"_s))
            setStateAndRestrict(ReadMediaType, LegacyMediaQuery::Only);
        else if (m_mediaQueryData.restrictor() != LegacyMediaQuery::None
            && isRestrictorOrLogicalOperator(token)) {
            m_state = SkipUntilComma;
        } else {
            m_mediaQueryData.setMediaType(token.value().toString());
            m_state = ReadAnd;
        }
    } else if (type == EOFToken && (!m_querySet->queryVector().size() || m_state != ReadRestrictor))
        m_state = Done;
    else {
        m_state = SkipUntilComma;
        if (type == CommaToken)
            skipUntilComma(type, token, range);
    }
}

void LegacyMediaQueryParser::commitMediaQuery()
{
    // FIXME-NEWPARSER: Convoluted and awful, but we can't change the MediaQuerySet yet because of the
    // old parser.
    static const NeverDestroyed<String> defaultMediaType { "all"_s };
    LegacyMediaQuery mediaQuery { m_mediaQueryData.restrictor(), m_mediaQueryData.mediaType().value_or(defaultMediaType), WTFMove(m_mediaQueryData.expressions()) };
    m_mediaQueryData.clear();
    m_querySet->addMediaQuery(WTFMove(mediaQuery));
}

void LegacyMediaQueryParser::readAnd(CSSParserTokenType type, const CSSParserToken& token, CSSParserTokenRange& /*range*/)
{
    if (type == IdentToken && equalLettersIgnoringASCIICase(token.value(), "and"_s)) {
        m_state = ReadFeatureStart;
    } else if (type == CommaToken && m_parserType != MediaConditionParser) {
        commitMediaQuery();
        m_state = ReadRestrictor;
    } else if (type == EOFToken)
        m_state = Done;
    else
        m_state = SkipUntilComma;
}

void LegacyMediaQueryParser::readFeatureStart(CSSParserTokenType type, const CSSParserToken& /*token*/, CSSParserTokenRange& /*range*/)
{
    if (type == LeftParenthesisToken)
        m_state = ReadFeature;
    else
        m_state = SkipUntilComma;
}

void LegacyMediaQueryParser::readFeature(CSSParserTokenType type, const CSSParserToken& token, CSSParserTokenRange& /*range*/)
{
    if (type == IdentToken) {
        m_mediaQueryData.setMediaFeature(token.value().toString());
        m_state = ReadFeatureColon;
    } else
        m_state = SkipUntilComma;
}

void LegacyMediaQueryParser::readFeatureColon(CSSParserTokenType type, const CSSParserToken& token, CSSParserTokenRange& range)
{
    if (type == ColonToken) {
        while (range.peek().type() == WhitespaceToken)
            range.consume();
        if (range.peek().type() == RightParenthesisToken || range.peek().type() == EOFToken)
            m_state = SkipUntilBlockEnd;
        else
            m_state = ReadFeatureValue;
    } else if (type == RightParenthesisToken || type == EOFToken) {
        m_mediaQueryData.addExpression(range);
        readFeatureEnd(type, token, range);
    } else
        m_state = SkipUntilBlockEnd;
}

void LegacyMediaQueryParser::readFeatureValue(CSSParserTokenType type, const CSSParserToken& token, CSSParserTokenRange& range)
{
    if (type == DimensionToken && token.unitType() == CSSUnitType::CSS_UNKNOWN) {
        range.consume();
        m_state = SkipUntilComma;
    } else {
        m_mediaQueryData.addExpression(range);
        m_state = ReadFeatureEnd;
    }
}

void LegacyMediaQueryParser::readFeatureEnd(CSSParserTokenType type, const CSSParserToken& /*token*/, CSSParserTokenRange& /*range*/)
{
    if (type == RightParenthesisToken || type == EOFToken) {
        if (type != EOFToken && m_mediaQueryData.lastExpressionValid())
            m_state = ReadAnd;
        else
            m_state = SkipUntilComma;
    } else {
        m_mediaQueryData.removeLastExpression();
        m_state = SkipUntilBlockEnd;
    }
}

void LegacyMediaQueryParser::skipUntilComma(CSSParserTokenType type, const CSSParserToken& /*token*/, CSSParserTokenRange& /*range*/)
{
    if ((type == CommaToken && !m_blockWatcher.blockLevel()) || type == EOFToken) {
        m_state = ReadRestrictor;
        m_mediaQueryData.clear();
        auto query = LegacyMediaQuery(LegacyMediaQuery::Not, "all"_s, Vector<MediaQueryExpression>());
        m_querySet->addMediaQuery(WTFMove(query));
    }
}

void LegacyMediaQueryParser::skipUntilBlockEnd(CSSParserTokenType /*type */, const CSSParserToken& token, CSSParserTokenRange& /*range*/)
{
    if (token.getBlockType() == CSSParserToken::BlockEnd && !m_blockWatcher.blockLevel())
        m_state = SkipUntilComma;
}

void LegacyMediaQueryParser::done(CSSParserTokenType /*type*/, const CSSParserToken& /*token*/, CSSParserTokenRange& /*range*/) { }

void LegacyMediaQueryParser::handleBlocks(const CSSParserToken& token)
{
    if (token.getBlockType() != CSSParserToken::BlockStart)
        return;
    auto shouldSkipBlock = [&] {
        // FIXME: Nested blocks should be supported.
        if (m_blockWatcher.blockLevel())
            return true;
        if (token.type() == LeftParenthesisToken)
            return false;
        return true;
    }();
    if (shouldSkipBlock)
        m_state = SkipUntilBlockEnd;
}

void LegacyMediaQueryParser::processToken(const CSSParserToken& token, CSSParserTokenRange& range)
{
    CSSParserTokenType type = token.type();

    if (type == WhitespaceToken) {
        range.consume();
        return;
    }

    if (m_state != ReadFeatureValue) {
        handleBlocks(token);
        m_blockWatcher.handleToken(token);
        range.consume();
    }

    // Call the function that handles current state
    ((this)->*(m_state))(type, token, range);
}

// The state machine loop
RefPtr<MediaQuerySet> LegacyMediaQueryParser::parseInternal(CSSParserTokenRange& range)
{
    
    while (!range.atEnd())
        processToken(range.peek(), range);

    // FIXME: Can we get rid of this special case?
    if (m_parserType == MediaQuerySetParser)
        processToken(CSSParserToken(EOFToken), range);

    if (m_state != ReadAnd && m_state != ReadRestrictor && m_state != Done && m_state != ReadMediaNot) {
        auto query = LegacyMediaQuery(LegacyMediaQuery::Not, "all"_s, Vector<MediaQueryExpression>());
        m_querySet->addMediaQuery(WTFMove(query));
    } else if (m_mediaQueryData.currentMediaQueryChanged())
        commitMediaQuery();

    m_querySet->shrinkToFit();

    return m_querySet;
}

LegacyMediaQueryParser::MediaQueryData::MediaQueryData(MediaQueryParserContext context)
    : m_context(context)
{
}

void LegacyMediaQueryParser::MediaQueryData::clear()
{
    m_restrictor = LegacyMediaQuery::None;
    m_mediaType = std::nullopt;
    m_mediaFeature = String();
    m_expressions.clear();
}

void LegacyMediaQueryParser::MediaQueryData::addExpression(CSSParserTokenRange& range)
{
    m_expressions.append(MediaQueryExpression { m_mediaFeature, range, m_context });
}

bool LegacyMediaQueryParser::MediaQueryData::lastExpressionValid()
{
    return m_expressions.last().isValid();
}

void LegacyMediaQueryParser::MediaQueryData::removeLastExpression()
{
    m_expressions.removeLast();
}

} // namespace WebCore
