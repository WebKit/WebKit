/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003, 2006 Apple Computer, Inc.
 *  Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "XMLSerializer.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "markup.h"

namespace WebCore {

String XMLSerializer::serializeToString(Node* node, ExceptionCode& ec)
{
    if (!node)
        return String();

    if (!node->document()) {
        // Due to the fact that DocumentType nodes are created by the DOMImplementation
        // and not the Document, it is possible for it to not have a Document associated
        // with it.  It should be the only type of node where this is possible.
        ASSERT(node->nodeType() == Node::DOCUMENT_TYPE_NODE);

        ec = INVALID_ACCESS_ERR;
        return String();
    }

    return createMarkup(node);
}

} // namespace WebCore
