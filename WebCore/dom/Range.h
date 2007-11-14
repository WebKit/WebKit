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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef Range_h
#define Range_h

#include <wtf/RefCounted.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

typedef int ExceptionCode;

class DocumentFragment;
class Document;
class IntRect;
class Node;
class Position;
class String;

class Range : public RefCounted<Range>
{
public:
    Range(Document*);
    Range(Document*, Node* startContainer, int startOffset, Node* endContainer, int endOffset);
    Range(Document*, const Position&, const Position&);
    ~Range();

    Document* ownerDocument() const { return m_ownerDocument.get(); }

    Node* startContainer(ExceptionCode&) const;
    int startOffset(ExceptionCode&) const;
    Node* endContainer(ExceptionCode&) const;
    int endOffset(ExceptionCode&) const;
    bool collapsed(ExceptionCode&) const;

    Node* commonAncestorContainer(ExceptionCode&) const;
    static Node* commonAncestorContainer(Node* containerA, Node* containerB);
    void setStart(Node* container, int offset, ExceptionCode&);
    void setEnd(Node* container, int offset, ExceptionCode&);
    void collapse(bool toStart, ExceptionCode&);
    bool isPointInRange(Node* refNode, int offset, ExceptionCode& ec);
    short comparePoint(Node* refNode, int offset, ExceptionCode& ec);
    enum CompareResults { NODE_BEFORE, NODE_AFTER, NODE_BEFORE_AND_AFTER, NODE_INSIDE };
    CompareResults compareNode(Node* refNode, ExceptionCode&);
    enum CompareHow { START_TO_START, START_TO_END, END_TO_END, END_TO_START };
    short compareBoundaryPoints(CompareHow, const Range* sourceRange, ExceptionCode&) const;
    static short compareBoundaryPoints(Node* containerA, int offsetA, Node* containerB, int offsetB);
    static short compareBoundaryPoints(const Position&, const Position&);
    bool boundaryPointsValid() const;
    bool intersectsNode(Node* refNode, ExceptionCode&);
    void deleteContents(ExceptionCode&);
    PassRefPtr<DocumentFragment> extractContents(ExceptionCode&);
    PassRefPtr<DocumentFragment> cloneContents(ExceptionCode&);
    void insertNode(PassRefPtr<Node>, ExceptionCode&);
    String toString(ExceptionCode&) const;

    String toHTML() const;
    String text() const;

    PassRefPtr<DocumentFragment> createContextualFragment(const String& html, ExceptionCode&) const;

    void detach(ExceptionCode&);
    bool isDetached() const;
    PassRefPtr<Range> cloneRange(ExceptionCode&) const;

    void setStartAfter(Node*, ExceptionCode&);
    void setEndBefore(Node*, ExceptionCode&);
    void setEndAfter(Node*, ExceptionCode&);
    void selectNode(Node*, ExceptionCode&);
    void selectNodeContents(Node*, ExceptionCode&);
    void surroundContents(PassRefPtr<Node>, ExceptionCode&);
    void setStartBefore(Node*, ExceptionCode&);

    enum ActionType {
        DELETE_CONTENTS,
        EXTRACT_CONTENTS,
        CLONE_CONTENTS
    };
    PassRefPtr<DocumentFragment> processContents(ActionType, ExceptionCode&);

    Position startPosition() const;
    Position endPosition() const;

    Node* startNode() const;
    Node* pastEndNode() const;

    Position editingStartPosition() const;

    IntRect boundingBox();
    void addLineBoxRects(Vector<IntRect>&, bool useSelectionHeight = false);

#ifndef NDEBUG
    void formatForDebugger(char* buffer, unsigned length) const;
#endif

private:
    RefPtr<Document> m_ownerDocument;
    RefPtr<Node> m_startContainer;
    unsigned m_startOffset;
    RefPtr<Node> m_endContainer;
    unsigned m_endOffset;
    bool m_detached;

    void checkNodeWOffset(Node*, int offset, ExceptionCode&) const;
    void checkNodeBA(Node*, ExceptionCode&) const;
    void checkDeleteExtract(ExceptionCode&);
    bool containedByReadOnly() const;
};

PassRefPtr<Range> rangeOfContents(Node*);

bool operator==(const Range&, const Range&);
inline bool operator!=(const Range& a, const Range& b) { return !(a == b); }

} // namespace

#endif
