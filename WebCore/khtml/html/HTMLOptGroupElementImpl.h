/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#ifndef HTML_HTMLOptGroupElementImpl_H
#define HTML_HTMLOptGroupElementImpl_H

#include "HTMLGenericFormElementImpl.h"

namespace DOM {

class HTMLOptGroupElementImpl : public HTMLGenericFormElementImpl
{
public:
    HTMLOptGroupElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f = 0);
    virtual ~HTMLOptGroupElementImpl();

    virtual bool checkDTD(const NodeImpl* newChild) { return newChild->hasTagName(HTMLNames::optionTag) || newChild->hasTagName(HTMLNames::hrTag); }

    DOMString type() const;

    virtual bool isFocusable() const;
    
    virtual NodeImpl *insertBefore ( NodeImpl *newChild, NodeImpl *refChild, int &exceptioncode );
    virtual NodeImpl *replaceChild ( NodeImpl *newChild, NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *removeChild ( NodeImpl *oldChild, int &exceptioncode );
    virtual NodeImpl *appendChild ( NodeImpl *newChild, int &exceptioncode );
    virtual NodeImpl *addChild( NodeImpl* newChild );
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);
    void recalcSelectOptions();

    DOMString label() const;
    void setLabel(const DOMString &);
};

} //namespace

#endif
