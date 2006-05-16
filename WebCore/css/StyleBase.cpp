/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 *               2001 Andreas Schlapbach (schlpbch@iam.unibe.ch)
 *               2001-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006 Apple Computer, Inc.
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
#include "StyleBase.h"

#include "Document.h"
#include "Node.h"
#include "StyleSheet.h"

namespace WebCore {

void StyleBase::checkLoaded()
{
    if (parent())
        parent()->checkLoaded();
}

StyleSheet* StyleBase::stylesheet()
{
    StyleBase *b = this;
    while (b && !b->isStyleSheet())
        b = b->parent();
    return static_cast<StyleSheet*>(b);
}

String StyleBase::baseURL()
{
    // try to find the style sheet. If found look for its url.
    // If it has none, look for the parentsheet, or the parentNode and
    // try to find out about their url

    StyleSheet* sheet = stylesheet();

    if (!sheet)
        return String();

    if (!sheet->href().isNull())
        return sheet->href();

    // find parent
    if (sheet->parent()) 
        return sheet->parent()->baseURL();

    if (!sheet->ownerNode()) 
        return String();

    return sheet->ownerNode()->document()->baseURL();
}

}
