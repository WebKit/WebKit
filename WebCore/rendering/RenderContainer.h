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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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

    virtual RenderObject* removeChildNode(RenderObject*, bool fullRemove = true);
    virtual void appendChildNode(RenderObject*, bool fullAppend = true);
    virtual void insertChildNode(RenderObject* child, RenderObject* before, bool fullInsert = true);
    
    // Designed for speed.  Don't waste time doing a bunch of work like layer updating and repainting when we know that our
    // change in parentage is not going to affect anything.
    virtual void moveChildNode(RenderObject* child) { appendChildNode(child->parent()->removeChildNode(child, false), false); }
    
    virtual void layout();
    virtual void calcPrefWidths() { setPrefWidthsDirty(false); }

    virtual void removeLeftoverAnonymousBlock(RenderBlock* child);

    RenderObject* beforeAfterContainer(RenderStyle::PseudoId);
    virtual void updateBeforeAfterContent(RenderStyle::PseudoId);
    void updateBeforeAfterContentForContainer(RenderStyle::PseudoId, RenderContainer*);
    bool isAfterContent(RenderObject* child) const;
    virtual void invalidateCounters();

    virtual VisiblePosition positionForCoordinates(int x, int y);

    virtual void addLineBoxRects(Vector<IntRect>&, unsigned startOffset = 0, unsigned endOffset = UINT_MAX, bool useSelectionHeight = false);
    virtual void collectAbsoluteLineBoxQuads(Vector<FloatQuad>&, unsigned startOffset = 0, unsigned endOffset = UINT_MAX, bool useSelectionHeight = false);

private:
    RenderObject* m_firstChild;
    RenderObject* m_lastChild;
};

} // namespace WebCore

#endif // RenderContainer_h
