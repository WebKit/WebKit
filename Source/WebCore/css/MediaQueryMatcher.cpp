/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "MediaQueryMatcher.h"

#include "Document.h"
#include "EventNames.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryList.h"
#include "MediaQueryParser.h"
#include "MediaQueryParserContext.h"
#include "NodeRenderStyle.h"
#include "RenderElement.h"
#include "ResolvedStyle.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

MediaQueryMatcher::MediaQueryMatcher(Document& document)
    : ContextDestructionObserver(&document)
{
}

MediaQueryMatcher::~MediaQueryMatcher() = default;

void MediaQueryMatcher::documentDestroyed()
{
    auto mediaQueryLists = std::exchange(m_mediaQueryLists, { });
    for (auto& mediaQueryList : mediaQueryLists) {
        if (mediaQueryList)
            mediaQueryList->detachFromMatcher();
    }
}

AtomString MediaQueryMatcher::mediaType() const
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (!document || !document->frame() || !document->frame()->view())
        return nullAtom();

    return document->frame()->view()->mediaType();
}

std::unique_ptr<RenderStyle> MediaQueryMatcher::documentElementUserAgentStyle() const
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (!document || !document->frame())
        return nullptr;

    auto* documentElement = document->documentElement();
    if (!documentElement)
        return nullptr;

    return document->styleScope().resolver().styleForElement(*documentElement, { document->renderStyle() }, RuleMatchingBehavior::MatchOnlyUserAgentRules).style;
}

bool MediaQueryMatcher::evaluate(const MQ::MediaQueryList& queries)
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    auto style = documentElementUserAgentStyle();
    if (!style)
        return false;
    return MQ::MediaQueryEvaluator { mediaType(), *document, style.get() }.evaluate(queries);
}

void MediaQueryMatcher::addMediaQueryList(MediaQueryList& list)
{
    ASSERT(!m_mediaQueryLists.contains(&list));
    m_mediaQueryLists.append(list);
}

void MediaQueryMatcher::removeMediaQueryList(MediaQueryList& list)
{
    m_mediaQueryLists.removeFirst(&list);
}

RefPtr<MediaQueryList> MediaQueryMatcher::matchMedia(const String& query)
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    if (!document)
        return nullptr;

    auto queries = MQ::MediaQueryParser::parse(query, MediaQueryParserContext(*document));
    bool matches = evaluate(queries);
    return MediaQueryList::create(*document, *this, WTFMove(queries), matches);
}

void MediaQueryMatcher::evaluateAll(EventMode eventMode)
{
    RefPtr document = downcast<Document>(scriptExecutionContext());
    ASSERT(document);

    ++m_evaluationRound;

    auto style = documentElementUserAgentStyle();
    if (!style)
        return;

    LOG_WITH_STREAM(MediaQueries, stream << "MediaQueryMatcher::styleResolverChanged " << document->url());

    MQ::MediaQueryEvaluator evaluator { mediaType(), *document, style.get() };

    auto mediaQueryLists = m_mediaQueryLists;
    for (auto& list : mediaQueryLists) {
        if (RefPtr protectedList = list.get()) {
            protectedList->evaluate(evaluator, eventMode);
            if (!document)
                break;
        }
    }
}

} // namespace WebCore
