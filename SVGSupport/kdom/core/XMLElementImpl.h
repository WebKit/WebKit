/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Peter Kelly (pmk@post.com)
              (C) 2001 Dirk Mueller (mueller@kde.org)
              (C) 2003 Apple Computer, Inc.

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

#ifndef KDOM_XMLElementImpl_H
#define KDOM_XMLElementImpl_H

#include <kdom/impl/ElementImpl.h>

namespace KDOM
{
    class XMLElementImpl : public ElementImpl
    {
    public:
        XMLElementImpl(DocumentPtr *doc, NodeImpl::Id id);
        XMLElementImpl(DocumentPtr *doc, NodeImpl::Id id, DOMStringImpl *prefix, bool nullNSSpecified = false);
        virtual ~XMLElementImpl();

        virtual DOMStringImpl *localName() const;
        virtual DOMStringImpl *tagName() const;

        virtual NodeImpl::Id id() const { return m_id; }

    protected:
        NodeImpl::Id m_id;
    };
};

#endif

// vim:ts=4:noet
