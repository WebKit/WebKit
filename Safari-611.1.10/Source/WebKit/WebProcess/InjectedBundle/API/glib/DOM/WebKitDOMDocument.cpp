/*
 *  Copyright (C) 2018 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitDOMDocument.h"

#include "DOMObjectCache.h"
#include "WebKitDOMDocumentPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"

#if PLATFORM(GTK)
#include "WebKitDOMEventTarget.h"
#endif

namespace WebKit {

WebKitDOMDocument* kit(WebCore::Document* obj)
{
    return WEBKIT_DOM_DOCUMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::Document* core(WebKitDOMDocument* document)
{
    return document ? static_cast<WebCore::Document*>(webkitDOMNodeGetCoreObject(WEBKIT_DOM_NODE(document))) : nullptr;
}

WebKitDOMDocument* wrapDocument(WebCore::Document* coreObject)
{
    ASSERT(coreObject);
#if PLATFORM(GTK)
    return WEBKIT_DOM_DOCUMENT(g_object_new(WEBKIT_DOM_TYPE_DOCUMENT, "core-object", coreObject, nullptr));
#else
    auto* document = WEBKIT_DOM_DOCUMENT(g_object_new(WEBKIT_DOM_TYPE_DOCUMENT, nullptr));
    webkitDOMNodeSetCoreObject(WEBKIT_DOM_NODE(document), static_cast<WebCore::Node*>(coreObject));
    return document;
#endif
}

} // namespace WebKit

#if PLATFORM(GTK)
G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
G_DEFINE_TYPE_WITH_CODE(WebKitDOMDocument, webkit_dom_document, WEBKIT_DOM_TYPE_NODE, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkitDOMDocumentDOMEventTargetInit))
G_GNUC_END_IGNORE_DEPRECATIONS;
#else
G_DEFINE_TYPE(WebKitDOMDocument, webkit_dom_document, WEBKIT_DOM_TYPE_NODE)
#endif

static void webkit_dom_document_class_init(WebKitDOMDocumentClass* documentClass)
{
#if PLATFORM(GTK)
    GObjectClass* gobjectClass = G_OBJECT_CLASS(documentClass);
    webkitDOMDocumentInstallProperties(gobjectClass);
#endif
}

static void webkit_dom_document_init(WebKitDOMDocument*)
{
}
