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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef RenderTableRow_h
#define RenderTableRow_h

#include "RenderTableSection.h"

namespace WebCore {

class RenderTableRow : public RenderContainer {
public:
    RenderTableRow(Node*);

    virtual const char* renderName() const { return isAnonymous() ? "RenderTableRow (anonymous)" : "RenderTableRow"; }

    virtual bool isTableRow() const { return true; }

    virtual void destroy();

    RenderTableSection* section() const { return static_cast<RenderTableSection*>(parent()); }
    RenderTable* table() const { return static_cast<RenderTable*>(parent()->parent()); }

    virtual void setStyle(RenderStyle*);
    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0);
    virtual short lineHeight(bool firstLine, bool isRootLineBox = false) const { return 0; }
    virtual void position(InlineBox*) { }
    virtual void layout();
    virtual IntRect absoluteClippedOverflowRect();
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

    // The only time rows get a layer is when they have transparency.
    virtual bool requiresLayer() { return isTransparent() || hasOverflowClip(); }

    virtual void paint(PaintInfo&, int tx, int ty);
    virtual void imageChanged(CachedImage*);
};

} // namespace WebCore

#endif // RenderTableRow_h
