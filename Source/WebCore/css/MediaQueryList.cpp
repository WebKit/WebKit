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
#include "MediaQueryList.h"

#include "AddEventListenerOptions.h"
#include "EventNames.h"
#include "HTMLFrameOwnerElement.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryListEvent.h"
#include "MediaQueryParser.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaQueryList);

MediaQueryList::MediaQueryList(Document& document, MediaQueryMatcher& matcher, MQ::MediaQueryList&& mediaQueries, bool matches)
    : ActiveDOMObject(&document)
    , m_matcher(&matcher)
    , m_mediaQueries(WTFMove(mediaQueries))
    , m_dynamicDependencies(MQ::MediaQueryEvaluator { matcher.mediaType() }.collectDynamicDependencies(m_mediaQueries))
    , m_evaluationRound(matcher.evaluationRound())
    , m_changeRound(m_evaluationRound - 1) // Any value that is not the same as m_evaluationRound would do.
    , m_matches(matches)
{
    matcher.addMediaQueryList(*this);
}

Ref<MediaQueryList> MediaQueryList::create(Document& document, MediaQueryMatcher& matcher, MQ::MediaQueryList&& mediaQueries, bool matches)
{
    auto list = adoptRef(*new MediaQueryList(document, matcher, WTFMove(mediaQueries), matches));
    list->suspendIfNeeded();
    return list;
}

MediaQueryList::~MediaQueryList()
{
    if (m_matcher)
        m_matcher->removeMediaQueryList(*this);
}

void MediaQueryList::detachFromMatcher()
{
    m_matcher = nullptr;
}

String MediaQueryList::media() const
{
    StringBuilder builder;
    MQ::serialize(builder, m_mediaQueries);
    return builder.toString();
}

void MediaQueryList::addListener(RefPtr<EventListener>&& listener)
{
    if (!listener)
        return;

    addEventListener(eventNames().changeEvent, listener.releaseNonNull(), { });
}

void MediaQueryList::removeListener(RefPtr<EventListener>&& listener)
{
    if (!listener)
        return;

    removeEventListener(eventNames().changeEvent, *listener, { });
}

void MediaQueryList::evaluate(MQ::MediaQueryEvaluator& evaluator, MediaQueryMatcher::EventMode eventMode)
{
    RELEASE_ASSERT(m_matcher);
    if (m_evaluationRound != m_matcher->evaluationRound())
        setMatches(evaluator.evaluate(m_mediaQueries));

    m_needsNotification = m_changeRound == m_matcher->evaluationRound() || m_needsNotification;
    if (!m_needsNotification || eventMode == MediaQueryMatcher::EventMode::Schedule)
        return;
    ASSERT(eventMode == MediaQueryMatcher::EventMode::DispatchNow);

    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (document && document->quirks().shouldSilenceMediaQueryListChangeEvents())
        return;

    dispatchEvent(MediaQueryListEvent::create(eventNames().changeEvent, media(), matches()));
    m_needsNotification = false;
}

void MediaQueryList::setMatches(bool newValue)
{
    ASSERT(m_matcher);
    m_evaluationRound = m_matcher->evaluationRound();

    if (newValue == m_matches)
        return;

    m_matches = newValue;
    m_changeRound = m_evaluationRound;
}

bool MediaQueryList::matches()
{
    if (!m_matcher)
        return m_matches;

    if (m_dynamicDependencies.contains(MQ::MediaQueryDynamicDependency::Viewport))  {
        if (RefPtr document = dynamicDowncast<Document>(scriptExecutionContext())) {
            if (RefPtr ownerElement = document->ownerElement()) {
                ownerElement->document().updateLayout();
                m_matcher->evaluateAll(MediaQueryMatcher::EventMode::Schedule);
            }
        }
    }

    if (m_evaluationRound != m_matcher->evaluationRound())
        setMatches(m_matcher->evaluate(m_mediaQueries));

    return m_matches;
}

void MediaQueryList::eventListenersDidChange()
{
    m_hasChangeEventListener = hasEventListeners(eventNames().changeEvent);
}

const char* MediaQueryList::activeDOMObjectName() const
{
    return "MediaQueryList";
}

bool MediaQueryList::virtualHasPendingActivity() const
{
    return m_hasChangeEventListener && m_matcher;
}

}
