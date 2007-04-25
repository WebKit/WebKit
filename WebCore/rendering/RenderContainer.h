/*
 * Copyright (C) 2001 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef RenderContainer_h
#define RenderContainer_h

#include "RenderBox.h"

namespace WebCore {

// Base class for rendering objects that can have children.
class RenderContainer : public RenderBox {
public:
    RenderContainer(Node*);
    virtual ~RenderContainer();

    virtual RenderObject* firstChild() const { return m_firstChild; }
    virtual RenderObject* lastChild() const { return m_lastChild; }

    virtual bool canHaveChildren() const;
    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0);
    virtual void removeChild(RenderObject*);

    virtual void destroy();
    void destroyLeftoverChildren();

    virtual RenderObject* removeChildNode(RenderObject*);
    virtual void appendChildNode(RenderObject*);
    virtual void insertChildNode(RenderObject* child, RenderObject* before);

    virtual void layout();
    virtual void calcPrefWidths() { setPrefWidthsDirty(false); }

    virtual void removeLeftoverAnonymousBoxes();

    RenderObject* pseudoChild(RenderStyle::PseudoId) const;
    virtual void updatePseudoChild(RenderStyle::PseudoId);
    void updatePseudoChildForObject(RenderStyle::PseudoId, RenderObject*);

    virtual VisiblePosition positionForCoordinates(int x, int y);

    virtual void addLineBoxRects(Vector<IntRect>&, unsigned startOffset = 0, unsigned endOffset = UINT_MAX);

private:
    RenderObject* m_firstChild;
    RenderObject* m_lastChild;
};

} // namespace WebCore

#endif // RenderContainer_h
