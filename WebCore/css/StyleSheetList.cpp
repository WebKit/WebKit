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
#include "StyleSheetList.h"

#include "CSSStyleSheet.h"

namespace WebCore {

StyleSheetList::~StyleSheetList()
{
    for (DeprecatedPtrListIterator<StyleSheet> it (styleSheets); it.current(); ++it)
        it.current()->deref();
}

void StyleSheetList::add(StyleSheet* s)
{
    if (!styleSheets.containsRef(s)) {
        s->ref();
        styleSheets.append(s);
    }
}

void StyleSheetList::remove(StyleSheet* s)
{
    if (styleSheets.removeRef(s))
        s->deref();
}

unsigned StyleSheetList::length() const
{
    // hack so implicit BODY stylesheets don't get counted here
    unsigned l = 0;
    DeprecatedPtrListIterator<StyleSheet> it(styleSheets);
    for (; it.current(); ++it) {
        if (!it.current()->isCSSStyleSheet() || !static_cast<CSSStyleSheet*>(it.current())->implicit())
            l++;
    }
    return l;
}

StyleSheet *StyleSheetList::item (unsigned index)
{
    unsigned l = 0;
    DeprecatedPtrListIterator<StyleSheet> it(styleSheets);
    for (; it.current(); ++it) {
        if (!it.current()->isCSSStyleSheet() || !static_cast<CSSStyleSheet*>(it.current())->implicit()) {
            if (l == index)
                return it.current();
            l++;
        }
    }
    return 0;
}

}
