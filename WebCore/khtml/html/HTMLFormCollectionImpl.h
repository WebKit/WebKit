/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
#ifndef HTMLFormCollectionImpl_H
#define HTMLFormCollectionImpl_H

#include "HTMLCollectionImpl.h"

namespace WebCore {

class QualifiedName;

// this whole class is just a big hack to find form elements even in
// malformed HTML elements
// the famous <table><tr><form><td> problem
class HTMLFormCollectionImpl : public HTMLCollectionImpl
{
public:
    // base must inherit HTMLGenericFormElementImpl or this won't work
    HTMLFormCollectionImpl(NodeImpl* _base);
    ~HTMLFormCollectionImpl();

    virtual NodeImpl *item ( unsigned index ) const;
    virtual NodeImpl *firstItem() const;
    virtual NodeImpl *nextItem() const;

    virtual NodeImpl *namedItem ( const DOMString &name, bool caseSensitive = true ) const;
    virtual NodeImpl *nextNamedItem( const DOMString &name ) const;

protected:
    virtual void updateNameCache() const;
    virtual unsigned calcLength() const;
    virtual NodeImpl *getNamedItem(NodeImpl* current, const QualifiedName& attrName, const DOMString& name, bool caseSensitive) const;
    virtual NodeImpl *nextNamedItemInternal( const DOMString &name ) const;
private:
    NodeImpl* getNamedFormItem(const QualifiedName& attrName, const DOMString& name, int duplicateNumber, bool caseSensitive) const;
    mutable int currentPos;
};

} //namespace

#endif
