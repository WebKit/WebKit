/**
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
 */

#include "config.h"
#include "CDATASectionImpl.h"

#include "DocumentImpl.h"

namespace WebCore {

CDATASectionImpl::CDATASectionImpl(DocumentImpl *impl, const DOMString &_text) : TextImpl(impl,_text)
{
}

CDATASectionImpl::CDATASectionImpl(DocumentImpl *impl) : TextImpl(impl)
{
}

CDATASectionImpl::~CDATASectionImpl()
{
}

DOMString CDATASectionImpl::nodeName() const
{
  return "#cdata-section";
}

NodeImpl::NodeType CDATASectionImpl::nodeType() const
{
    return CDATA_SECTION_NODE;
}

PassRefPtr<NodeImpl> CDATASectionImpl::cloneNode(bool /*deep*/)
{
    ExceptionCode ec = 0;
    return getDocument()->createCDATASection(str, ec);
}

// DOM Section 1.1.1
bool CDATASectionImpl::childTypeAllowed(NodeType)
{
    return false;
}

TextImpl *CDATASectionImpl::createNew(DOMStringImpl *_str)
{
    return new CDATASectionImpl(getDocument(), _str);
}

DOMString CDATASectionImpl::toString() const
{
    // FIXME: substitute entity references as needed!
    return DOMString("<![CDATA[") + nodeValue() + "]]>";
}

} // namespace WebCore
