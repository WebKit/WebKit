/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef DOM2_RangeImpl_h_
#define DOM2_RangeImpl_h_

#include "dom2_range.h"
#include "Shared.h"
#include <kxmlcore/RefPtr.h>

namespace KXMLCore {
    template <typename T> class PassRefPtr;
}
using KXMLCore::PassRefPtr;

namespace WebCore {

typedef int ExceptionCode;

class DocumentFragmentImpl;
class DocumentImpl;
class NodeImpl;
class Position;
class String;

class RangeImpl : public Shared<RangeImpl>
{
public:
    RangeImpl(DocumentImpl*);
    RangeImpl(DocumentImpl*, NodeImpl* startContainer, int startOffset, NodeImpl* endContainer, int endOffset);

    NodeImpl* startContainer(ExceptionCode&) const;
    int startOffset(ExceptionCode&) const;
    NodeImpl* endContainer(ExceptionCode&) const;
    int endOffset(ExceptionCode&) const;
    bool collapsed(ExceptionCode&) const;

    NodeImpl* commonAncestorContainer(ExceptionCode&) const;
    static NodeImpl* commonAncestorContainer(NodeImpl* containerA, NodeImpl* containerB);
    void setStart(NodeImpl* container, int offset, ExceptionCode&);
    void setEnd(NodeImpl* container, int offset, ExceptionCode&);
    void collapse(bool toStart, ExceptionCode&);
    short compareBoundaryPoints(Range::CompareHow how, const RangeImpl* sourceRange, ExceptionCode&) const;
    static short compareBoundaryPoints(NodeImpl* containerA, int offsetA, NodeImpl* containerB, int offsetB );
    static short compareBoundaryPoints(const Position&, const Position&);
    bool boundaryPointsValid() const;
    void deleteContents(ExceptionCode&);
    PassRefPtr<DocumentFragmentImpl> extractContents(ExceptionCode&);
    PassRefPtr<DocumentFragmentImpl> cloneContents(ExceptionCode&);
    void insertNode(PassRefPtr<NodeImpl>, ExceptionCode&);
    String toString(ExceptionCode&) const;
    String toHTML() const;
    String text() const;

    PassRefPtr<DocumentFragmentImpl> createContextualFragment(const String& html, ExceptionCode&) const;

    void detach(ExceptionCode&);
    bool isDetached() const;
    PassRefPtr<RangeImpl> cloneRange(ExceptionCode&) const;

    void setStartAfter(NodeImpl*, ExceptionCode&);
    void setEndBefore(NodeImpl*, ExceptionCode&);
    void setEndAfter(NodeImpl*, ExceptionCode&);
    void selectNode(NodeImpl*, ExceptionCode&);
    void selectNodeContents(NodeImpl*, ExceptionCode&);
    void surroundContents(PassRefPtr<NodeImpl>, ExceptionCode&);
    void setStartBefore(NodeImpl*, ExceptionCode&);

    enum ActionType {
        DELETE_CONTENTS,
        EXTRACT_CONTENTS,
        CLONE_CONTENTS
    };
    PassRefPtr<DocumentFragmentImpl> processContents(ActionType, ExceptionCode&);

    Position startPosition() const;
    Position endPosition() const;

    NodeImpl* startNode() const;
    NodeImpl* pastEndNode() const;

    Position editingStartPosition() const;

#if !NDEBUG
    void formatForDebugger(char *buffer, unsigned length) const;
#endif

private:
    RefPtr<DocumentImpl> m_ownerDocument;
    RefPtr<NodeImpl> m_startContainer;
    unsigned m_startOffset;
    RefPtr<NodeImpl> m_endContainer;
    unsigned m_endOffset;
    bool m_detached;

    void checkNodeWOffset(NodeImpl*, int offset, ExceptionCode&) const;
    void checkNodeBA(NodeImpl*, ExceptionCode&) const;
    void checkDeleteExtract(ExceptionCode&);
    bool containedByReadOnly() const;
};

PassRefPtr<RangeImpl> rangeOfContents(NodeImpl*);

bool operator==(const RangeImpl&, const RangeImpl&);
inline bool operator!=(const RangeImpl& a, const RangeImpl& b) { return !(a == b); }

} // namespace

#endif
