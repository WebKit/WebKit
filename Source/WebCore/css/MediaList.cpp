/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2010, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "MediaList.h"

#include "CSSImportRule.h"
#include "CSSParser.h"
#include "CSSStyleSheet.h"
#include "DOMWindow.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "HTMLParserIdioms.h"
#include "MediaFeatureNames.h"
#include "MediaQuery.h"
#include "ScriptableDocumentParser.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

/* MediaList is used to store 3 types of media related entities which mean the same:
 * Media Queries, Media Types and Media Descriptors.
 * Currently MediaList always tries to parse media queries and if parsing fails,
 * tries to fallback to Media Descriptors if m_fallbackToDescriptor flag is set.
 * Slight problem with syntax error handling:
 * CSS 2.1 Spec (http://www.w3.org/TR/CSS21/media.html)
 * specifies that failing media type parsing is a syntax error
 * CSS 3 Media Queries Spec (http://www.w3.org/TR/css3-mediaqueries/)
 * specifies that failing media query is a syntax error
 * HTML 4.01 spec (http://www.w3.org/TR/REC-html40/present/styles.html#adef-media)
 * specifies that Media Descriptors should be parsed with forward-compatible syntax
 * DOM Level 2 Style Sheet spec (http://www.w3.org/TR/DOM-Level-2-Style/)
 * talks about MediaList.mediaText and refers
 *   -  to Media Descriptors of HTML 4.0 in context of StyleSheet
 *   -  to Media Types of CSS 2.0 in context of CSSMediaRule and CSSImportRule
 *
 * These facts create situation where same (illegal) media specification may result in
 * different parses depending on whether it is media attr of style element or part of
 * css @media rule.
 * <style media="screen and resolution > 40dpi"> ..</style> will be enabled on screen devices where as
 * @media screen and resolution > 40dpi {..} will not.
 * This gets more counter-intuitive in JavaScript:
 * document.styleSheets[0].media.mediaText = "screen and resolution > 40dpi" will be ok and
 * enabled, while
 * document.styleSheets[0].cssRules[0].media.mediaText = "screen and resolution > 40dpi" will
 * throw SYNTAX_ERR exception.
 */
    
MediaQuerySet::MediaQuerySet()
    : m_fallbackToDescriptor(false)
    , m_lastLine(0)
{
}

MediaQuerySet::MediaQuerySet(const String& mediaString, bool fallbackToDescriptor)
    : m_fallbackToDescriptor(fallbackToDescriptor)
    , m_lastLine(0)
{
    bool success = parse(mediaString);

    // FIXME: parsing can fail. The problem with failing constructor is that
    // we would need additional flag saying MediaList is not valid
    // Parse can fail only when fallbackToDescriptor == false, i.e when HTML4 media descriptor
    // forward-compatible syntax is not in use.
    // DOMImplementationCSS seems to mandate that media descriptors are used
    // for both HTML and SVG, even though svg:style doesn't use media descriptors
    // Currently the only places where parsing can fail are
    // creating <svg:style>, creating css media / import rules from js

    // FIXME: This doesn't make much sense.
    if (!success)
        parse("invalid");
}

MediaQuerySet::MediaQuerySet(const MediaQuerySet& o)
    : RefCounted()
    , m_fallbackToDescriptor(o.m_fallbackToDescriptor)
    , m_lastLine(o.m_lastLine)
    , m_queries(o.m_queries)
{
}

MediaQuerySet::~MediaQuerySet()
{
}

static String parseMediaDescriptor(const String& string)
{
    // http://www.w3.org/TR/REC-html40/types.html#type-media-descriptors
    // "Each entry is truncated just before the first character that isn't a
    // US ASCII letter [a-zA-Z] (ISO 10646 hex 41-5a, 61-7a), digit [0-9] (hex 30-39),
    // or hyphen (hex 2d)."
    unsigned length = string.length();
    unsigned i;
    for (i = 0; i < length; ++i) {
        auto character = string[i];
        if (!(isASCIIAlphanumeric(character) || character == '-'))
            break;
    }
    return string.left(i);
}

Optional<MediaQuery> MediaQuerySet::internalParse(CSSParser& parser, const String& queryString)
{
    if (auto query = parser.parseMediaQuery(queryString))
        return WTFMove(*query);
    if (!m_fallbackToDescriptor)
        return Nullopt;
    return MediaQuery { MediaQuery::None, parseMediaDescriptor(queryString), Vector<MediaQueryExpression> { } };
}

Optional<MediaQuery> MediaQuerySet::internalParse(const String& queryString)
{
    CSSParser parser(CSSStrictMode);
    return internalParse(parser, queryString);
}

bool MediaQuerySet::parse(const String& mediaString)
{
    CSSParser parser(CSSStrictMode);
    
    Vector<MediaQuery> result;
    Vector<String> list;
    mediaString.split(',', list);
    for (auto& listString : list) {
        String medium = stripLeadingAndTrailingHTMLSpaces(listString);
        if (medium.isEmpty()) {
            if (m_fallbackToDescriptor)
                continue;
        } else if (auto query = internalParse(parser, medium)) {
            result.append(WTFMove(query.value()));
            continue;
        }
        return false;
    }
    // ",,,," falls straight through, but is not valid unless fallback
    if (!m_fallbackToDescriptor && list.isEmpty()) {
        String strippedMediaString = stripLeadingAndTrailingHTMLSpaces(mediaString);
        if (!strippedMediaString.isEmpty())
            return false;
    }
    m_queries = WTFMove(result);
    shrinkToFit();
    return true;
}

bool MediaQuerySet::add(const String& queryString)
{
    auto parsedQuery = internalParse(queryString);
    if (!parsedQuery)
        return false;
    m_queries.append(WTFMove(parsedQuery.value()));
    return true;
}

bool MediaQuerySet::remove(const String& queryString)
{
    auto parsedQuery = internalParse(queryString);
    if (!parsedQuery)
        return false;
    return m_queries.removeFirstMatching([&parsedQuery](auto& query) {
        return query == parsedQuery.value();
    });
}

void MediaQuerySet::addMediaQuery(MediaQuery&& mediaQuery)
{
    m_queries.append(WTFMove(mediaQuery));
}

String MediaQuerySet::mediaText() const
{
    StringBuilder text;
    bool needComma = false;
    for (auto& query : m_queries) {
        if (needComma)
            text.appendLiteral(", ");
        text.append(query.cssText());
        needComma = true;
    }
    return text.toString();
}

void MediaQuerySet::shrinkToFit()
{
    m_queries.shrinkToFit();
    for (auto& query : m_queries)
        query.shrinkToFit();
}

MediaList::MediaList(MediaQuerySet* mediaQueries, CSSStyleSheet* parentSheet)
    : m_mediaQueries(mediaQueries)
    , m_parentStyleSheet(parentSheet)
{
}

MediaList::MediaList(MediaQuerySet* mediaQueries, CSSRule* parentRule)
    : m_mediaQueries(mediaQueries)
    , m_parentRule(parentRule)
{
}

MediaList::~MediaList()
{
}

void MediaList::setMediaText(const String& value, ExceptionCode& ec)
{
    CSSStyleSheet::RuleMutationScope mutationScope(m_parentRule);
    if (!m_mediaQueries->parse(value)) {
        ec = SYNTAX_ERR;
        return;
    }
    if (m_parentStyleSheet)
        m_parentStyleSheet->didMutate();
}

String MediaList::item(unsigned index) const
{
    auto& queries = m_mediaQueries->queryVector();
    if (index < queries.size())
        return queries[index].cssText();
    return String();
}

void MediaList::deleteMedium(const String& medium, ExceptionCode& ec)
{
    CSSStyleSheet::RuleMutationScope mutationScope(m_parentRule);

    bool success = m_mediaQueries->remove(medium);
    if (!success) {
        ec = NOT_FOUND_ERR;
        return;
    }
    if (m_parentStyleSheet)
        m_parentStyleSheet->didMutate();
}

void MediaList::appendMedium(const String& medium, ExceptionCode& ec)
{
    CSSStyleSheet::RuleMutationScope mutationScope(m_parentRule);

    bool success = m_mediaQueries->add(medium);
    if (!success) {
        // FIXME: Should this really be INVALID_CHARACTER_ERR?
        ec = INVALID_CHARACTER_ERR;
        return;
    }
    if (m_parentStyleSheet)
        m_parentStyleSheet->didMutate();
}

void MediaList::reattach(MediaQuerySet* mediaQueries)
{
    ASSERT(mediaQueries);
    m_mediaQueries = mediaQueries;
}

#if ENABLE(RESOLUTION_MEDIA_QUERY)

static void addResolutionWarningMessageToConsole(Document& document, const String& serializedExpression, const CSSPrimitiveValue& value)
{
    static NeverDestroyed<String> mediaQueryMessage(ASCIILiteral("Consider using 'dppx' units instead of '%replacementUnits%', as in CSS '%replacementUnits%' means dots-per-CSS-%lengthUnit%, not dots-per-physical-%lengthUnit%, so does not correspond to the actual '%replacementUnits%' of a screen. In media query expression: "));
    static NeverDestroyed<String> mediaValueDPI(ASCIILiteral("dpi"));
    static NeverDestroyed<String> mediaValueDPCM(ASCIILiteral("dpcm"));
    static NeverDestroyed<String> lengthUnitInch(ASCIILiteral("inch"));
    static NeverDestroyed<String> lengthUnitCentimeter(ASCIILiteral("centimeter"));

    String message;
    if (value.isDotsPerInch())
        message = mediaQueryMessage.get().replace("%replacementUnits%", mediaValueDPI).replace("%lengthUnit%", lengthUnitInch);
    else if (value.isDotsPerCentimeter())
        message = mediaQueryMessage.get().replace("%replacementUnits%", mediaValueDPCM).replace("%lengthUnit%", lengthUnitCentimeter);
    else
        ASSERT_NOT_REACHED();

    message.append(serializedExpression);

    document.addConsoleMessage(MessageSource::CSS, MessageLevel::Debug, message);
}

void reportMediaQueryWarningIfNeeded(Document* document, const MediaQuerySet* mediaQuerySet)
{
    if (!mediaQuerySet || !document)
        return;

    for (auto& query : mediaQuerySet->queryVector()) {
        if (!query.ignored() && !equalLettersIgnoringASCIICase(query.mediaType(), "print")) {
            auto& expressions = query.expressions();
            for (auto& expression : expressions) {
                if (expression.mediaFeature() == MediaFeatureNames::resolution || expression.mediaFeature() == MediaFeatureNames::maxResolution || expression.mediaFeature() == MediaFeatureNames::minResolution) {
                    auto* value = expression.value();
                    if (is<CSSPrimitiveValue>(value)) {
                        auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
                        if (primitiveValue.isDotsPerInch() || primitiveValue.isDotsPerCentimeter())
                            addResolutionWarningMessageToConsole(*document, mediaQuerySet->mediaText(), primitiveValue);
                    }
                }
            }
        }
    }
}

#endif

}
