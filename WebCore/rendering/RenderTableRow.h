/*
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

#ifndef RenderTableRow_H
#define RenderTableRow_H

#include "RenderTableSection.h"

namespace WebCore {

class RenderTableRow : public RenderContainer
{
public:
    RenderTableRow(NodeImpl*);

    virtual void destroy();
    virtual void setStyle(RenderStyle*);
    virtual const char* renderName() const { return "RenderTableRow"; }
    virtual bool isTableRow() const { return true; }
    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0);
    virtual short lineHeight(bool) const { return 0; }
    virtual void position(int, int, int, int, int, bool, bool, int) { }
    virtual void layout();
    virtual IntRect getAbsoluteRepaintRect();
    virtual bool nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty, HitTestAction action);

    // The only time rows get a layer is when they have transparency.
    virtual bool requiresLayer() { return isTransparent(); }

    virtual void paint(PaintInfo& i, int tx, int ty);

    RenderTable* table() const { return static_cast<RenderTable*>(parent()->parent()); }
    RenderTableSection* section() const { return static_cast<RenderTableSection*>(parent()); }
};

}

#endif
