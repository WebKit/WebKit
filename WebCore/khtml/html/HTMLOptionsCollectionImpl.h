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
#ifndef HTML_HTMLOptionsCollectionImpl_H
#define HTML_HTMLOptionsCollectionImpl_H

#include "shared.h"

namespace DOM {

class NodeImpl;
class HTMLSelectElementImpl;

class HTMLOptionsCollectionImpl : public khtml::Shared<HTMLOptionsCollectionImpl>
{
public:
    HTMLOptionsCollectionImpl(HTMLSelectElementImpl *impl) : m_select(impl) { }

    unsigned length() const { return 0; }
    void setLength(unsigned) { }
    NodeImpl *item(unsigned index) const { return 0; }
    NodeImpl *namedItem(const DOMString &name) const { return 0; }

    void detach() { m_select = 0; }

private:
    HTMLSelectElementImpl *m_select;
};

} //namespace

#endif
