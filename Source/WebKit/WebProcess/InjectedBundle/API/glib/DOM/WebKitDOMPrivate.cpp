/*
 *  Copyright (C) 2018 Igalia S.L.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WebKitDOMPrivate.h"

#include "WebKitDOMDocumentPrivate.h"
#include "WebKitDOMElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include <WebCore/HTMLFormElement.h>

#if PLATFORM(GTK)
#include "WebKitDOMPrivateGtk.h"
#endif

namespace WebKit {

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

WebKitDOMNode* wrap(WebCore::Node* node)
{
    ASSERT(node);
    ASSERT(node->nodeType());

#if PLATFORM(GTK)
    if (auto* wrapper = wrapNodeGtk(node))
        return wrapper;
#endif

    switch (node->nodeType()) {
    case WebCore::Node::ELEMENT_NODE:
        return WEBKIT_DOM_NODE(wrapElement(downcast<WebCore::Element>(node)));
    case WebCore::Node::DOCUMENT_NODE:
        return WEBKIT_DOM_NODE(wrapDocument(downcast<WebCore::Document>(node)));
    case WebCore::Node::ATTRIBUTE_NODE:
    case WebCore::Node::TEXT_NODE:
    case WebCore::Node::CDATA_SECTION_NODE:
    case WebCore::Node::PROCESSING_INSTRUCTION_NODE:
    case WebCore::Node::COMMENT_NODE:
    case WebCore::Node::DOCUMENT_TYPE_NODE:
    case WebCore::Node::DOCUMENT_FRAGMENT_NODE:
        break;
    }

    return wrapNode(node);
}

G_GNUC_END_IGNORE_DEPRECATIONS;

} // namespace WebKit
