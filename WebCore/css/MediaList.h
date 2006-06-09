/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef MediaList_H
#define MediaList_H

#include "StyleBase.h"
#include <wtf/Vector.h>
#include "PlatformString.h"

namespace WebCore {
typedef int ExceptionCode;
class CSSStyleSheet;
class MediaQuery;
class CSSRule;

class MediaList : public StyleBase
{
public:
    MediaList(bool fallbackToDescription = false) : StyleBase(0), m_fallback(fallbackToDescription) {}
    MediaList(CSSStyleSheet* parentSheet, bool fallbackToDescription = false);
    MediaList(CSSStyleSheet* parentSheet, const String& media, bool fallbackToDescription = false);
    MediaList(CSSRule* parentRule, const String& media, bool fallbackToDescription = false);
    ~MediaList();

    virtual bool isMediaList() { return true; }

    CSSStyleSheet* parentStyleSheet() const;
    CSSRule* parentRule() const;
    unsigned length() const { return (unsigned) m_queries.size(); }
    String item(unsigned index) const;
    void deleteMedium(const String& oldMedium, ExceptionCode&);
    void appendMedium(const String& newMedium, ExceptionCode&);

    String mediaText() const;
    void setMediaText(const String&, ExceptionCode&xo);

    void appendMediaQuery(MediaQuery* mediaQuery);
    const Vector<MediaQuery*>* mediaQueries() const { return &m_queries; }

protected:
    Vector<MediaQuery*> m_queries;
    bool m_fallback; // true if failed media query parsing should fallback to media description parsing
};

} // namespace

#endif
