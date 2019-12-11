/*
 *  This file is part of the WebKit open source project.
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
#include "WebKitDOMDocumentFragment.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/JSExecState.h>
#include "WebKitDOMDocumentFragmentPrivate.h"
#include "WebKitDOMElementPrivate.h"
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLCollectionPrivate.h"
#include "WebKitDOMNodeListPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include "WebKitDOMDocumentFragmentUnstable.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMDocumentFragment* kit(WebCore::DocumentFragment* obj)
{
    return WEBKIT_DOM_DOCUMENT_FRAGMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::DocumentFragment* core(WebKitDOMDocumentFragment* request)
{
    return request ? static_cast<WebCore::DocumentFragment*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMDocumentFragment* wrapDocumentFragment(WebCore::DocumentFragment* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_DOCUMENT_FRAGMENT(g_object_new(WEBKIT_DOM_TYPE_DOCUMENT_FRAGMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_document_fragment_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::DocumentFragment* coreTarget = static_cast<WebCore::DocumentFragment*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_document_fragment_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::DocumentFragment* coreTarget = static_cast<WebCore::DocumentFragment*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_document_fragment_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::DocumentFragment* coreTarget = static_cast<WebCore::DocumentFragment*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_document_fragment_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_document_fragment_dispatch_event;
    iface->add_event_listener = webkit_dom_document_fragment_add_event_listener;
    iface->remove_event_listener = webkit_dom_document_fragment_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMDocumentFragment, webkit_dom_document_fragment, WEBKIT_DOM_TYPE_NODE, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_document_fragment_dom_event_target_init))

enum {
    DOM_DOCUMENT_FRAGMENT_PROP_0,
    DOM_DOCUMENT_FRAGMENT_PROP_CHILDREN,
    DOM_DOCUMENT_FRAGMENT_PROP_FIRST_ELEMENT_CHILD,
    DOM_DOCUMENT_FRAGMENT_PROP_LAST_ELEMENT_CHILD,
    DOM_DOCUMENT_FRAGMENT_PROP_CHILD_ELEMENT_COUNT,
};

static void webkit_dom_document_fragment_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMDocumentFragment* self = WEBKIT_DOM_DOCUMENT_FRAGMENT(object);

    switch (propertyId) {
    case DOM_DOCUMENT_FRAGMENT_PROP_CHILDREN:
        g_value_set_object(value, webkit_dom_document_fragment_get_children(self));
        break;
    case DOM_DOCUMENT_FRAGMENT_PROP_FIRST_ELEMENT_CHILD:
        g_value_set_object(value, webkit_dom_document_fragment_get_first_element_child(self));
        break;
    case DOM_DOCUMENT_FRAGMENT_PROP_LAST_ELEMENT_CHILD:
        g_value_set_object(value, webkit_dom_document_fragment_get_last_element_child(self));
        break;
    case DOM_DOCUMENT_FRAGMENT_PROP_CHILD_ELEMENT_COUNT:
        g_value_set_ulong(value, webkit_dom_document_fragment_get_child_element_count(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_document_fragment_class_init(WebKitDOMDocumentFragmentClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->get_property = webkit_dom_document_fragment_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_FRAGMENT_PROP_CHILDREN,
        g_param_spec_object(
            "children",
            "DocumentFragment:children",
            "read-only WebKitDOMHTMLCollection* DocumentFragment:children",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_FRAGMENT_PROP_FIRST_ELEMENT_CHILD,
        g_param_spec_object(
            "first-element-child",
            "DocumentFragment:first-element-child",
            "read-only WebKitDOMElement* DocumentFragment:first-element-child",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_FRAGMENT_PROP_LAST_ELEMENT_CHILD,
        g_param_spec_object(
            "last-element-child",
            "DocumentFragment:last-element-child",
            "read-only WebKitDOMElement* DocumentFragment:last-element-child",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_FRAGMENT_PROP_CHILD_ELEMENT_COUNT,
        g_param_spec_ulong(
            "child-element-count",
            "DocumentFragment:child-element-count",
            "read-only gulong DocumentFragment:child-element-count",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_document_fragment_init(WebKitDOMDocumentFragment* request)
{
    UNUSED_PARAM(request);
}

WebKitDOMElement* webkit_dom_document_fragment_get_element_by_id(WebKitDOMDocumentFragment* self, const gchar* elementId)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT_FRAGMENT(self), 0);
    g_return_val_if_fail(elementId, 0);
    WebCore::DocumentFragment* item = WebKit::core(self);
    WTF::String convertedElementId = WTF::String::fromUTF8(elementId);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->getElementById(convertedElementId));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMElement* webkit_dom_document_fragment_query_selector(WebKitDOMDocumentFragment* self, const gchar* selectors, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT_FRAGMENT(self), 0);
    g_return_val_if_fail(selectors, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::DocumentFragment* item = WebKit::core(self);
    WTF::String convertedSelectors = WTF::String::fromUTF8(selectors);
    auto result = item->querySelector(convertedSelectors);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue());
}

WebKitDOMNodeList* webkit_dom_document_fragment_query_selector_all(WebKitDOMDocumentFragment* self, const gchar* selectors, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT_FRAGMENT(self), 0);
    g_return_val_if_fail(selectors, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::DocumentFragment* item = WebKit::core(self);
    WTF::String convertedSelectors = WTF::String::fromUTF8(selectors);
    auto result = item->querySelectorAll(convertedSelectors);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMHTMLCollection* webkit_dom_document_fragment_get_children(WebKitDOMDocumentFragment* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT_FRAGMENT(self), 0);
    WebCore::DocumentFragment* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->children());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMElement* webkit_dom_document_fragment_get_first_element_child(WebKitDOMDocumentFragment* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT_FRAGMENT(self), 0);
    WebCore::DocumentFragment* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->firstElementChild());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMElement* webkit_dom_document_fragment_get_last_element_child(WebKitDOMDocumentFragment* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT_FRAGMENT(self), 0);
    WebCore::DocumentFragment* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->lastElementChild());
    return WebKit::kit(gobjectResult.get());
}

gulong webkit_dom_document_fragment_get_child_element_count(WebKitDOMDocumentFragment* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT_FRAGMENT(self), 0);
    WebCore::DocumentFragment* item = WebKit::core(self);
    gulong result = item->childElementCount();
    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS;
