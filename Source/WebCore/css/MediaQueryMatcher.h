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

#include <memory>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class MediaQueryList;
class LegacyMediaQueryEvaluator;
class MediaQuerySet;
class RenderStyle;
class WeakPtrImplWithEventTargetData;

// MediaQueryMatcher class is responsible for evaluating the queries whenever it
// is needed and dispatch "change" event on MediaQueryLists if the corresponding
// query has changed. MediaQueryLists are invoked in the order in which they were added.

class MediaQueryMatcher final : public RefCounted<MediaQueryMatcher> {
public:
    static Ref<MediaQueryMatcher> create(Document& document) { return adoptRef(*new MediaQueryMatcher(document)); }
    ~MediaQueryMatcher();

    void documentDestroyed();
    void addMediaQueryList(MediaQueryList&);
    void removeMediaQueryList(MediaQueryList&);

    RefPtr<MediaQueryList> matchMedia(const String&);

    unsigned evaluationRound() const { return m_evaluationRound; }

    enum class EventMode : uint8_t { Schedule, DispatchNow };
    void evaluateAll(EventMode);

    bool evaluate(const MediaQuerySet&);

private:
    explicit MediaQueryMatcher(Document&);
    std::unique_ptr<RenderStyle> documentElementUserAgentStyle() const;
    AtomString mediaType() const;

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
    Vector<WeakPtr<MediaQueryList, WeakPtrImplWithEventTargetData>> m_mediaQueryLists;

    // This value is incremented at style selector changes.
    // It is used to avoid evaluating queries more then once and to make sure
    // that a media query result change is notified exactly once.
    unsigned m_evaluationRound { 1 };
};

} // namespace WebCore
