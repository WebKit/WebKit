/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "DocumentType.h"

#include "Document.h"
#include "NamedNodeMap.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DocumentType);

DocumentType::DocumentType(Document& document, const String& name, const String& publicId, const String& systemId)
    : Node(document, CreateOther)
    , m_name(name)
    , m_publicId(publicId.isNull() ? emptyString() : publicId)
    , m_systemId(systemId.isNull() ? emptyString() : systemId)
{
}

String DocumentType::nodeName() const
{
    return name();
}

Node::NodeType DocumentType::nodeType() const
{
    return DOCUMENT_TYPE_NODE;
}

Ref<Node> DocumentType::cloneNodeInternal(Document& documentTarget, CloningOperation)
{
    return create(documentTarget, m_name, m_publicId, m_systemId);
}

}
