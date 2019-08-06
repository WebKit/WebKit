/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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

#pragma once

#include "ExceptionOr.h"
#include "MediaQueryParserContext.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class CSSParser;
class CSSRule;
class CSSStyleSheet;
class Document;
class MediaQuery;

class MediaQuerySet final : public RefCounted<MediaQuerySet> {
public:
    static Ref<MediaQuerySet> create()
    {
        return adoptRef(*new MediaQuerySet);
    }
    static WEBCORE_EXPORT Ref<MediaQuerySet> create(const String& mediaString, MediaQueryParserContext = MediaQueryParserContext());

    WEBCORE_EXPORT ~MediaQuerySet();

    bool set(const String&);
    bool add(const String&);
    bool remove(const String&);

    void addMediaQuery(MediaQuery&&);

    const Vector<MediaQuery>& queryVector() const { return m_queries; }

    WEBCORE_EXPORT String mediaText() const;

    Ref<MediaQuerySet> copy() const { return adoptRef(*new MediaQuerySet(*this)); }

    void shrinkToFit();

private:
    MediaQuerySet();
    WEBCORE_EXPORT MediaQuerySet(const String& mediaQuery);
    MediaQuerySet(const MediaQuerySet&);

    Vector<MediaQuery> m_queries;
};

class MediaList final : public RefCounted<MediaList> {
public:
    static Ref<MediaList> create(MediaQuerySet* mediaQueries, CSSStyleSheet* parentSheet)
    {
        return adoptRef(*new MediaList(mediaQueries, parentSheet));
    }
    static Ref<MediaList> create(MediaQuerySet* mediaQueries, CSSRule* parentRule)
    {
        return adoptRef(*new MediaList(mediaQueries, parentRule));
    }

    WEBCORE_EXPORT ~MediaList();

    unsigned length() const { return m_mediaQueries->queryVector().size(); }
    WEBCORE_EXPORT String item(unsigned index) const;
    WEBCORE_EXPORT ExceptionOr<void> deleteMedium(const String& oldMedium);
    WEBCORE_EXPORT void appendMedium(const String& newMedium);

    String mediaText() const { return m_mediaQueries->mediaText(); }
    WEBCORE_EXPORT ExceptionOr<void> setMediaText(const String&);

    CSSRule* parentRule() const { return m_parentRule; }
    CSSStyleSheet* parentStyleSheet() const { return m_parentStyleSheet; }
    void clearParentStyleSheet() { ASSERT(m_parentStyleSheet); m_parentStyleSheet = nullptr; }
    void clearParentRule() { ASSERT(m_parentRule); m_parentRule = nullptr; }
    const MediaQuerySet* queries() const { return m_mediaQueries.get(); }

    void reattach(MediaQuerySet*);

private:
    MediaList();
    MediaList(MediaQuerySet*, CSSStyleSheet* parentSheet);
    MediaList(MediaQuerySet*, CSSRule* parentRule);

    RefPtr<MediaQuerySet> m_mediaQueries;
    CSSStyleSheet* m_parentStyleSheet { nullptr };
    CSSRule* m_parentRule { nullptr };
};

// Adds message to inspector console whenever dpi or dpcm values are used for "screen" media.
// FIXME: Seems strange to have this here in this file, and unclear exactly who should call this and when.
void reportMediaQueryWarningIfNeeded(Document*, const MediaQuerySet*);

#if !ENABLE(RESOLUTION_MEDIA_QUERY)

inline void reportMediaQueryWarningIfNeeded(Document*, const MediaQuerySet*)
{
}

#endif

WTF::TextStream& operator<<(WTF::TextStream&, const MediaQuerySet&);
WTF::TextStream& operator<<(WTF::TextStream&, const MediaList&);

} // namespace
