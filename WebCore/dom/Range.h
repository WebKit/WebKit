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

#include "Shared.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

typedef int ExceptionCode;

class DocumentFragment;
class Document;
class Node;
class Position;
class String;

const int RangeExceptionOffset = 200;
const int RangeExceptionMax = 299;
enum RangeExceptionCode { BAD_BOUNDARYPOINTS_ERR = RangeExceptionOffset + 1, INVALID_NODE_TYPE_ERR };

class Range : public Shared<Range>
{
public:
    Range(Document*);
    Range(Document*, Node* startContainer, int startOffset, Node* endContainer, int endOffset);
    Range(Document*, const Position&, const Position&);
    ~Range();

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
    enum CompareHow { START_TO_START, START_TO_END, END_TO_END, END_TO_START };
    short compareBoundaryPoints(CompareHow, const Range* sourceRange, ExceptionCode&) const;
    static short compareBoundaryPoints(Node* containerA, int offsetA, Node* containerB, int offsetB);
    static short compareBoundaryPoints(const Position&, const Position&);
    bool boundaryPointsValid() const;
    void deleteContents(ExceptionCode&);
    PassRefPtr<DocumentFragment> extractContents(ExceptionCode&);
    PassRefPtr<DocumentFragment> cloneContents(ExceptionCode&);
    void insertNode(PassRefPtr<Node>, ExceptionCode&);
    String toString(ExceptionCode&) const;
    String toString(bool convertBRsToNewlines, ExceptionCode&) const;

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

#if !NDEBUG
    void formatForDebugger(char *buffer, unsigned length) const;
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
