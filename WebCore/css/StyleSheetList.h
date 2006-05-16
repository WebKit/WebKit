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

#ifndef StyleSheetList_H
#define StyleSheetList_H

#include "Shared.h"
#include "DeprecatedPtrList.h"

namespace WebCore {

class StyleSheet;

class StyleSheetList : public Shared<StyleSheetList>
{
public:
    ~StyleSheetList();

    // the following two ignore implicit stylesheets
    unsigned length() const;
    StyleSheet* item(unsigned index);

    void add(StyleSheet*);
    void remove(StyleSheet*);

    DeprecatedPtrList<StyleSheet> styleSheets;
};

} // namespace

#endif
