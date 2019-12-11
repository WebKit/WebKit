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

#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryListListener.h"
#include "MediaQueryMatcher.h"

namespace WebCore {

inline MediaQueryList::MediaQueryList(MediaQueryMatcher& matcher, Ref<MediaQuerySet>&& media, bool matches)
    : m_matcher(matcher)
    , m_media(WTFMove(media))
    , m_evaluationRound(m_matcher->evaluationRound())
    , m_changeRound(m_evaluationRound - 1) // Any value that is not the same as m_evaluationRound would do.
    , m_matches(matches)
{
}

Ref<MediaQueryList> MediaQueryList::create(MediaQueryMatcher& matcher, Ref<MediaQuerySet>&& media, bool matches)
{
    return adoptRef(*new MediaQueryList(matcher, WTFMove(media), matches));
}

MediaQueryList::~MediaQueryList() = default;

String MediaQueryList::media() const
{
    return m_media->mediaText();
}

void MediaQueryList::addListener(RefPtr<MediaQueryListListener>&& listener)
{
    if (!listener)
        return;

    m_matcher->addListener(listener.releaseNonNull(), *this);
}

void MediaQueryList::removeListener(RefPtr<MediaQueryListListener>&& listener)
{
    if (!listener)
        return;

    m_matcher->removeListener(*listener, *this);
}

void MediaQueryList::evaluate(MediaQueryEvaluator& evaluator, bool& notificationNeeded)
{
    if (m_evaluationRound != m_matcher->evaluationRound())
        setMatches(evaluator.evaluate(m_media.get()));
    notificationNeeded = m_changeRound == m_matcher->evaluationRound();
}

void MediaQueryList::setMatches(bool newValue)
{
    m_evaluationRound = m_matcher->evaluationRound();

    if (newValue == m_matches)
        return;

    m_matches = newValue;
    m_changeRound = m_evaluationRound;
}

bool MediaQueryList::matches()
{
    if (m_evaluationRound != m_matcher->evaluationRound())
        setMatches(m_matcher->evaluate(m_media.get()));
    return m_matches;
}

}
