/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2008 Luke Kenneth Casson Leighton <lkcl@lkcl.net>
 *  Copyright (C) 2008 Martin Soto <soto@freedesktop.org>
 *  Copyright (C) 2009-2013 Igalia S.L.
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

#include <WebCore/Blob.h>
#include "DOMObjectCache.h"
#include <WebCore/Element.h>
#include <WebCore/Event.h>
#include <WebCore/EventTarget.h>
#include <WebCore/File.h>
#include <WebCore/HTMLElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/StyleSheet.h>
#include <WebCore/UIEvent.h>
#include "WebKitDOMAttrPrivate.h"
#include "WebKitDOMBlobPrivate.h"
#include "WebKitDOMCDATASectionPrivate.h"
#include "WebKitDOMCSSStyleSheetPrivate.h"
#include "WebKitDOMCommentPrivate.h"
#include "WebKitDOMDOMWindowPrivate.h"
#include "WebKitDOMDocumentFragmentPrivate.h"
#include "WebKitDOMDocumentPrivate.h"
#include "WebKitDOMDocumentTypePrivate.h"
#include "WebKitDOMElementPrivate.h"
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTargetPrivate.h"
#include "WebKitDOMFilePrivate.h"
#include "WebKitDOMHTMLCollectionPrivate.h"
#include "WebKitDOMHTMLDocumentPrivate.h"
#include "WebKitDOMHTMLOptionsCollectionPrivate.h"
#include "WebKitDOMHTMLPrivate.h"
#include "WebKitDOMKeyboardEventPrivate.h"
#include "WebKitDOMMouseEventPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMProcessingInstructionPrivate.h"
#include "WebKitDOMStyleSheetPrivate.h"
#include "WebKitDOMTextPrivate.h"
#include "WebKitDOMUIEventPrivate.h"
#include "WebKitDOMWheelEventPrivate.h"

namespace WebKit {

using namespace WebCore;
using namespace WebCore::HTMLNames;

WebKitDOMNode* wrap(Node* node)
{
    ASSERT(node);
    ASSERT(node->nodeType());

    switch (node->nodeType()) {
    case Node::ELEMENT_NODE:
        if (is<HTMLElement>(*node))
            return WEBKIT_DOM_NODE(wrap(downcast<HTMLElement>(node)));
        return WEBKIT_DOM_NODE(wrapElement(downcast<Element>(node)));
    case Node::ATTRIBUTE_NODE:
        return WEBKIT_DOM_NODE(wrapAttr(static_cast<Attr*>(node)));
    case Node::TEXT_NODE:
        return WEBKIT_DOM_NODE(wrapText(downcast<Text>(node)));
    case Node::CDATA_SECTION_NODE:
        return WEBKIT_DOM_NODE(wrapCDATASection(static_cast<CDATASection*>(node)));
    case Node::PROCESSING_INSTRUCTION_NODE:
        return WEBKIT_DOM_NODE(wrapProcessingInstruction(static_cast<ProcessingInstruction*>(node)));
    case Node::COMMENT_NODE:
        return WEBKIT_DOM_NODE(wrapComment(static_cast<Comment*>(node)));
    case Node::DOCUMENT_NODE:
        if (is<HTMLDocument>(*node))
            return WEBKIT_DOM_NODE(wrapHTMLDocument(downcast<HTMLDocument>(node)));
        return WEBKIT_DOM_NODE(wrapDocument(downcast<Document>(node)));
    case Node::DOCUMENT_TYPE_NODE:
        return WEBKIT_DOM_NODE(wrapDocumentType(static_cast<DocumentType*>(node)));
    case Node::DOCUMENT_FRAGMENT_NODE:
        return WEBKIT_DOM_NODE(wrapDocumentFragment(static_cast<DocumentFragment*>(node)));
    }

    return wrapNode(node);
}

WebKitDOMEvent* wrap(Event* event)
{
    ASSERT(event);

    if (event->isUIEvent()) {
        if (event->isMouseEvent())
            return WEBKIT_DOM_EVENT(wrapMouseEvent(static_cast<MouseEvent*>(event)));

        if (event->isKeyboardEvent())
            return WEBKIT_DOM_EVENT(wrapKeyboardEvent(static_cast<KeyboardEvent*>(event)));

        if (event->eventInterface() == WheelEventInterfaceType)
            return WEBKIT_DOM_EVENT(wrapWheelEvent(static_cast<WheelEvent*>(event)));

        return WEBKIT_DOM_EVENT(wrapUIEvent(static_cast<UIEvent*>(event)));
    }

    return wrapEvent(event);
}

WebKitDOMStyleSheet* wrap(StyleSheet* styleSheet)
{
    ASSERT(styleSheet);

    if (is<CSSStyleSheet>(*styleSheet))
        return WEBKIT_DOM_STYLE_SHEET(wrapCSSStyleSheet(downcast<CSSStyleSheet>(styleSheet)));
    return wrapStyleSheet(styleSheet);
}

WebKitDOMHTMLCollection* wrap(HTMLCollection* collection)
{
    ASSERT(collection);

    if (is<HTMLOptionsCollection>(*collection))
        return WEBKIT_DOM_HTML_COLLECTION(wrapHTMLOptionsCollection(downcast<HTMLOptionsCollection>(collection)));
    return wrapHTMLCollection(collection);
}

WebKitDOMEventTarget* wrap(EventTarget* eventTarget)
{
    ASSERT(eventTarget);

    if (Node* node = eventTarget->toNode())
        return WEBKIT_DOM_EVENT_TARGET(kit(node));

    if (DOMWindow* window = eventTarget->toDOMWindow())
        return WEBKIT_DOM_EVENT_TARGET(kit(window));

    return 0;
}

WebKitDOMBlob* wrap(Blob* blob)
{
    ASSERT(blob);

    if (blob->isFile())
        return WEBKIT_DOM_BLOB(wrapFile(static_cast<File*>(blob)));
    return wrapBlob(blob);
}

} // namespace WebKit
