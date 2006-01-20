/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
#include "RenderTableCol.h"
#include "html_tableimpl.h"
#include "htmlnames.h"
#include <qtextstream.h>

namespace WebCore {

using namespace HTMLNames;

RenderTableCol::RenderTableCol(NodeImpl* node)
    : RenderContainer(node)
{
    // init RenderObject attributes
    setInline(true);   // our object is not Inline

    _span = 1;
    updateFromElement();
}

void RenderTableCol::updateFromElement()
{
    int oldSpan = _span;
    NodeImpl *node = element();
    if (node && (node->hasTagName(colTag) || node->hasTagName(colgroupTag))) {
        HTMLTableColElementImpl *tc = static_cast<HTMLTableColElementImpl *>(node);
        _span = tc->span();
    } else
      _span = !(style() && style()->display() == TABLE_COLUMN_GROUP);
    if (_span != oldSpan && style() && parent())
        setNeedsLayoutAndMinMaxRecalc();
}

bool RenderTableCol::canHaveChildren() const
{
    // cols cannot have children.  This is actually necessary to fix a bug
    // with libraries.uc.edu, which makes a <p> be a table-column.
    return style()->display() == TABLE_COLUMN_GROUP;
}

void RenderTableCol::addChild(RenderObject *child, RenderObject *beforeChild)
{
    KHTMLAssert(child->style()->display() == TABLE_COLUMN);

    // these have to come before the table definition!
    RenderContainer::addChild(child, beforeChild);
}

#ifndef NDEBUG
void RenderTableCol::dump(QTextStream *stream, QString ind) const
{
    *stream << " _span=" << _span;
    RenderContainer::dump(stream, ind);
}
#endif

}
