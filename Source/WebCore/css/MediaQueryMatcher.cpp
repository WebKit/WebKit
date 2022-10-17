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
#include "Frame.h"
#include "FrameView.h"
#include "LegacyMediaQueryEvaluator.h"
#include "Logging.h"
#include "MediaList.h"
#include "MediaQueryList.h"
#include "MediaQueryParserContext.h"
#include "NodeRenderStyle.h"
#include "RenderElement.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

MediaQueryMatcher::MediaQueryMatcher(Document& document)
    : m_document(document)
{
}

MediaQueryMatcher::~MediaQueryMatcher() = default;

void MediaQueryMatcher::documentDestroyed()
{
    m_document = nullptr;
    auto mediaQueryLists = std::exchange(m_mediaQueryLists, { });
    for (auto& mediaQueryList : mediaQueryLists) {
        if (mediaQueryList)
            mediaQueryList->detachFromMatcher();
    }
}

String MediaQueryMatcher::mediaType() const
{
    if (!m_document || !m_document->frame() || !m_document->frame()->view())
        return String();

    return m_document->frame()->view()->mediaType();
}

std::unique_ptr<RenderStyle> MediaQueryMatcher::documentElementUserAgentStyle() const
{
    if (!m_document || !m_document->frame())
        return nullptr;

    auto* documentElement = m_document->documentElement();
    if (!documentElement)
        return nullptr;

    return m_document->styleScope().resolver().styleForElement(*documentElement, { m_document->renderStyle() }, RuleMatchingBehavior::MatchOnlyUserAgentRules).renderStyle;
}

bool MediaQueryMatcher::evaluate(const MediaQuerySet& media)
{
    auto style = documentElementUserAgentStyle();
    if (!style)
        return false;
    return LegacyMediaQueryEvaluator { mediaType(), *m_document, style.get() }.evaluate(media);
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
    if (!m_document)
        return nullptr;

    auto media = MediaQuerySet::create(query, MediaQueryParserContext(*m_document));
    bool matches = evaluate(media.get());
    return MediaQueryList::create(*m_document, *this, WTFMove(media), matches);
}

void MediaQueryMatcher::evaluateAll(EventMode eventMode)
{
    ASSERT(m_document);

    ++m_evaluationRound;

    auto style = documentElementUserAgentStyle();
    if (!style)
        return;

    LOG_WITH_STREAM(MediaQueries, stream << "MediaQueryMatcher::styleResolverChanged " << m_document->url());

    LegacyMediaQueryEvaluator evaluator { mediaType(), *m_document, style.get() };

    auto mediaQueryLists = m_mediaQueryLists;
    for (auto& list : mediaQueryLists) {
        if (RefPtr protectedList = list.get()) {
            protectedList->evaluate(evaluator, eventMode);
            if (!m_document)
                break;
        }
    }
}

} // namespace WebCore
