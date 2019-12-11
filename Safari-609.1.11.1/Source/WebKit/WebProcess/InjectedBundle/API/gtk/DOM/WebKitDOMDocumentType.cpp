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
#include "WebKitDOMDocumentType.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/JSExecState.h>
#include "WebKitDOMDocumentTypePrivate.h"
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMNamedNodeMapPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMDocumentType* kit(WebCore::DocumentType* obj)
{
    return WEBKIT_DOM_DOCUMENT_TYPE(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::DocumentType* core(WebKitDOMDocumentType* request)
{
    return request ? static_cast<WebCore::DocumentType*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMDocumentType* wrapDocumentType(WebCore::DocumentType* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_DOCUMENT_TYPE(g_object_new(WEBKIT_DOM_TYPE_DOCUMENT_TYPE, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_document_type_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::DocumentType* coreTarget = static_cast<WebCore::DocumentType*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_document_type_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::DocumentType* coreTarget = static_cast<WebCore::DocumentType*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_document_type_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::DocumentType* coreTarget = static_cast<WebCore::DocumentType*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_document_type_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_document_type_dispatch_event;
    iface->add_event_listener = webkit_dom_document_type_add_event_listener;
    iface->remove_event_listener = webkit_dom_document_type_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMDocumentType, webkit_dom_document_type, WEBKIT_DOM_TYPE_NODE, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_document_type_dom_event_target_init))

enum {
    DOM_DOCUMENT_TYPE_PROP_0,
    DOM_DOCUMENT_TYPE_PROP_NAME,
    DOM_DOCUMENT_TYPE_PROP_ENTITIES,
    DOM_DOCUMENT_TYPE_PROP_NOTATIONS,
    DOM_DOCUMENT_TYPE_PROP_INTERNAL_SUBSET,
    DOM_DOCUMENT_TYPE_PROP_PUBLIC_ID,
    DOM_DOCUMENT_TYPE_PROP_SYSTEM_ID,
};

static void webkit_dom_document_type_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMDocumentType* self = WEBKIT_DOM_DOCUMENT_TYPE(object);

    switch (propertyId) {
    case DOM_DOCUMENT_TYPE_PROP_NAME:
        g_value_take_string(value, webkit_dom_document_type_get_name(self));
        break;
    case DOM_DOCUMENT_TYPE_PROP_ENTITIES:
        g_value_set_object(value, webkit_dom_document_type_get_entities(self));
        break;
    case DOM_DOCUMENT_TYPE_PROP_NOTATIONS:
        g_value_set_object(value, webkit_dom_document_type_get_notations(self));
        break;
    case DOM_DOCUMENT_TYPE_PROP_INTERNAL_SUBSET:
        g_value_take_string(value, webkit_dom_document_type_get_internal_subset(self));
        break;
    case DOM_DOCUMENT_TYPE_PROP_PUBLIC_ID:
        g_value_take_string(value, webkit_dom_document_type_get_public_id(self));
        break;
    case DOM_DOCUMENT_TYPE_PROP_SYSTEM_ID:
        g_value_take_string(value, webkit_dom_document_type_get_system_id(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_document_type_class_init(WebKitDOMDocumentTypeClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->get_property = webkit_dom_document_type_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_TYPE_PROP_NAME,
        g_param_spec_string(
            "name",
            "DocumentType:name",
            "read-only gchar* DocumentType:name",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_TYPE_PROP_ENTITIES,
        g_param_spec_object(
            "entities",
            "DocumentType:entities",
            "read-only WebKitDOMNamedNodeMap* DocumentType:entities",
            WEBKIT_DOM_TYPE_NAMED_NODE_MAP,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_TYPE_PROP_NOTATIONS,
        g_param_spec_object(
            "notations",
            "DocumentType:notations",
            "read-only WebKitDOMNamedNodeMap* DocumentType:notations",
            WEBKIT_DOM_TYPE_NAMED_NODE_MAP,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_TYPE_PROP_INTERNAL_SUBSET,
        g_param_spec_string(
            "internal-subset",
            "DocumentType:internal-subset",
            "read-only gchar* DocumentType:internal-subset",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_TYPE_PROP_PUBLIC_ID,
        g_param_spec_string(
            "public-id",
            "DocumentType:public-id",
            "read-only gchar* DocumentType:public-id",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_TYPE_PROP_SYSTEM_ID,
        g_param_spec_string(
            "system-id",
            "DocumentType:system-id",
            "read-only gchar* DocumentType:system-id",
            "",
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_document_type_init(WebKitDOMDocumentType* request)
{
    UNUSED_PARAM(request);
}

gchar* webkit_dom_document_type_get_name(WebKitDOMDocumentType* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT_TYPE(self), 0);
    WebCore::DocumentType* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->name());
    return result;
}

WebKitDOMNamedNodeMap* webkit_dom_document_type_get_entities(WebKitDOMDocumentType* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT_TYPE(self), nullptr);
    return nullptr;
}

WebKitDOMNamedNodeMap* webkit_dom_document_type_get_notations(WebKitDOMDocumentType* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT_TYPE(self), nullptr);
    return nullptr;
}

gchar* webkit_dom_document_type_get_internal_subset(WebKitDOMDocumentType* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT_TYPE(self), nullptr);
    return nullptr;
}

gchar* webkit_dom_document_type_get_public_id(WebKitDOMDocumentType* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT_TYPE(self), 0);
    WebCore::DocumentType* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->publicId());
    return result;
}

gchar* webkit_dom_document_type_get_system_id(WebKitDOMDocumentType* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT_TYPE(self), 0);
    WebCore::DocumentType* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->systemId());
    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS;
