/**
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
#include "config.h"
#include "MediaList.h"

#include "CSSStyleSheet.h"
#include "css_ruleimpl.h"
#include "DeprecatedStringList.h"

namespace WebCore {

MediaList::MediaList(CSSStyleSheet* parentSheet)
    : StyleBase(parentSheet)
{
}

MediaList::MediaList(CSSStyleSheet* parentSheet, const String& media)
    : StyleBase(parentSheet)
{
    setMediaText(media);
}

MediaList::MediaList(CSSRule* parentRule, const String& media)
    : StyleBase(parentRule)
{
    setMediaText(media);
}

bool MediaList::contains(const String& medium) const
{
    return m_lstMedia.count() == 0 || m_lstMedia.contains(medium) || m_lstMedia.contains("all");
}

CSSStyleSheet *MediaList::parentStyleSheet() const
{
    return parent()->isCSSStyleSheet() ? static_cast<CSSStyleSheet*>(parent()) : 0;
}

CSSRule *MediaList::parentRule() const
{
    return parent()->isRule() ? static_cast<CSSRule*>(parent()) : 0;
}

void MediaList::deleteMedium(const String& oldMedium)
{
    for (DeprecatedValueList<String>::Iterator it = m_lstMedia.begin(); it != m_lstMedia.end(); ++it) {
        if ((*it) == oldMedium) {
            m_lstMedia.remove(it);
            return;
        }
    }
}

WebCore::String MediaList::mediaText() const
{
    String text = "";
    for (DeprecatedValueList<String>::ConstIterator it = m_lstMedia.begin(); it != m_lstMedia.end(); ++it) {
        if (text.length() > 0)
            text += ", ";
        text += *it;
    }
    return text;
}

void MediaList::setMediaText(const WebCore::String& value)
{
    m_lstMedia.clear();
    DeprecatedStringList list = DeprecatedStringList::split(',', value.deprecatedString());
    for (DeprecatedStringList::Iterator it = list.begin(); it != list.end(); ++it) {
        String medium = (*it).stripWhiteSpace();
        if (!medium.isEmpty())
            m_lstMedia.append(medium);
    }
}

}
