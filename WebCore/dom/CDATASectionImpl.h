/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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

#ifndef DOM_CDATASectionImpl_h
#define DOM_CDATASectionImpl_h

#include "TextImpl.h"

namespace WebCore {

class CDATASectionImpl : public TextImpl
{
// ### should these have id==ID_TEXT
public:
    CDATASectionImpl(DocumentImpl *impl, const DOMString &_text);
    CDATASectionImpl(DocumentImpl *impl);
    virtual ~CDATASectionImpl();

    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual PassRefPtr<NodeImpl> cloneNode(bool deep);
    virtual bool childTypeAllowed( unsigned short type );
    virtual DOMString toString() const;

protected:
    virtual TextImpl* createNew(DOMStringImpl*);
};

} // namespace WebCore

#endif // DOM_CDATASectionImpl_h
