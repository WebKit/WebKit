/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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

#ifndef NodeIterator_h
#define NodeIterator_h

#include "Traversal.h"

namespace WebCore {

    class Document;

    typedef int ExceptionCode;

    class NodeIterator : public Traversal {
    public:
        NodeIterator(Node*, unsigned whatToShow, PassRefPtr<NodeFilter>, bool expandEntityReferences);
        ~NodeIterator();

        Node* nextNode(ExceptionCode&);
        Node* previousNode(ExceptionCode&);
        void detach(ExceptionCode&);

        Node* referenceNode() const { return m_referenceNode.get(); }
        bool pointerBeforeReferenceNode() const { return m_beforeReferenceNode; }

        /**
         * This function has to be called if you delete a node from the
         * document tree and you want the Iterator to react if there
         * are any changes concerning it.
         */
        void notifyBeforeNodeRemoval(Node* removed);

    private:
        void setReferenceNode(Node*);
        void setPointerBeforeReferenceNode(bool flag = true) { m_beforeReferenceNode = flag; }
        bool detached() const { return m_detached; }
        void setDetached(bool flag = true) { m_detached = flag; }
        Document* document() const { return m_doc.get(); }
        Node* findNextNode(Node*) const;
        Node* findPreviousNode(Node*) const;

        RefPtr<Node> m_referenceNode;
        bool m_beforeReferenceNode;
        bool m_detached;
        RefPtr<Document> m_doc;
    };

} // namespace WebCore

#endif // NodeIterator_h
