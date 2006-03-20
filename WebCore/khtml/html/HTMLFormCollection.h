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

#include "HTMLCollection.h"

namespace WebCore {

class QualifiedName;

// this whole class is just a big hack to find form elements even in
// malformed HTML elements
// the famous <table><tr><form><td> problem
class HTMLFormCollection : public HTMLCollection
{
public:
    // base must inherit HTMLGenericFormElement or this won't work
    HTMLFormCollection(Node* _base);
    ~HTMLFormCollection();

    virtual Node *item ( unsigned index ) const;
    virtual Node *firstItem() const;
    virtual Node *nextItem() const;

    virtual Node *namedItem ( const String &name, bool caseSensitive = true ) const;
    virtual Node *nextNamedItem( const String &name ) const;

protected:
    virtual void updateNameCache() const;
    virtual unsigned calcLength() const;
    virtual Node *getNamedItem(Node* current, const QualifiedName& attrName, const String& name, bool caseSensitive) const;
    virtual Node *nextNamedItemInternal( const String &name ) const;
private:
    Node* getNamedFormItem(const QualifiedName& attrName, const String& name, int duplicateNumber, bool caseSensitive) const;
    mutable int currentPos;
};

} //namespace

#endif
