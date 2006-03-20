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
#include "CDATASection.h"

#include "Document.h"

namespace WebCore {

CDATASection::CDATASection(Document *impl, const String &_text) : Text(impl,_text)
{
}

CDATASection::CDATASection(Document *impl) : Text(impl)
{
}

CDATASection::~CDATASection()
{
}

String CDATASection::nodeName() const
{
  return "#cdata-section";
}

Node::NodeType CDATASection::nodeType() const
{
    return CDATA_SECTION_NODE;
}

PassRefPtr<Node> CDATASection::cloneNode(bool /*deep*/)
{
    ExceptionCode ec = 0;
    return getDocument()->createCDATASection(str, ec);
}

// DOM Section 1.1.1
bool CDATASection::childTypeAllowed(NodeType)
{
    return false;
}

Text *CDATASection::createNew(StringImpl *_str)
{
    return new CDATASection(getDocument(), _str);
}

String CDATASection::toString() const
{
    // FIXME: substitute entity references as needed!
    return String("<![CDATA[") + nodeValue() + "]]>";
}

} // namespace WebCore
