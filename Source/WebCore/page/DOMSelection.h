/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2012 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef DOMSelection_h
#define DOMSelection_h

#include "DOMWindowProperty.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class Frame;
    class Node;
    class Position;
    class Range;
    class TreeScope;
    class VisibleSelection;

    typedef int ExceptionCode;

    class DOMSelection : public RefCounted<DOMSelection>, public DOMWindowProperty {
    public:
        static Ref<DOMSelection> create(const TreeScope* treeScope) { return adoptRef(*new DOMSelection(treeScope)); }

        void clearTreeScope();

        // Safari Selection Object API
        // These methods return the valid equivalents of internal editing positions.
        Node* baseNode() const;
        Node* extentNode() const;
        unsigned baseOffset() const;
        unsigned extentOffset() const;
        String type() const;
        void setBaseAndExtent(Node* baseNode, unsigned baseOffset, Node* extentNode, unsigned extentOffset);
        void setPosition(Node*, unsigned offset);
        void modify(const String& alter, const String& direction, const String& granularity);

        // Mozilla Selection Object API
        // In Firefox, anchor/focus are the equal to the start/end of the selection,
        // but reflect the direction in which the selection was made by the user.  That does
        // not mean that they are base/extent, since the base/extent don't reflect
        // expansion.
        // These methods return the valid equivalents of internal editing positions.
        Node* anchorNode() const;
        unsigned anchorOffset() const;
        Node* focusNode() const;
        unsigned focusOffset() const;
        bool isCollapsed() const;
        unsigned rangeCount() const;
        void collapse(Node*, unsigned offset);
        void collapseToEnd(ExceptionCode&);
        void collapseToStart(ExceptionCode&);
        void extend(Node&, unsigned offset, ExceptionCode&);
        RefPtr<Range> getRangeAt(unsigned, ExceptionCode&);
        void removeAllRanges();
        void addRange(Range&);
        void deleteFromDocument();
        bool containsNode(Node&, bool partlyContained) const;
        void selectAllChildren(Node&);

        String toString();

        // Microsoft Selection Object API
        void empty();

    private:
        const TreeScope* m_treeScope;

        explicit DOMSelection(const TreeScope*);

        // Convenience method for accessors, does not NULL check m_frame.
        const VisibleSelection& visibleSelection() const;

        Node* shadowAdjustedNode(const Position&) const;
        unsigned shadowAdjustedOffset(const Position&) const;

        bool isValidForPosition(Node*) const;
    };

} // namespace WebCore

#endif // DOMSelection_h
