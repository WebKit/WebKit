/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef RenderTableCol_h
#define RenderTableCol_h

#include "RenderBox.h"

namespace WebCore {

class RenderTableCol : public RenderBox
{
public:
    RenderTableCol(Node*);

    virtual RenderObjectChildList* virtualChildren() { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const { return children(); }
    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    virtual const char* renderName() const { return "RenderTableCol"; }
    virtual bool isTableCol() const { return true; }
    virtual int lineHeight(bool) const { return 0; }
    virtual void updateFromElement();

    virtual bool isChildAllowed(RenderObject*, RenderStyle*) const;
    virtual bool canHaveChildren() const;
    virtual bool requiresLayer() const { return false; }

    virtual IntRect clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer);
    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0);

    int span() const { return m_span; }
    void setSpan(int s) { m_span = s; }
    
private:
    RenderObjectChildList m_children;
    int m_span;
};

}

#endif
