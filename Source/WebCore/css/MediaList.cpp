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
#include "CSSMediaRule.h"
#include "CSSStyleSheet.h"
#include "Document.h"
#include "LocalDOMWindow.h"
#include "MediaQuery.h"
#include "MediaQueryParser.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

MediaList::MediaList(CSSStyleSheet* parentSheet)
    : m_parentStyleSheet(parentSheet)
{
}

MediaList::MediaList(CSSRule* parentRule)
    : m_parentRule(parentRule)
{
}

MediaList::~MediaList() = default;

void MediaList::detachFromParent()
{
    m_detachedMediaQueries = mediaQueries();
    m_parentStyleSheet = nullptr;
    m_parentRule = nullptr;
}

unsigned MediaList::length() const
{
    return mediaQueries().size();
}

const MQ::MediaQueryList& MediaList::mediaQueries() const
{
    if (m_detachedMediaQueries)
        return *m_detachedMediaQueries;
    if (auto* rule = dynamicDowncast<CSSImportRule>(m_parentRule.get()))
        return rule->mediaQueries();
    if (auto* rule = dynamicDowncast<CSSMediaRule>(m_parentRule.get()))
        return rule->mediaQueries();
    return m_parentStyleSheet->mediaQueries();
}

void MediaList::setMediaQueries(MQ::MediaQueryList&& queries)
{
    if (RefPtr parentStyleSheet = m_parentStyleSheet.get()) {
        parentStyleSheet->setMediaQueries(WTF::move(queries));
        parentStyleSheet->didMutate();
        return;
    }

    CSSStyleSheet::RuleMutationScope mutationScope(m_parentRule.get());
    if (RefPtr rule = dynamicDowncast<CSSImportRule>(m_parentRule.get()))
        rule->setMediaQueries(WTF::move(queries));
    if (RefPtr rule = dynamicDowncast<CSSMediaRule>(m_parentRule.get()))
        rule->setMediaQueries(WTF::move(queries));
}

String MediaList::mediaText() const
{
    StringBuilder builder;
    MQ::serialize(builder, mediaQueries());
    return builder.toString();
}

void MediaList::setMediaText(const String& value)
{
    setMediaQueries(MQ::MediaQueryParser::parse(value, strictCSSParserContext()));
}

CSSRule* MediaList::parentRule() const
{
    return m_parentRule.get();
}

CSSStyleSheet* MediaList::parentStyleSheet() const
{
    return m_parentStyleSheet.get();
}

String MediaList::item(unsigned index) const
{
    auto& queries = mediaQueries();
    if (index < queries.size()) {
        StringBuilder builder;
        MQ::serialize(builder, queries[index]);
        return builder.toString();
    }
    return { };
}

ExceptionOr<void> MediaList::deleteMedium(const String& value)
{
    // https://drafts.csswg.org/cssom/#dom-medialist-deletemedium
    // The value is parsed as a single media query; bail unless parsing
    // produces exactly one query.
    auto parsedQueries = MQ::MediaQueryParser::parse(value, strictCSSParserContext());
    if (parsedQueries.size() != 1)
        return Exception { ExceptionCode::NotFoundError };

    auto serializeQuery = [](const MQ::MediaQuery& query) {
        StringBuilder builder;
        MQ::serialize(builder, query);
        return builder.toString();
    };
    auto queryToRemove = serializeQuery(parsedQueries[0]);
    auto queries = mediaQueries();
    auto removedCount = queries.removeAllMatching([&](auto& query) {
        return serializeQuery(query) == queryToRemove;
    });
    if (!removedCount)
        return Exception { ExceptionCode::NotFoundError };

    setMediaQueries(WTF::move(queries));
    return { };
}

void MediaList::appendMedium(const String& value)
{
    if (value.isEmpty())
        return;

    // https://drafts.csswg.org/cssom/#dom-medialist-appendmedium
    auto parsedQueries = MQ::MediaQueryParser::parse(value, strictCSSParserContext());
    if (parsedQueries.size() != 1)
        return;

    StringBuilder builder;
    MQ::serialize(builder, parsedQueries[0]);
    auto newQueryText = builder.toString();

    auto queries = mediaQueries();
    bool alreadyPresent = queries.containsIf([&](auto& query) {
        StringBuilder existingQueryBuilder;
        MQ::serialize(existingQueryBuilder, query);
        return existingQueryBuilder.toString() == newQueryText;
    });
    if (alreadyPresent)
        return;

    queries.append(WTF::move(parsedQueries[0]));
    setMediaQueries(WTF::move(queries));
}

TextStream& operator<<(TextStream& ts, const MediaList& mediaList)
{
    ts << mediaList.mediaText();
    return ts;
}

} // namespace WebCore

