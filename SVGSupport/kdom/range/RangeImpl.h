/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
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

#ifndef KDOM_RangeImpl_H
#define KDOM_RangeImpl_H

#include <kdom/Shared.h>

namespace KDOM
{

class NodeImpl;
class DocumentPtr;
class DOMStringImpl;
class DocumentFragmentImpl;

class RangeImpl : public Shared<RangeImpl>
{
friend class DocumentImpl;
public:
    RangeImpl(DocumentPtr *_ownerDocument);
    RangeImpl(DocumentPtr *_ownerDocument,
              NodeImpl *_startContainer, long _startOffset,
              NodeImpl *_endContainer, long _endOffset);
    virtual ~RangeImpl();

    NodeImpl *startContainer() const;
    long startOffset() const;
    NodeImpl *endContainer() const;
    long endOffset() const;
    bool isCollapsed() const;

    NodeImpl *commonAncestorContainer();
    static NodeImpl *commonAncestorContainer(NodeImpl *containerA, NodeImpl *containerB);
    
    void setStart(NodeImpl *refNode, long offset);
    void setEnd(NodeImpl *refNode, long offset);
    void collapse(bool toStart);
    
    short compareBoundaryPoints(unsigned short how, RangeImpl *sourceRange);
    static short compareBoundaryPoints(NodeImpl *containerA, long offsetA, NodeImpl *containerB, long offsetB);
    
    bool boundaryPointsValid();
    void deleteContents();
    
    DocumentFragmentImpl *extractContents();
    DocumentFragmentImpl *cloneContents();
    
    void insertNode(NodeImpl *newNode);

    void detach();
    bool isDetached() const;
    
    RangeImpl *cloneRange();
    DOMStringImpl *toString();

    void setStartAfter(NodeImpl *refNode);
    void setEndBefore(NodeImpl *refNode);
    void setEndAfter(NodeImpl *refNode);
    void selectNode(NodeImpl *refNode);
    void selectNodeContents(NodeImpl *refNode);
    void surroundContents(NodeImpl *newParent);
    void setStartBefore(NodeImpl *refNode);

    enum ActionType
    {
        DELETE_CONTENTS,
        EXTRACT_CONTENTS,
        CLONE_CONTENTS
    };
    
    DocumentFragmentImpl *processContents(ActionType action);

    bool readOnly() { return false; }

protected:
    DocumentPtr *m_ownerDocument;
    NodeImpl *m_startContainer;
    unsigned long m_startOffset;
    NodeImpl *m_endContainer;
    unsigned long m_endOffset;
    bool m_detached;

private:
    void checkNodeWOffset(NodeImpl *n, int offset) const;
    void checkNodeBA(NodeImpl *n) const;
    void setStartContainer(NodeImpl *_startContainer);
    void setEndContainer(NodeImpl *_endContainer);
    void checkDeleteExtract();
    bool containedByReadOnly();
};

} // namespace

#endif

// vim:ts=4:noet
