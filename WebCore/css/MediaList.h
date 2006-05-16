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

#include "css_base.h"
#include "DeprecatedValueList.h"

namespace WebCore {

class CSSStyleSheet;

class MediaList : public StyleBase
{
public:
    MediaList() : StyleBase(0) {}
    MediaList(CSSStyleSheet* parentSheet);
    MediaList(CSSStyleSheet* parentSheet, const String& media);
    MediaList(CSSRule* parentRule, const String& media);

    virtual bool isMediaList() { return true; }

    CSSStyleSheet* parentStyleSheet() const;
    CSSRule* parentRule() const;
    unsigned length() const { return m_lstMedia.count(); }
    String item(unsigned index) const { return m_lstMedia[index]; }
    void deleteMedium(const String& oldMedium);
    void appendMedium(const String& newMedium) { m_lstMedia.append(newMedium); }

    String mediaText() const;
    void setMediaText(const String&);

    /**
     * Check if the list contains either the requested medium, or the
     * catch-all "all" media type. Returns true when found, false otherwise.
     * Since not specifying media types should be treated as "all" according
     * to DOM specs, an empty list always returns true.
     *
     * _NOT_ part of the DOM!
     */
    bool contains(const String& medium) const;

protected:
    DeprecatedValueList<String> m_lstMedia;
};

} // namespace

#endif
