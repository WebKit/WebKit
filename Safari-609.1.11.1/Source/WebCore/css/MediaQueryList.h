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

#pragma once

#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class MediaQueryListListener;
class MediaQueryEvaluator;
class MediaQueryMatcher;
class MediaQuerySet;

// MediaQueryList interface is specified at http://dev.w3.org/csswg/cssom-view/#the-mediaquerylist-interface
// The objects of this class are returned by window.matchMedia. They may be used to
// retrieve the current value of the given media query and to add/remove listeners that
// will be called whenever the value of the query changes.

class MediaQueryList final : public RefCounted<MediaQueryList> {
public:
    static Ref<MediaQueryList> create(MediaQueryMatcher&, Ref<MediaQuerySet>&&, bool);
    ~MediaQueryList();

    String media() const;
    bool matches();

    void addListener(RefPtr<MediaQueryListListener>&&);
    void removeListener(RefPtr<MediaQueryListListener>&&);

    void evaluate(MediaQueryEvaluator&, bool& notificationNeeded);

private:
    MediaQueryList(MediaQueryMatcher&, Ref<MediaQuerySet>&&, bool matches);

    void setMatches(bool);

    Ref<MediaQueryMatcher> m_matcher;
    Ref<MediaQuerySet> m_media;
    unsigned m_evaluationRound; // Indicates if the query has been evaluated after the last style selector change.
    unsigned m_changeRound; // Used to know if the query has changed in the last style selector change.
    bool m_matches;
};

}
