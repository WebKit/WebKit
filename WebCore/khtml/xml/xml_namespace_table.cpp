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

#include "xml_namespace_table.h"
#include "dom_nodeimpl.h"

namespace DOM
{

QDict<XmlNamespaceEntry>* XmlNamespaceTable::gNamespaceTable = 0;

int XmlNamespaceTable::getNamespaceID(const DOMString& uri, bool readonly)
{
    if (uri == XHTML_NAMESPACE)
        return xhtmlNamespace;
    
    if (uri.isEmpty())
        return noNamespace;
    
    QString uriStr = uri.string();

    if (!gNamespaceTable) {
        gNamespaceTable = new QDict<XmlNamespaceEntry>;
        gNamespaceTable->insert(XHTML_NAMESPACE, new XmlNamespaceEntry(xhtmlNamespace, XHTML_NAMESPACE));
    }
    
    XmlNamespaceEntry* ns = gNamespaceTable->find(uriStr);
    if (ns) return ns->m_id;
    
    if (!readonly) {
        static int id = xhtmlNamespace+1;
        ns = new XmlNamespaceEntry(id++, uri);
        gNamespaceTable->insert(uriStr, ns);
        return ns->m_id;
    }
    
    return -1;
}

DOMString XmlNamespaceTable::getNamespaceURI(int id)
{
    if (id == noNamespace || id == anyNamespace || !gNamespaceTable)
        return "";

    if (!gNamespaceTable) {
        gNamespaceTable = new QDict<XmlNamespaceEntry>;
        gNamespaceTable->insert(XHTML_NAMESPACE, new XmlNamespaceEntry(xhtmlNamespace, XHTML_NAMESPACE));
    }
    
    // Have to iterate over the dictionary entries looking for a match.
    // (This should be ok, since this is not the common direction.)
    QDictIterator<XmlNamespaceEntry> it(*gNamespaceTable);
    for (unsigned i = 0; i < it.count(); i++, ++it) {
        if (it.current()->m_id == id)
            return it.current()->m_uri;
    }
    
    return "";
}

}
