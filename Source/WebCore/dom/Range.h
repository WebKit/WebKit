/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "RangeBoundaryPoint.h"

namespace WebCore {

class DOMRect;
class DOMRectList;
class DocumentFragment;
class NodeWithIndex;
class Text;

struct SimpleRange;

class Range : public RefCounted<Range> {
public:
    WEBCORE_EXPORT static Ref<Range> create(Document&);
    WEBCORE_EXPORT ~Range();

    Node& startContainer() const { return m_start.container(); }
    unsigned startOffset() const { return m_start.offset(); }
    Node& endContainer() const { return m_end.container(); }
    unsigned endOffset() const { return m_end.offset(); }
    bool collapsed() const { return m_start == m_end; }
    Node* commonAncestorContainer() const { return commonInclusiveAncestor(startContainer(), endContainer()).get(); }

    WEBCORE_EXPORT ExceptionOr<void> setStart(Ref<Node>&&, unsigned offset);
    WEBCORE_EXPORT ExceptionOr<void> setEnd(Ref<Node>&&, unsigned offset);
    WEBCORE_EXPORT ExceptionOr<void> setStartBefore(Node&);
    WEBCORE_EXPORT ExceptionOr<void> setStartAfter(Node&);
    WEBCORE_EXPORT ExceptionOr<void> setEndBefore(Node&);
    WEBCORE_EXPORT ExceptionOr<void> setEndAfter(Node&);
    WEBCORE_EXPORT void collapse(bool toStart);
    WEBCORE_EXPORT ExceptionOr<void> selectNode(Node&);
    WEBCORE_EXPORT ExceptionOr<void> selectNodeContents(Node&);

    enum CompareHow : unsigned short { START_TO_START, START_TO_END, END_TO_END, END_TO_START };
    WEBCORE_EXPORT ExceptionOr<short> compareBoundaryPoints(unsigned short compareHow, const Range& sourceRange) const;

    WEBCORE_EXPORT ExceptionOr<void> deleteContents();
    WEBCORE_EXPORT ExceptionOr<Ref<DocumentFragment>> extractContents();
    WEBCORE_EXPORT ExceptionOr<Ref<DocumentFragment>> cloneContents();
    WEBCORE_EXPORT ExceptionOr<void> insertNode(Ref<Node>&&);
    WEBCORE_EXPORT ExceptionOr<void> surroundContents(Node&);

    WEBCORE_EXPORT Ref<Range> cloneRange() const;
    static void detach() { }

    WEBCORE_EXPORT ExceptionOr<bool> isPointInRange(Node&, unsigned offset);
    WEBCORE_EXPORT ExceptionOr<short> comparePoint(Node&, unsigned offset) const;
    WEBCORE_EXPORT bool intersectsNode(Node&) const;

    WEBCORE_EXPORT String toString() const;

    Ref<DOMRectList> getClientRects() const;
    Ref<DOMRect> getBoundingClientRect() const;

    WEBCORE_EXPORT ExceptionOr<Ref<DocumentFragment>> createContextualFragment(const String& fragment);

    // Expand range to a unit (word or sentence or block or document) boundary.
    // Please refer to https://bugs.webkit.org/show_bug.cgi?id=27632 comment #5 for details.
    WEBCORE_EXPORT ExceptionOr<void> expand(const String&);

    enum CompareResults : uint8_t { NODE_BEFORE, NODE_AFTER, NODE_BEFORE_AND_AFTER, NODE_INSIDE };
    WEBCORE_EXPORT ExceptionOr<CompareResults> compareNode(Node&) const;

    void nodeChildrenChanged(ContainerNode&);
    void nodeChildrenWillBeRemoved(ContainerNode&);
    void nodeWillBeRemoved(Node&);
    bool parentlessNodeMovedToNewDocumentAffectsRange(Node&);
    void updateRangeForParentlessNodeMovedToNewDocument(Node&);

    void textInserted(Node&, unsigned offset, unsigned length);
    void textRemoved(Node&, unsigned offset, unsigned length);
    void textNodesMerged(NodeWithIndex& oldNode, unsigned offset);
    void textNodeSplit(Text& oldNode);

    void didAssociateWithSelection() { m_isAssociatedWithSelection = true; }
    void didDisassociateFromSelection() { m_isAssociatedWithSelection = false; }
    void updateFromSelection(const SimpleRange&);

    static ExceptionOr<Node*> checkNodeOffsetPair(Node&, unsigned offset);

#if ENABLE(TREE_DEBUGGING)
    String debugDescription() const;
#endif

    enum ActionType : uint8_t { Delete, Extract, Clone };

private:
    explicit Range(Document&);

    void updateDocument();
    void updateAssociatedSelection();
    ExceptionOr<RefPtr<DocumentFragment>> processContents(ActionType);

    Ref<Document> m_ownerDocument;
    RangeBoundaryPoint m_start;
    RangeBoundaryPoint m_end;
    bool m_isAssociatedWithSelection { false };
};

WEBCORE_EXPORT SimpleRange makeSimpleRange(const Range&);
WEBCORE_EXPORT SimpleRange makeSimpleRange(const Ref<Range>&);
WEBCORE_EXPORT Optional<SimpleRange> makeSimpleRange(const Range*);
WEBCORE_EXPORT Optional<SimpleRange> makeSimpleRange(const RefPtr<Range>&);

WEBCORE_EXPORT Ref<Range> createLiveRange(const SimpleRange&);
WEBCORE_EXPORT RefPtr<Range> createLiveRange(const Optional<SimpleRange>&);

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from the debugger.
void showTree(const WebCore::Range*);
#endif
