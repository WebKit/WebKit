/**
 * This file is part of the DOM implementation for KDE.
 *
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
 */

#include <qdict.h>
#include "dom/dom_string.h"

namespace DOM
{    
struct XmlNamespaceEntry;

class XmlNamespaceTable
{
public:
    static int getNamespaceID(const DOMString& uri, bool readonly = true);
    static DOMString getNamespaceURI(int id);
    
    static QDict<XmlNamespaceEntry>* gNamespaceTable;
};

struct XmlNamespaceEntry
{
    XmlNamespaceEntry(int id, const DOMString& uri) :m_id(id), m_uri(uri) {}
    
    int m_id;
    DOMString m_uri;
};

}
