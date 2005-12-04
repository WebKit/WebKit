/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
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
 * This file includes excerpts from the Document Object Model (DOM)
 * Level 2 Specification (Candidate Recommendation)
 * http://www.w3.org/TR/2000/CR-DOM-Level-2-20000510/
 * Copyright © 2000 W3C® (MIT, INRIA, Keio), All Rights Reserved.
 *
 */
#ifndef _dom2_traversal_h_
#define _dom2_traversal_h_

#include <dom/dom_misc.h>

namespace DOM {

class NodeImpl;
class NodeFilterImpl;
class NodeIteratorImpl;
class TreeWalkerImpl;

typedef NodeImpl *FilterNode;


class NodeFilterCondition : public DomShared
{
public:
    virtual short acceptNode(FilterNode) const;
};

/**
 * Filters are objects that know how to "filter out" nodes. If an
 * Iterator or TreeWalker is given a filter, before it
 * returns the next node, it applies the filter. If the filter says to
 * accept the node, the Iterator returns it; otherwise, the Iterator
 * looks for the next node and pretends that the node that was
 * rejected was not there.
 *
 *  The DOM does not provide any filters. Filter is just an interface
 * that users can implement to provide their own filters.
 *
 *  Filters do not need to know how to iterate, nor do they need to
 * know anything about the data structure that is being iterated. This
 * makes it very easy to write filters, since the only thing they have
 * to know how to do is evaluate a single node. One filter may be used
 * with a number of different kinds of Iterators, encouraging code
 * reuse.
 *
 * To create your own custom NodeFilter, define a subclass of
 * CustomNodeFilter which overrides the acceptNode() method and assign
 * an instance of it to the NodeFilter. For more details see the
 * CustomNodeFilter class
 */
class NodeFilter
{
public:

    /**
     * The following constants are returned by the acceptNode()
     * method:
     *
     */
    enum {
        FILTER_ACCEPT = 1,
        FILTER_REJECT = 2,
        FILTER_SKIP   = 3
    };

    /**
     * These are the available values for the whatToShow parameter.
     * They are the same as the set of possible types for Node, and
     * their values are derived by using a bit position corresponding
     * to the value of NodeType for the equivalent node type.
     *
     */
    enum {
        SHOW_ALL                       = 0xFFFFFFFF,
        SHOW_ELEMENT                   = 0x00000001,
        SHOW_ATTRIBUTE                 = 0x00000002,
        SHOW_TEXT                      = 0x00000004,
        SHOW_CDATA_SECTION             = 0x00000008,
        SHOW_ENTITY_REFERENCE          = 0x00000010,
        SHOW_ENTITY                    = 0x00000020,
        SHOW_PROCESSING_INSTRUCTION    = 0x00000040,
        SHOW_COMMENT                   = 0x00000080,
        SHOW_DOCUMENT                  = 0x00000100,
        SHOW_DOCUMENT_TYPE             = 0x00000200,
        SHOW_DOCUMENT_FRAGMENT         = 0x00000400,
        SHOW_NOTATION                  = 0x00000800
    };
};
} // namespace

#endif
