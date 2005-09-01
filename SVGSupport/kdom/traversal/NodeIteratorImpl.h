/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
              (C) 2001 Peter Kelly (pmk@post.com)
          
    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_NodeIteratorImpl_H
#define KDOM_NodeIteratorImpl_H

#include <kdom/traversal/TraversalImpl.h>

namespace KDOM
{
    class DocumentImpl;

    class NodeIteratorImpl : public TraversalImpl
    {
    public:
        NodeIteratorImpl(NodeImpl *n, short show,
                         NodeFilterImpl *filter, bool expansion);
        virtual ~NodeIteratorImpl();

        NodeImpl *nextNode();
        NodeImpl *previousNode();
        void detach();

        NodeImpl *referenceNode() const { return m_referenceNode; }
        bool pointerBeforeReferenceNode() const { return m_beforeReferenceNode; }

        /**
         * This function has to be called if you delete a node from the
         * document tree and you want the Iterator to react if there
         * are any changes concerning it.
         */
        void notifyBeforeNodeRemoval(NodeImpl *removed);

    private:
        NodeIteratorImpl(const NodeIteratorImpl &);
        NodeIteratorImpl &operator=(const NodeIteratorImpl &);

        void setReferenceNode(NodeImpl *);
        void setPointerBeforeReferenceNode(bool flag = true)
        {
            m_beforeReferenceNode = flag;
        }

        bool detached() const { return m_detached; }
        void setDetached(bool flag = true) { m_detached = flag; }
        DocumentImpl *document() const { return m_doc; }
        void setDocument(DocumentImpl *);

    private:
        NodeImpl *m_referenceNode;
        bool m_beforeReferenceNode;
        bool m_detached;
        DocumentImpl *m_doc;
    };
};

#endif

// vim:ts=4:noet
