/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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
#include "CSSStyleSheet.h"
#include "DOMWindow.h"
#include "Document.h"
#include "MediaFeatureNames.h"
#include "MediaQuery.h"
#include "MediaQueryParser.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

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
 * throw SyntaxError exception.
 */
    
Ref<MediaQuerySet> MediaQuerySet::create(const String& mediaString, MediaQueryParserContext context)
{
    if (mediaString.isEmpty())
        return MediaQuerySet::create();

    auto parsedMediaQuerySet = MediaQueryParser::parseMediaQuerySet(mediaString, context);
    if (UNLIKELY(!parsedMediaQuerySet))
        return MediaQuerySet::create();
    
    return parsedMediaQuerySet.releaseNonNull();
}

MediaQuerySet::MediaQuerySet() = default;

MediaQuerySet::MediaQuerySet(const MediaQuerySet& o)
    : RefCounted()
    , m_queries(o.m_queries)
{
}

MediaQuerySet::~MediaQuerySet() = default;

bool MediaQuerySet::set(const String& mediaString)
{
    auto result = create(mediaString);
    m_queries = WTFMove(result->m_queries);
    return true;
}

bool MediaQuerySet::add(const String& queryString)
{
    // To "parse a media query" for a given string means to follow "the parse
    // a media query list" steps and return "null" if more than one media query
    // is returned, or else the returned media query.
    auto result = create(queryString);
    
    // Only continue if exactly one media query is found, as described above.
    if (result->m_queries.size() != 1)
        return false;
    
    // If comparing with any of the media queries in the collection of media
    // queries returns true terminate these steps.
    for (size_t i = 0; i < m_queries.size(); ++i) {
        if (m_queries[i] == result->m_queries[0])
            return false;
    }
    
    m_queries.append(result->m_queries[0]);
    return true;
}

bool MediaQuerySet::remove(const String& queryStringToRemove)
{
    // To "parse a media query" for a given string means to follow "the parse
    // a media query list" steps and return "null" if more than one media query
    // is returned, or else the returned media query.
    auto result = create(queryStringToRemove);
    
    // Only continue if exactly one media query is found, as described above.
    if (result->m_queries.size() != 1)
        return true;
    
    // Remove any media query from the collection of media queries for which
    // comparing with the media query returns true.
    bool found = false;
    
    // Using signed int here, since for the first value, --i will result in -1.
    for (int i = 0; i < (int)m_queries.size(); ++i) {
        if (m_queries[i] == result->m_queries[0]) {
            m_queries.remove(i);
            --i;
            found = true;
        }
    }
    
    return found;
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
            text.append(", ");
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

MediaList::~MediaList() = default;

void MediaList::setMediaText(const String& value)
{
    CSSStyleSheet::RuleMutationScope mutationScope(m_parentRule);
    m_mediaQueries->set(value);
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

ExceptionOr<void> MediaList::deleteMedium(const String& medium)
{
    CSSStyleSheet::RuleMutationScope mutationScope(m_parentRule);

    bool success = m_mediaQueries->remove(medium);
    if (!success)
        return Exception { NotFoundError };
    if (m_parentStyleSheet)
        m_parentStyleSheet->didMutate();
    return { };
}

void MediaList::appendMedium(const String& medium)
{
    CSSStyleSheet::RuleMutationScope mutationScope(m_parentRule);

    if (!m_mediaQueries->add(medium))
        return;
    if (m_parentStyleSheet)
        m_parentStyleSheet->didMutate();
}

void MediaList::reattach(MediaQuerySet* mediaQueries)
{
    ASSERT(mediaQueries);
    m_mediaQueries = mediaQueries;
}

TextStream& operator<<(TextStream& ts, const MediaQuerySet& querySet)
{
    ts << querySet.mediaText();
    return ts;
}

TextStream& operator<<(TextStream& ts, const MediaList& mediaList)
{
    ts << mediaList.mediaText();
    return ts;
}

} // namespace WebCore

