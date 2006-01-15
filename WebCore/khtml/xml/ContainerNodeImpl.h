/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
 *
 */
#ifndef DOM_ContainerNodeImpl_h
#define DOM_ContainerNodeImpl_h

#include "NodeImpl.h"

namespace DOM {

class ContainerNodeImpl : public NodeImpl
{
public:
    ContainerNodeImpl(DocumentImpl *doc);
    virtual ~ContainerNodeImpl();

    // DOM methods overridden from  parent classes
    virtual NodeImpl *firstChild() const;
    virtual NodeImpl *lastChild() const;
    virtual NodeImpl *insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode );
    virtual NodeImpl *replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *removeChild ( NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *appendChild ( NodeImpl *newChild, int &exceptioncode );
    virtual bool hasChildNodes (  ) const;

    // Other methods (not part of DOM)
    void willRemove();
    int willRemoveChild(NodeImpl *child);
    void removeAllChildren();
    void removeChildren();
    void cloneChildNodes(NodeImpl *clone);

    virtual void setFirstChild(NodeImpl *child);
    virtual void setLastChild(NodeImpl *child);
    virtual NodeImpl *addChild(NodeImpl *newChild);
    virtual void attach();
    virtual void detach();

    virtual WebCore::IntRect getRect() const;
    bool getUpperLeftCorner(int &xPos, int &yPos) const;
    bool getLowerRightCorner(int &xPos, int &yPos) const;

    virtual void setFocus(bool=true);
    virtual void setActive(bool active = true, bool pause = false);
    virtual void setHovered(bool=true);
    virtual unsigned childNodeCount() const;
    virtual NodeImpl *childNode(unsigned index);

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void insertedIntoTree(bool deep);
    virtual void removedFromTree(bool deep);

    NodeImpl *_first;
    NodeImpl *_last;

    // helper functions for inserting children:

    // ### this should vanish. do it in dom/ !
    // check for same source document:
    bool checkSameDocument( NodeImpl *newchild, int &exceptioncode );
    // check for being child:
    bool checkIsChild( NodeImpl *oldchild, int &exceptioncode );
    // ###

    // find out if a node is allowed to be our child
    void dispatchChildInsertedEvents( NodeImpl *child, int &exceptioncode );
    void dispatchChildRemovalEvents( NodeImpl *child, int &exceptioncode );
};

}; //namespace
#endif
