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

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "MediaQuery.h"
#include "MediaQueryMatcher.h"

namespace WebCore {

namespace MQ {
class MediaQueryEvaluator;
}

// MediaQueryList interface is specified at https://drafts.csswg.org/cssom-view/#the-mediaquerylist-interface
// The objects of this class are returned by window.matchMedia. They may be used to
// retrieve the current value of the given media query and to add/remove listeners that
// will be called whenever the value of the query changes.

class MediaQueryList final : public RefCounted<MediaQueryList>, public EventTarget, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(MediaQueryList);
public:
    static Ref<MediaQueryList> create(Document&, MediaQueryMatcher&, MQ::MediaQueryList&&, bool matches);
    ~MediaQueryList();

    String media() const;
    bool matches();

    void addListener(RefPtr<EventListener>&&);
    void removeListener(RefPtr<EventListener>&&);

    void evaluate(MQ::MediaQueryEvaluator&, MediaQueryMatcher::EventMode);

    void detachFromMatcher();

    using RefCounted::ref;
    using RefCounted::deref;

private:
    MediaQueryList(Document&, MediaQueryMatcher&, MQ::MediaQueryList&&, bool matches);

    void setMatches(bool);

    EventTargetInterface eventTargetInterface() const final { return MediaQueryListEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    void eventListenersDidChange() final;

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    RefPtr<MediaQueryMatcher> m_matcher;
    const MQ::MediaQueryList m_mediaQueries;
    const OptionSet<MQ::MediaQueryDynamicDependency> m_dynamicDependencies;
    unsigned m_evaluationRound; // Indicates if the query has been evaluated after the last style selector change.
    unsigned m_changeRound; // Used to know if the query has changed in the last style selector change.
    bool m_matches;
    bool m_hasChangeEventListener { false };
    bool m_needsNotification { false };
};

}
