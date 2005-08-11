// -*- c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2005 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */


#ifndef GRAMMAR_TYPES
#define GRAMMAR_TYPES

namespace KJS {

    class Node;
    class Identifier;
    
    struct NodePair {
        Node *first;
        Node *second;
    };
    
    static inline NodePair makeNodePair(Node *f, Node *s) 
    {
        NodePair np;
        np.first = f;
        np.second = s;
        return np;
    }
    
    struct NodeWithIdent {
        Node *node;
        Identifier *ident;
    };

    static inline NodeWithIdent makeNodeWithIdent(Node *n, Identifier *i) 
    {
        NodeWithIdent ni;
        ni.node = n;
        ni.ident = i;
        return ni;
    }

} // namespace

#endif // GRAMMAR_TYPES
