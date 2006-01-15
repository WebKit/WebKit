/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
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
#ifndef DOM_NameNodeListImpl_h
#define DOM_NameNodeListImpl_h

#include "NodeListImpl.h"
#include "dom_string.h"

namespace DOM {

/**
 * NodeList which lists all Nodes in a Element with a given "name=" tag
 */
class NameNodeListImpl : public NodeListImpl
{
public:
    NameNodeListImpl( NodeImpl *doc, const DOMString &t );

    // DOM methods overridden from  parent classes

    virtual unsigned length() const;
    virtual NodeImpl *item ( unsigned index ) const;

    // Other methods (not part of DOM)
    virtual void rootNodeChildrenChanged() {};
    virtual void rootNodeAttributeChanged() { NodeListImpl::rootNodeChildrenChanged(); }

protected:
    virtual bool nodeMatches( NodeImpl *testNode ) const;

    DOMString nodeName;
};

}; //namespace
#endif
