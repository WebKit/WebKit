/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#ifndef KDOM_TraversalImpl_H
#define KDOM_TraversalImpl_H

#include <kdom/Shared.h>
#include <kdom/traversal/kdomtraversal.h>

namespace KDOM
{
    class NodeImpl;
    class NodeFilterImpl;

    class TraversalImpl : public Shared<TraversalImpl>
    {
    public:
        TraversalImpl(NodeImpl *n, short show,
                      NodeFilterImpl *nodeFilter,
                      bool expandEntityReferences);
        virtual ~TraversalImpl();

        NodeImpl *root() const { return m_root; }
        unsigned long whatToShow() const { return m_whatToShow; }
        NodeFilterImpl *filter() const { return m_filter; }
        bool expandEntityReferences() const { return m_expandEntityReferences; }

        NodeImpl *findParentNode(NodeImpl *, short accept = FILTER_ACCEPT) const;
        NodeImpl *findFirstChild(NodeImpl *) const;
        NodeImpl *findLastChild(NodeImpl *) const;
        NodeImpl *findNextSibling(NodeImpl *) const;
        NodeImpl *findPreviousSibling(NodeImpl *) const;
        NodeImpl *findNextNode(NodeImpl *) const;
        NodeImpl *findLastDescendant(NodeImpl *node) const;
        NodeImpl *findPreviousNode(NodeImpl *) const;
        short acceptNode(NodeImpl *) const;

    private:
        TraversalImpl(const TraversalImpl &);
        TraversalImpl &operator=(const TraversalImpl &);

    private:
        NodeImpl *m_root;
        long m_whatToShow;
        NodeFilterImpl *m_filter;
        bool m_expandEntityReferences;
    };
};

#endif

// vim:ts=4:noet
