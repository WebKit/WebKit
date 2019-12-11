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
#include "WebKitDOMElement.h"

#include <WebCore/CSSImportRule.h>
#include <WebCore/DOMException.h>
#include <WebCore/DOMRect.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include <WebCore/ScrollIntoViewOptions.h>
#include <WebCore/StyledElement.h>
#include "WebKitDOMAttrPrivate.h"
#include "WebKitDOMCSSStyleDeclarationPrivate.h"
#include "WebKitDOMClientRectListPrivate.h"
#include "WebKitDOMClientRectPrivate.h"
#include "WebKitDOMDOMTokenListPrivate.h"
#include "WebKitDOMElementPrivate.h"
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLCollectionPrivate.h"
#include "WebKitDOMNamedNodeMapPrivate.h"
#include "WebKitDOMNodeListPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include "WebKitDOMElementUnstable.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

static gboolean webkit_dom_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::Element* coreTarget = static_cast<WebCore::Element*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::Element* coreTarget = static_cast<WebCore::Element*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::Element* coreTarget = static_cast<WebCore::Element*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

void webkitDOMElementDOMEventTargetInit(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_element_dispatch_event;
    iface->add_event_listener = webkit_dom_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_element_remove_event_listener;
}

enum {
    DOM_ELEMENT_PROP_0,
    DOM_ELEMENT_PROP_TAG_NAME,
    DOM_ELEMENT_PROP_ATTRIBUTES,
    DOM_ELEMENT_PROP_STYLE,
    DOM_ELEMENT_PROP_ID,
    DOM_ELEMENT_PROP_NAMESPACE_URI,
    DOM_ELEMENT_PROP_PREFIX,
    DOM_ELEMENT_PROP_LOCAL_NAME,
    DOM_ELEMENT_PROP_OFFSET_LEFT,
    DOM_ELEMENT_PROP_OFFSET_TOP,
    DOM_ELEMENT_PROP_OFFSET_WIDTH,
    DOM_ELEMENT_PROP_OFFSET_HEIGHT,
    DOM_ELEMENT_PROP_CLIENT_LEFT,
    DOM_ELEMENT_PROP_CLIENT_TOP,
    DOM_ELEMENT_PROP_CLIENT_WIDTH,
    DOM_ELEMENT_PROP_CLIENT_HEIGHT,
    DOM_ELEMENT_PROP_SCROLL_LEFT,
    DOM_ELEMENT_PROP_SCROLL_TOP,
    DOM_ELEMENT_PROP_SCROLL_WIDTH,
    DOM_ELEMENT_PROP_SCROLL_HEIGHT,
    DOM_ELEMENT_PROP_OFFSET_PARENT,
    DOM_ELEMENT_PROP_INNER_HTML,
    DOM_ELEMENT_PROP_OUTER_HTML,
    DOM_ELEMENT_PROP_CLASS_NAME,
    DOM_ELEMENT_PROP_CLASS_LIST,
    DOM_ELEMENT_PROP_WEBKIT_REGION_OVERSET,
    DOM_ELEMENT_PROP_PREVIOUS_ELEMENT_SIBLING,
    DOM_ELEMENT_PROP_NEXT_ELEMENT_SIBLING,
    DOM_ELEMENT_PROP_CHILDREN,
    DOM_ELEMENT_PROP_FIRST_ELEMENT_CHILD,
    DOM_ELEMENT_PROP_LAST_ELEMENT_CHILD,
    DOM_ELEMENT_PROP_CHILD_ELEMENT_COUNT,
};

static void webkit_dom_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMElement* self = WEBKIT_DOM_ELEMENT(object);

    switch (propertyId) {
    case DOM_ELEMENT_PROP_ID:
        webkit_dom_element_set_id(self, g_value_get_string(value));
        break;
    case DOM_ELEMENT_PROP_SCROLL_LEFT:
        webkit_dom_element_set_scroll_left(self, g_value_get_long(value));
        break;
    case DOM_ELEMENT_PROP_SCROLL_TOP:
        webkit_dom_element_set_scroll_top(self, g_value_get_long(value));
        break;
    case DOM_ELEMENT_PROP_INNER_HTML:
        webkit_dom_element_set_inner_html(self, g_value_get_string(value), nullptr);
        break;
    case DOM_ELEMENT_PROP_OUTER_HTML:
        webkit_dom_element_set_outer_html(self, g_value_get_string(value), nullptr);
        break;
    case DOM_ELEMENT_PROP_CLASS_NAME:
        webkit_dom_element_set_class_name(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMElement* self = WEBKIT_DOM_ELEMENT(object);

    switch (propertyId) {
    case DOM_ELEMENT_PROP_TAG_NAME:
        g_value_take_string(value, webkit_dom_element_get_tag_name(self));
        break;
    case DOM_ELEMENT_PROP_ATTRIBUTES:
        g_value_set_object(value, webkit_dom_element_get_attributes(self));
        break;
    case DOM_ELEMENT_PROP_STYLE:
        g_value_set_object(value, webkit_dom_element_get_style(self));
        break;
    case DOM_ELEMENT_PROP_ID:
        g_value_take_string(value, webkit_dom_element_get_id(self));
        break;
    case DOM_ELEMENT_PROP_NAMESPACE_URI:
        g_value_take_string(value, webkit_dom_element_get_namespace_uri(self));
        break;
    case DOM_ELEMENT_PROP_PREFIX:
        g_value_take_string(value, webkit_dom_element_get_prefix(self));
        break;
    case DOM_ELEMENT_PROP_LOCAL_NAME:
        g_value_take_string(value, webkit_dom_element_get_local_name(self));
        break;
    case DOM_ELEMENT_PROP_OFFSET_LEFT:
        g_value_set_double(value, webkit_dom_element_get_offset_left(self));
        break;
    case DOM_ELEMENT_PROP_OFFSET_TOP:
        g_value_set_double(value, webkit_dom_element_get_offset_top(self));
        break;
    case DOM_ELEMENT_PROP_OFFSET_WIDTH:
        g_value_set_double(value, webkit_dom_element_get_offset_width(self));
        break;
    case DOM_ELEMENT_PROP_OFFSET_HEIGHT:
        g_value_set_double(value, webkit_dom_element_get_offset_height(self));
        break;
    case DOM_ELEMENT_PROP_CLIENT_LEFT:
        g_value_set_double(value, webkit_dom_element_get_client_left(self));
        break;
    case DOM_ELEMENT_PROP_CLIENT_TOP:
        g_value_set_double(value, webkit_dom_element_get_client_top(self));
        break;
    case DOM_ELEMENT_PROP_CLIENT_WIDTH:
        g_value_set_double(value, webkit_dom_element_get_client_width(self));
        break;
    case DOM_ELEMENT_PROP_CLIENT_HEIGHT:
        g_value_set_double(value, webkit_dom_element_get_client_height(self));
        break;
    case DOM_ELEMENT_PROP_SCROLL_LEFT:
        g_value_set_long(value, webkit_dom_element_get_scroll_left(self));
        break;
    case DOM_ELEMENT_PROP_SCROLL_TOP:
        g_value_set_long(value, webkit_dom_element_get_scroll_top(self));
        break;
    case DOM_ELEMENT_PROP_SCROLL_WIDTH:
        g_value_set_long(value, webkit_dom_element_get_scroll_width(self));
        break;
    case DOM_ELEMENT_PROP_SCROLL_HEIGHT:
        g_value_set_long(value, webkit_dom_element_get_scroll_height(self));
        break;
    case DOM_ELEMENT_PROP_OFFSET_PARENT:
        g_value_set_object(value, webkit_dom_element_get_offset_parent(self));
        break;
    case DOM_ELEMENT_PROP_INNER_HTML:
        g_value_take_string(value, webkit_dom_element_get_inner_html(self));
        break;
    case DOM_ELEMENT_PROP_OUTER_HTML:
        g_value_take_string(value, webkit_dom_element_get_outer_html(self));
        break;
    case DOM_ELEMENT_PROP_CLASS_NAME:
        g_value_take_string(value, webkit_dom_element_get_class_name(self));
        break;
    case DOM_ELEMENT_PROP_CLASS_LIST:
        g_value_set_object(value, webkit_dom_element_get_class_list(self));
        break;
    case DOM_ELEMENT_PROP_WEBKIT_REGION_OVERSET:
        g_warning("%s: CSS Regions support has been removed, the webkit-region-overset property no longer works.", __func__);
        g_value_set_static_string(value, nullptr);
        break;
    case DOM_ELEMENT_PROP_PREVIOUS_ELEMENT_SIBLING:
        g_value_set_object(value, webkit_dom_element_get_previous_element_sibling(self));
        break;
    case DOM_ELEMENT_PROP_NEXT_ELEMENT_SIBLING:
        g_value_set_object(value, webkit_dom_element_get_next_element_sibling(self));
        break;
    case DOM_ELEMENT_PROP_CHILDREN:
        g_value_set_object(value, webkit_dom_element_get_children(self));
        break;
    case DOM_ELEMENT_PROP_FIRST_ELEMENT_CHILD:
        g_value_set_object(value, webkit_dom_element_get_first_element_child(self));
        break;
    case DOM_ELEMENT_PROP_LAST_ELEMENT_CHILD:
        g_value_set_object(value, webkit_dom_element_get_last_element_child(self));
        break;
    case DOM_ELEMENT_PROP_CHILD_ELEMENT_COUNT:
        g_value_set_ulong(value, webkit_dom_element_get_child_element_count(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

void webkitDOMElementInstallProperties(GObjectClass* gobjectClass)
{
    gobjectClass->set_property = webkit_dom_element_set_property;
    gobjectClass->get_property = webkit_dom_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_TAG_NAME,
        g_param_spec_string(
            "tag-name",
            "Element:tag-name",
            "read-only gchar* Element:tag-name",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_ATTRIBUTES,
        g_param_spec_object(
            "attributes",
            "Element:attributes",
            "read-only WebKitDOMNamedNodeMap* Element:attributes",
            WEBKIT_DOM_TYPE_NAMED_NODE_MAP,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_STYLE,
        g_param_spec_object(
            "style",
            "Element:style",
            "read-only WebKitDOMCSSStyleDeclaration* Element:style",
            WEBKIT_DOM_TYPE_CSS_STYLE_DECLARATION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_ID,
        g_param_spec_string(
            "id",
            "Element:id",
            "read-write gchar* Element:id",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_NAMESPACE_URI,
        g_param_spec_string(
            "namespace-uri",
            "Element:namespace-uri",
            "read-only gchar* Element:namespace-uri",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_PREFIX,
        g_param_spec_string(
            "prefix",
            "Element:prefix",
            "read-only gchar* Element:prefix",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_LOCAL_NAME,
        g_param_spec_string(
            "local-name",
            "Element:local-name",
            "read-only gchar* Element:local-name",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_OFFSET_LEFT,
        g_param_spec_double(
            "offset-left",
            "Element:offset-left",
            "read-only gdouble Element:offset-left",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_OFFSET_TOP,
        g_param_spec_double(
            "offset-top",
            "Element:offset-top",
            "read-only gdouble Element:offset-top",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_OFFSET_WIDTH,
        g_param_spec_double(
            "offset-width",
            "Element:offset-width",
            "read-only gdouble Element:offset-width",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_OFFSET_HEIGHT,
        g_param_spec_double(
            "offset-height",
            "Element:offset-height",
            "read-only gdouble Element:offset-height",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_CLIENT_LEFT,
        g_param_spec_double(
            "client-left",
            "Element:client-left",
            "read-only gdouble Element:client-left",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_CLIENT_TOP,
        g_param_spec_double(
            "client-top",
            "Element:client-top",
            "read-only gdouble Element:client-top",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_CLIENT_WIDTH,
        g_param_spec_double(
            "client-width",
            "Element:client-width",
            "read-only gdouble Element:client-width",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_CLIENT_HEIGHT,
        g_param_spec_double(
            "client-height",
            "Element:client-height",
            "read-only gdouble Element:client-height",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_SCROLL_LEFT,
        g_param_spec_long(
            "scroll-left",
            "Element:scroll-left",
            "read-write glong Element:scroll-left",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_SCROLL_TOP,
        g_param_spec_long(
            "scroll-top",
            "Element:scroll-top",
            "read-write glong Element:scroll-top",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_SCROLL_WIDTH,
        g_param_spec_long(
            "scroll-width",
            "Element:scroll-width",
            "read-only glong Element:scroll-width",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_SCROLL_HEIGHT,
        g_param_spec_long(
            "scroll-height",
            "Element:scroll-height",
            "read-only glong Element:scroll-height",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_OFFSET_PARENT,
        g_param_spec_object(
            "offset-parent",
            "Element:offset-parent",
            "read-only WebKitDOMElement* Element:offset-parent",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_INNER_HTML,
        g_param_spec_string(
            "inner-html",
            "Element:inner-html",
            "read-write gchar* Element:inner-html",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_OUTER_HTML,
        g_param_spec_string(
            "outer-html",
            "Element:outer-html",
            "read-write gchar* Element:outer-html",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_CLASS_NAME,
        g_param_spec_string(
            "class-name",
            "Element:class-name",
            "read-write gchar* Element:class-name",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_CLASS_LIST,
        g_param_spec_object(
            "class-list",
            "Element:class-list",
            "read-only WebKitDOMDOMTokenList* Element:class-list",
            WEBKIT_DOM_TYPE_DOM_TOKEN_LIST,
            WEBKIT_PARAM_READABLE));

    /**
     * WebKitDOMElement:webkit-region-overset:
     *
     * This property is always %NULL.
     *
     * Deprecated: 2.20
     */
    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_WEBKIT_REGION_OVERSET,
        g_param_spec_string(
            "webkit-region-overset",
            "Element:webkit-region-overset",
            "read-only gchar* Element:webkit-region-overset",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_PREVIOUS_ELEMENT_SIBLING,
        g_param_spec_object(
            "previous-element-sibling",
            "Element:previous-element-sibling",
            "read-only WebKitDOMElement* Element:previous-element-sibling",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_NEXT_ELEMENT_SIBLING,
        g_param_spec_object(
            "next-element-sibling",
            "Element:next-element-sibling",
            "read-only WebKitDOMElement* Element:next-element-sibling",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_CHILDREN,
        g_param_spec_object(
            "children",
            "Element:children",
            "read-only WebKitDOMHTMLCollection* Element:children",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_FIRST_ELEMENT_CHILD,
        g_param_spec_object(
            "first-element-child",
            "Element:first-element-child",
            "read-only WebKitDOMElement* Element:first-element-child",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_LAST_ELEMENT_CHILD,
        g_param_spec_object(
            "last-element-child",
            "Element:last-element-child",
            "read-only WebKitDOMElement* Element:last-element-child",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_ELEMENT_PROP_CHILD_ELEMENT_COUNT,
        g_param_spec_ulong(
            "child-element-count",
            "Element:child-element-count",
            "read-only gulong Element:child-element-count",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

}

gchar* webkit_dom_element_get_attribute(WebKitDOMElement* self, const gchar* name)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(name, 0);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedName = WTF::String::fromUTF8(name);
    gchar* result = convertToUTF8String(item->getAttribute(convertedName));
    return result;
}

void webkit_dom_element_set_attribute(WebKitDOMElement* self, const gchar* name, const gchar* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    g_return_if_fail(name);
    g_return_if_fail(value);
    g_return_if_fail(!error || !*error);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedName = WTF::String::fromUTF8(name);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    auto result = item->setAttribute(convertedName, convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_element_remove_attribute(WebKitDOMElement* self, const gchar* name)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    g_return_if_fail(name);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedName = WTF::String::fromUTF8(name);
    item->removeAttribute(convertedName);
}

WebKitDOMAttr* webkit_dom_element_get_attribute_node(WebKitDOMElement* self, const gchar* name)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(name, 0);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedName = WTF::String::fromUTF8(name);
    RefPtr<WebCore::Attr> gobjectResult = WTF::getPtr(item->getAttributeNode(convertedName));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMAttr* webkit_dom_element_set_attribute_node(WebKitDOMElement* self, WebKitDOMAttr* newAttr, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_ATTR(newAttr), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Element* item = WebKit::core(self);
    WebCore::Attr* convertedNewAttr = WebKit::core(newAttr);
    auto result = item->setAttributeNode(*convertedNewAttr);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().get());
}

WebKitDOMAttr* webkit_dom_element_remove_attribute_node(WebKitDOMElement* self, WebKitDOMAttr* oldAttr, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_ATTR(oldAttr), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Element* item = WebKit::core(self);
    WebCore::Attr* convertedOldAttr = WebKit::core(oldAttr);
    auto result = item->removeAttributeNode(*convertedOldAttr);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMHTMLCollection* webkit_dom_element_get_elements_by_tag_name_as_html_collection(WebKitDOMElement* self, const gchar* name)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(name, 0);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedName = WTF::String::fromUTF8(name);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->getElementsByTagName(convertedName));
    return WebKit::kit(gobjectResult.get());
}

gboolean webkit_dom_element_has_attributes(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), FALSE);
    WebCore::Element* item = WebKit::core(self);
    gboolean result = item->hasAttributes();
    return result;
}

gchar* webkit_dom_element_get_attribute_ns(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* localName)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(namespaceURI, 0);
    g_return_val_if_fail(localName, 0);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedNamespaceURI = WTF::String::fromUTF8(namespaceURI);
    WTF::String convertedLocalName = WTF::String::fromUTF8(localName);
    gchar* result = convertToUTF8String(item->getAttributeNS(convertedNamespaceURI, convertedLocalName));
    return result;
}

void webkit_dom_element_set_attribute_ns(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* qualifiedName, const gchar* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    g_return_if_fail(qualifiedName);
    g_return_if_fail(value);
    g_return_if_fail(!error || !*error);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedNamespaceURI = WTF::String::fromUTF8(namespaceURI);
    WTF::String convertedQualifiedName = WTF::String::fromUTF8(qualifiedName);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    auto result = item->setAttributeNS(convertedNamespaceURI, convertedQualifiedName, convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_element_remove_attribute_ns(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* localName)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    g_return_if_fail(namespaceURI);
    g_return_if_fail(localName);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedNamespaceURI = WTF::String::fromUTF8(namespaceURI);
    WTF::String convertedLocalName = WTF::String::fromUTF8(localName);
    item->removeAttributeNS(convertedNamespaceURI, convertedLocalName);
}

WebKitDOMHTMLCollection* webkit_dom_element_get_elements_by_tag_name_ns_as_html_collection(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* localName)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(namespaceURI, 0);
    g_return_val_if_fail(localName, 0);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedNamespaceURI = WTF::String::fromUTF8(namespaceURI);
    WTF::String convertedLocalName = WTF::String::fromUTF8(localName);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->getElementsByTagNameNS(convertedNamespaceURI, convertedLocalName));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMAttr* webkit_dom_element_get_attribute_node_ns(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* localName)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(namespaceURI, 0);
    g_return_val_if_fail(localName, 0);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedNamespaceURI = WTF::String::fromUTF8(namespaceURI);
    WTF::String convertedLocalName = WTF::String::fromUTF8(localName);
    RefPtr<WebCore::Attr> gobjectResult = WTF::getPtr(item->getAttributeNodeNS(convertedNamespaceURI, convertedLocalName));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMAttr* webkit_dom_element_set_attribute_node_ns(WebKitDOMElement* self, WebKitDOMAttr* newAttr, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_ATTR(newAttr), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Element* item = WebKit::core(self);
    WebCore::Attr* convertedNewAttr = WebKit::core(newAttr);
    auto result = item->setAttributeNodeNS(*convertedNewAttr);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().get());
}

gboolean webkit_dom_element_has_attribute(WebKitDOMElement* self, const gchar* name)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), FALSE);
    g_return_val_if_fail(name, FALSE);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedName = WTF::String::fromUTF8(name);
    gboolean result = item->hasAttribute(convertedName);
    return result;
}

gboolean webkit_dom_element_has_attribute_ns(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* localName)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), FALSE);
    g_return_val_if_fail(namespaceURI, FALSE);
    g_return_val_if_fail(localName, FALSE);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedNamespaceURI = WTF::String::fromUTF8(namespaceURI);
    WTF::String convertedLocalName = WTF::String::fromUTF8(localName);
    gboolean result = item->hasAttributeNS(convertedNamespaceURI, convertedLocalName);
    return result;
}

void webkit_dom_element_focus(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    WebCore::Element* item = WebKit::core(self);
    item->focus();
}

void webkit_dom_element_blur(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    WebCore::Element* item = WebKit::core(self);
    item->blur();
}

void webkit_dom_element_scroll_into_view(WebKitDOMElement* self, gboolean alignWithTop)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    WebCore::Element* item = WebKit::core(self);
    item->scrollIntoView(alignWithTop);
}

void webkit_dom_element_scroll_into_view_if_needed(WebKitDOMElement* self, gboolean centerIfNeeded)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    WebCore::Element* item = WebKit::core(self);
    item->scrollIntoViewIfNeeded(centerIfNeeded);
}

void webkit_dom_element_scroll_by_lines(WebKitDOMElement* self, glong lines)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    WebCore::Element* item = WebKit::core(self);
    item->scrollByLines(lines);
}

void webkit_dom_element_scroll_by_pages(WebKitDOMElement* self, glong pages)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    WebCore::Element* item = WebKit::core(self);
    item->scrollByPages(pages);
}

WebKitDOMHTMLCollection* webkit_dom_element_get_elements_by_class_name_as_html_collection(WebKitDOMElement* self, const gchar* name)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(name, 0);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedName = WTF::String::fromUTF8(name);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->getElementsByClassName(convertedName));
    return WebKit::kit(gobjectResult.get());
}

gboolean webkit_dom_element_matches(WebKitDOMElement* self, const gchar* selectors, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), FALSE);
    g_return_val_if_fail(selectors, FALSE);
    g_return_val_if_fail(!error || !*error, FALSE);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedSelectors = WTF::String::fromUTF8(selectors);
    auto result = item->matches(convertedSelectors);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

WebKitDOMElement* webkit_dom_element_closest(WebKitDOMElement* self, const gchar* selectors, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(selectors, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedSelectors = WTF::String::fromUTF8(selectors);
    auto result = item->closest(convertedSelectors);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue());
}

gboolean webkit_dom_element_webkit_matches_selector(WebKitDOMElement* self, const gchar* selectors, GError** error)
{
    return webkit_dom_element_matches(self, selectors, error);
}

void webkit_dom_element_webkit_request_fullscreen(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    WebCore::Element* item = WebKit::core(self);
    item->webkitRequestFullscreen();
}

WebKitDOMElement* webkit_dom_element_insert_adjacent_element(WebKitDOMElement* self, const gchar* where, WebKitDOMElement* element, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(where, 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(element), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedWhere = WTF::String::fromUTF8(where);
    WebCore::Element* convertedElement = WebKit::core(element);
    auto result = item->insertAdjacentElement(convertedWhere, *convertedElement);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue());
}

void webkit_dom_element_insert_adjacent_html(WebKitDOMElement* self, const gchar* where, const gchar* html, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    g_return_if_fail(where);
    g_return_if_fail(html);
    g_return_if_fail(!error || !*error);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedWhere = WTF::String::fromUTF8(where);
    WTF::String convertedHtml = WTF::String::fromUTF8(html);
    auto result = item->insertAdjacentHTML(convertedWhere, convertedHtml);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_element_insert_adjacent_text(WebKitDOMElement* self, const gchar* where, const gchar* text, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    g_return_if_fail(where);
    g_return_if_fail(text);
    g_return_if_fail(!error || !*error);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedWhere = WTF::String::fromUTF8(where);
    WTF::String convertedText = WTF::String::fromUTF8(text);
    auto result = item->insertAdjacentText(convertedWhere, convertedText);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_element_request_pointer_lock(WebKitDOMElement* self)
{
#if ENABLE(POINTER_LOCK)
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    WebCore::Element* item = WebKit::core(self);
    item->requestPointerLock();
#else
    UNUSED_PARAM(self);
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Pointer Lock")
#endif /* ENABLE(POINTER_LOCK) */
}

void webkit_dom_element_remove(WebKitDOMElement* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    g_return_if_fail(!error || !*error);
    WebCore::Element* item = WebKit::core(self);
    auto result = item->remove();
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

WebKitDOMElement* webkit_dom_element_query_selector(WebKitDOMElement* self, const gchar* selectors, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(selectors, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedSelectors = WTF::String::fromUTF8(selectors);
    auto result = item->querySelector(convertedSelectors);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue());
}

WebKitDOMNodeList* webkit_dom_element_query_selector_all(WebKitDOMElement* self, const gchar* selectors, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    g_return_val_if_fail(selectors, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedSelectors = WTF::String::fromUTF8(selectors);
    auto result = item->querySelectorAll(convertedSelectors);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

gchar* webkit_dom_element_get_tag_name(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->tagName());
    return result;
}

WebKitDOMNamedNodeMap* webkit_dom_element_get_attributes(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    RefPtr<WebCore::NamedNodeMap> gobjectResult = WTF::getPtr(item->attributes());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMCSSStyleDeclaration* webkit_dom_element_get_style(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    auto& item = *WebKit::core(self);
    if (!is<WebCore::StyledElement>(item))
        return nullptr;
    return WebKit::kit(&downcast<WebCore::StyledElement>(item).cssomStyle());
}

gchar* webkit_dom_element_get_id(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->getIdAttribute());
    return result;
}

void webkit_dom_element_set_id(WebKitDOMElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::idAttr, convertedValue);
}

gchar* webkit_dom_element_get_namespace_uri(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->namespaceURI());
    return result;
}

gchar* webkit_dom_element_get_prefix(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->prefix());
    return result;
}

gchar* webkit_dom_element_get_local_name(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->localName());
    return result;
}

gdouble webkit_dom_element_get_offset_left(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gdouble result = item->offsetLeftForBindings();
    return result;
}

gdouble webkit_dom_element_get_offset_top(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gdouble result = item->offsetTopForBindings();
    return result;
}

gdouble webkit_dom_element_get_offset_width(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gdouble result = item->offsetWidth();
    return result;
}

gdouble webkit_dom_element_get_offset_height(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gdouble result = item->offsetHeight();
    return result;
}

gdouble webkit_dom_element_get_client_left(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gdouble result = item->clientLeft();
    return result;
}

gdouble webkit_dom_element_get_client_top(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gdouble result = item->clientTop();
    return result;
}

gdouble webkit_dom_element_get_client_width(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gdouble result = item->clientWidth();
    return result;
}

gdouble webkit_dom_element_get_client_height(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gdouble result = item->clientHeight();
    return result;
}

glong webkit_dom_element_get_scroll_left(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    glong result = item->scrollLeft();
    return result;
}

void webkit_dom_element_set_scroll_left(WebKitDOMElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    WebCore::Element* item = WebKit::core(self);
    item->setScrollLeft(value);
}

glong webkit_dom_element_get_scroll_top(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    glong result = item->scrollTop();
    return result;
}

void webkit_dom_element_set_scroll_top(WebKitDOMElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    WebCore::Element* item = WebKit::core(self);
    item->setScrollTop(value);
}

glong webkit_dom_element_get_scroll_width(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    glong result = item->scrollWidth();
    return result;
}

glong webkit_dom_element_get_scroll_height(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    glong result = item->scrollHeight();
    return result;
}

WebKitDOMClientRect* webkit_dom_element_get_bounding_client_rect(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), nullptr);
    WebCore::Element* item = WebKit::core(self);
    return WebKit::kit(item->getBoundingClientRect().ptr());
}

WebKitDOMClientRectList* webkit_dom_element_get_client_rects(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), nullptr);
    WebCore::Element* item = WebKit::core(self);
    return WebKit::kit(item->getClientRects().ptr());
}

WebKitDOMElement* webkit_dom_element_get_offset_parent(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->offsetParentForBindings());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_element_get_inner_html(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->innerHTML());
    return result;
}

void webkit_dom_element_set_inner_html(WebKitDOMElement* self, const gchar* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    g_return_if_fail(value);
    g_return_if_fail(!error || !*error);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    auto result = item->setInnerHTML(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

gchar* webkit_dom_element_get_outer_html(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->outerHTML());
    return result;
}

void webkit_dom_element_set_outer_html(WebKitDOMElement* self, const gchar* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    g_return_if_fail(value);
    g_return_if_fail(!error || !*error);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    auto result = item->setOuterHTML(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

gchar* webkit_dom_element_get_class_name(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::classAttr));
    return result;
}

void webkit_dom_element_set_class_name(WebKitDOMElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::Element* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::classAttr, convertedValue);
}

WebKitDOMDOMTokenList* webkit_dom_element_get_class_list(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    RefPtr<WebCore::DOMTokenList> gobjectResult = WTF::getPtr(item->classList());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMElement* webkit_dom_element_get_previous_element_sibling(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->previousElementSibling());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMElement* webkit_dom_element_get_next_element_sibling(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->nextElementSibling());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMHTMLCollection* webkit_dom_element_get_children(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->children());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMElement* webkit_dom_element_get_first_element_child(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->firstElementChild());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMElement* webkit_dom_element_get_last_element_child(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->lastElementChild());
    return WebKit::kit(gobjectResult.get());
}

gulong webkit_dom_element_get_child_element_count(WebKitDOMElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), 0);
    WebCore::Element* item = WebKit::core(self);
    gulong result = item->childElementCount();
    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS;
