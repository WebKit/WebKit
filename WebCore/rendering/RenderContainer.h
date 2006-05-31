/*
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2001 Antti Koivisto (koivisto@kde.org)
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

#ifndef RenderContainer_H
#define RenderContainer_H

#include "RenderBox.h"

namespace WebCore {

class Position;

/**
 * Base class for rendering objects that can have children
 */
class RenderContainer : public RenderBox
{
public:
    RenderContainer(Node*);
    virtual ~RenderContainer();

    RenderObject *firstChild() const { return m_first; }
    RenderObject *lastChild() const { return m_last; }

    virtual bool canHaveChildren() const;
    virtual void addChild(RenderObject *newChild, RenderObject *beforeChild = 0);
    virtual void removeChild(RenderObject *oldChild);

    virtual void destroy();
    void destroyLeftoverChildren();
    
    virtual RenderObject* removeChildNode(RenderObject* child);
    virtual void appendChildNode(RenderObject* child);
    virtual void insertChildNode(RenderObject* child, RenderObject* before);

    virtual void layout();
    virtual void calcMinMaxWidth() { setMinMaxKnown( true ); }

    virtual void removeLeftoverAnonymousBoxes();

    void updatePseudoChild(RenderStyle::PseudoId type, RenderObject* child);

    virtual VisiblePosition positionForCoordinates(int x, int y);
    
    virtual DeprecatedValueList<IntRect> lineBoxRects();

protected:
    void setFirstChild(RenderObject *first) { m_first = first; }
    void setLastChild(RenderObject *last) { m_last = last; }

protected:
    RenderObject *m_first;
    RenderObject *m_last;
};

}
#endif
