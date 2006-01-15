/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
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

#ifndef _DOM2_RangeImpl_h_
#define _DOM2_RangeImpl_h_

#include "dom/dom2_range.h"
#include "Shared.h"
#include "DocPtr.h"

namespace DOM {

class DocumentFragmentImpl;
class DOMString;
class NodeImpl;
class Position;

class RangeImpl : public Shared<RangeImpl>
{
    friend class DocumentImpl;
public:
    RangeImpl(DocumentImpl *_ownerDocument);
    RangeImpl(DocumentImpl *_ownerDocument,
              NodeImpl *_startContainer, int _startOffset,
              NodeImpl *_endContainer, int _endOffset);

    ~RangeImpl();

    NodeImpl *startContainer(int &exceptioncode) const;
    int startOffset(int &exceptioncode) const;
    NodeImpl *endContainer(int &exceptioncode) const;
    int endOffset(int &exceptioncode) const;
    bool collapsed(int &exceptioncode) const;

    NodeImpl *commonAncestorContainer(int &exceptioncode) const;
    static NodeImpl *commonAncestorContainer(NodeImpl *containerA, NodeImpl *containerB);
    void setStart ( NodeImpl *refNode, int offset, int &exceptioncode );
    void setEnd ( NodeImpl *refNode, int offset, int &exceptioncode );
    void collapse ( bool toStart, int &exceptioncode );
    short compareBoundaryPoints ( Range::CompareHow how, const RangeImpl *sourceRange, int &exceptioncode ) const;
    static short compareBoundaryPoints ( NodeImpl *containerA, int offsetA, NodeImpl *containerB, int offsetB );
    static short compareBoundaryPoints ( const Position &a, const Position &b );
    bool boundaryPointsValid (  ) const;
    void deleteContents ( int &exceptioncode );
    DocumentFragmentImpl *extractContents ( int &exceptioncode );
    DocumentFragmentImpl *cloneContents ( int &exceptioncode );
    void insertNode( NodeImpl *newNode, int &exceptioncode );
    DOMString toString ( int &exceptioncode ) const;
    DOMString toHTML() const;
    DOMString text() const;

    DocumentFragmentImpl *createContextualFragment(const DOMString &html, int &exceptioncode) const;
    
    void detach ( int &exceptioncode );
    bool isDetached() const;
    RangeImpl *cloneRange(int &exceptioncode) const;

    void setStartAfter( NodeImpl *refNode, int &exceptioncode );
    void setEndBefore( NodeImpl *refNode, int &exceptioncode );
    void setEndAfter( NodeImpl *refNode, int &exceptioncode );
    void selectNode( NodeImpl *refNode, int &exceptioncode );
    void selectNodeContents( NodeImpl *refNode, int &exceptioncode );
    void surroundContents( NodeImpl *newParent, int &exceptioncode );
    void setStartBefore( NodeImpl *refNode, int &exceptioncode );

    enum ActionType {
        DELETE_CONTENTS,
        EXTRACT_CONTENTS,
        CLONE_CONTENTS
    };
    DocumentFragmentImpl *processContents ( ActionType action, int &exceptioncode );

    Position startPosition() const;
    Position endPosition() const;

    NodeImpl *startNode() const;
    NodeImpl *pastEndNode() const;

    Position editingStartPosition() const;

#ifndef NDEBUG
    void formatForDebugger(char *buffer, unsigned length) const;
#endif

protected:
    // FIXME - this could be a full-on RefPtr in principle...
    DocPtr<DocumentImpl> m_ownerDocument;
    NodeImpl *m_startContainer;
    unsigned m_startOffset;
    NodeImpl *m_endContainer;
    unsigned m_endOffset;
    bool m_detached;

private:
    void checkNodeWOffset( NodeImpl *n, int offset, int &exceptioncode) const;
    void checkNodeBA( NodeImpl *n, int &exceptioncode ) const;
    void setStartContainer(NodeImpl *_startContainer);
    void setEndContainer(NodeImpl *_endContainer);
    void checkDeleteExtract(int &exceptioncode);
    bool containedByReadOnly() const;
};

RefPtr<RangeImpl> rangeOfContents(NodeImpl *node);

bool operator==(const RangeImpl &, const RangeImpl &);
inline bool operator!=(const RangeImpl &a, const RangeImpl &b) { return !(a == b); }

} // namespace

#endif
