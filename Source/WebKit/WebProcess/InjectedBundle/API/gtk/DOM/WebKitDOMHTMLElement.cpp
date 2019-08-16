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
#include "WebKitDOMHTMLElement.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include "GObjectEventListener.h"
#include <WebCore/HTMLNames.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include "WebKitDOMHTMLElementUnstable.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLElement* kit(WebCore::HTMLElement* obj)
{
    return WEBKIT_DOM_HTML_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::HTMLElement* core(WebKitDOMHTMLElement* request)
{
    return request ? static_cast<WebCore::HTMLElement*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLElement* wrapHTMLElement(WebCore::HTMLElement* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_HTML_ELEMENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

static gboolean webkit_dom_html_element_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::HTMLElement* coreTarget = static_cast<WebCore::HTMLElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_html_element_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLElement* coreTarget = static_cast<WebCore::HTMLElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_html_element_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::HTMLElement* coreTarget = static_cast<WebCore::HTMLElement*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static void webkit_dom_html_element_dom_event_target_init(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_html_element_dispatch_event;
    iface->add_event_listener = webkit_dom_html_element_add_event_listener;
    iface->remove_event_listener = webkit_dom_html_element_remove_event_listener;
}

G_DEFINE_TYPE_WITH_CODE(WebKitDOMHTMLElement, webkit_dom_html_element, WEBKIT_DOM_TYPE_ELEMENT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkit_dom_html_element_dom_event_target_init))

enum {
    DOM_HTML_ELEMENT_PROP_0,
    DOM_HTML_ELEMENT_PROP_TITLE,
    DOM_HTML_ELEMENT_PROP_LANG,
    DOM_HTML_ELEMENT_PROP_TRANSLATE,
    DOM_HTML_ELEMENT_PROP_DIR,
    DOM_HTML_ELEMENT_PROP_TAB_INDEX,
    DOM_HTML_ELEMENT_PROP_DRAGGABLE,
    DOM_HTML_ELEMENT_PROP_WEBKITDROPZONE,
    DOM_HTML_ELEMENT_PROP_HIDDEN,
    DOM_HTML_ELEMENT_PROP_ACCESS_KEY,
    DOM_HTML_ELEMENT_PROP_INNER_TEXT,
    DOM_HTML_ELEMENT_PROP_OUTER_TEXT,
    DOM_HTML_ELEMENT_PROP_CONTENT_EDITABLE,
    DOM_HTML_ELEMENT_PROP_IS_CONTENT_EDITABLE,
    DOM_HTML_ELEMENT_PROP_SPELLCHECK,
};

static void webkit_dom_html_element_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLElement* self = WEBKIT_DOM_HTML_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_ELEMENT_PROP_TITLE:
        webkit_dom_html_element_set_title(self, g_value_get_string(value));
        break;
    case DOM_HTML_ELEMENT_PROP_LANG:
        webkit_dom_html_element_set_lang(self, g_value_get_string(value));
        break;
    case DOM_HTML_ELEMENT_PROP_TRANSLATE:
        webkit_dom_html_element_set_translate(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_ELEMENT_PROP_DIR:
        webkit_dom_html_element_set_dir(self, g_value_get_string(value));
        break;
    case DOM_HTML_ELEMENT_PROP_TAB_INDEX:
        webkit_dom_html_element_set_tab_index(self, g_value_get_long(value));
        break;
    case DOM_HTML_ELEMENT_PROP_DRAGGABLE:
        webkit_dom_html_element_set_draggable(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_ELEMENT_PROP_WEBKITDROPZONE:
        webkit_dom_html_element_set_webkitdropzone(self, g_value_get_string(value));
        break;
    case DOM_HTML_ELEMENT_PROP_HIDDEN:
        webkit_dom_html_element_set_hidden(self, g_value_get_boolean(value));
        break;
    case DOM_HTML_ELEMENT_PROP_ACCESS_KEY:
        webkit_dom_html_element_set_access_key(self, g_value_get_string(value));
        break;
    case DOM_HTML_ELEMENT_PROP_INNER_TEXT:
        webkit_dom_html_element_set_inner_text(self, g_value_get_string(value), nullptr);
        break;
    case DOM_HTML_ELEMENT_PROP_OUTER_TEXT:
        webkit_dom_html_element_set_outer_text(self, g_value_get_string(value), nullptr);
        break;
    case DOM_HTML_ELEMENT_PROP_CONTENT_EDITABLE:
        webkit_dom_html_element_set_content_editable(self, g_value_get_string(value), nullptr);
        break;
    case DOM_HTML_ELEMENT_PROP_SPELLCHECK:
        webkit_dom_html_element_set_spellcheck(self, g_value_get_boolean(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_element_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLElement* self = WEBKIT_DOM_HTML_ELEMENT(object);

    switch (propertyId) {
    case DOM_HTML_ELEMENT_PROP_TITLE:
        g_value_take_string(value, webkit_dom_html_element_get_title(self));
        break;
    case DOM_HTML_ELEMENT_PROP_LANG:
        g_value_take_string(value, webkit_dom_html_element_get_lang(self));
        break;
    case DOM_HTML_ELEMENT_PROP_TRANSLATE:
        g_value_set_boolean(value, webkit_dom_html_element_get_translate(self));
        break;
    case DOM_HTML_ELEMENT_PROP_DIR:
        g_value_take_string(value, webkit_dom_html_element_get_dir(self));
        break;
    case DOM_HTML_ELEMENT_PROP_TAB_INDEX:
        g_value_set_long(value, webkit_dom_html_element_get_tab_index(self));
        break;
    case DOM_HTML_ELEMENT_PROP_DRAGGABLE:
        g_value_set_boolean(value, webkit_dom_html_element_get_draggable(self));
        break;
    case DOM_HTML_ELEMENT_PROP_WEBKITDROPZONE:
        g_value_take_string(value, webkit_dom_html_element_get_webkitdropzone(self));
        break;
    case DOM_HTML_ELEMENT_PROP_HIDDEN:
        g_value_set_boolean(value, webkit_dom_html_element_get_hidden(self));
        break;
    case DOM_HTML_ELEMENT_PROP_ACCESS_KEY:
        g_value_take_string(value, webkit_dom_html_element_get_access_key(self));
        break;
    case DOM_HTML_ELEMENT_PROP_INNER_TEXT:
        g_value_take_string(value, webkit_dom_html_element_get_inner_text(self));
        break;
    case DOM_HTML_ELEMENT_PROP_OUTER_TEXT:
        g_value_take_string(value, webkit_dom_html_element_get_outer_text(self));
        break;
    case DOM_HTML_ELEMENT_PROP_CONTENT_EDITABLE:
        g_value_take_string(value, webkit_dom_html_element_get_content_editable(self));
        break;
    case DOM_HTML_ELEMENT_PROP_IS_CONTENT_EDITABLE:
        g_value_set_boolean(value, webkit_dom_html_element_get_is_content_editable(self));
        break;
    case DOM_HTML_ELEMENT_PROP_SPELLCHECK:
        g_value_set_boolean(value, webkit_dom_html_element_get_spellcheck(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_element_class_init(WebKitDOMHTMLElementClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_element_set_property;
    gobjectClass->get_property = webkit_dom_html_element_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_TITLE,
        g_param_spec_string(
            "title",
            "HTMLElement:title",
            "read-write gchar* HTMLElement:title",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_LANG,
        g_param_spec_string(
            "lang",
            "HTMLElement:lang",
            "read-write gchar* HTMLElement:lang",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_TRANSLATE,
        g_param_spec_boolean(
            "translate",
            "HTMLElement:translate",
            "read-write gboolean HTMLElement:translate",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_DIR,
        g_param_spec_string(
            "dir",
            "HTMLElement:dir",
            "read-write gchar* HTMLElement:dir",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_TAB_INDEX,
        g_param_spec_long(
            "tab-index",
            "HTMLElement:tab-index",
            "read-write glong HTMLElement:tab-index",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_DRAGGABLE,
        g_param_spec_boolean(
            "draggable",
            "HTMLElement:draggable",
            "read-write gboolean HTMLElement:draggable",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_WEBKITDROPZONE,
        g_param_spec_string(
            "webkitdropzone",
            "HTMLElement:webkitdropzone",
            "read-write gchar* HTMLElement:webkitdropzone",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_HIDDEN,
        g_param_spec_boolean(
            "hidden",
            "HTMLElement:hidden",
            "read-write gboolean HTMLElement:hidden",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_ACCESS_KEY,
        g_param_spec_string(
            "access-key",
            "HTMLElement:access-key",
            "read-write gchar* HTMLElement:access-key",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_INNER_TEXT,
        g_param_spec_string(
            "inner-text",
            "HTMLElement:inner-text",
            "read-write gchar* HTMLElement:inner-text",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_OUTER_TEXT,
        g_param_spec_string(
            "outer-text",
            "HTMLElement:outer-text",
            "read-write gchar* HTMLElement:outer-text",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_CONTENT_EDITABLE,
        g_param_spec_string(
            "content-editable",
            "HTMLElement:content-editable",
            "read-write gchar* HTMLElement:content-editable",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_IS_CONTENT_EDITABLE,
        g_param_spec_boolean(
            "is-content-editable",
            "HTMLElement:is-content-editable",
            "read-only gboolean HTMLElement:is-content-editable",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_ELEMENT_PROP_SPELLCHECK,
        g_param_spec_boolean(
            "spellcheck",
            "HTMLElement:spellcheck",
            "read-write gboolean HTMLElement:spellcheck",
            FALSE,
            WEBKIT_PARAM_READWRITE));

}

static void webkit_dom_html_element_init(WebKitDOMHTMLElement* request)
{
    UNUSED_PARAM(request);
}

void webkit_dom_html_element_click(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    WebCore::HTMLElement* item = WebKit::core(self);
    item->click();
}

gchar* webkit_dom_html_element_get_title(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), 0);
    WebCore::HTMLElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::titleAttr));
    return result;
}

void webkit_dom_html_element_set_title(WebKitDOMHTMLElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::titleAttr, convertedValue);
}

gchar* webkit_dom_html_element_get_lang(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), 0);
    WebCore::HTMLElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::langAttr));
    return result;
}

void webkit_dom_html_element_set_lang(WebKitDOMHTMLElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::langAttr, convertedValue);
}

gboolean webkit_dom_html_element_get_translate(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), FALSE);
    WebCore::HTMLElement* item = WebKit::core(self);
    gboolean result = item->translate();
    return result;
}

void webkit_dom_html_element_set_translate(WebKitDOMHTMLElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    WebCore::HTMLElement* item = WebKit::core(self);
    item->setTranslate(value);
}

gchar* webkit_dom_html_element_get_dir(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), 0);
    WebCore::HTMLElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->dir());
    return result;
}

void webkit_dom_html_element_set_dir(WebKitDOMHTMLElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setDir(convertedValue);
}

glong webkit_dom_html_element_get_tab_index(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), 0);
    WebCore::HTMLElement* item = WebKit::core(self);
    glong result = item->tabIndexForBindings();
    return result;
}

void webkit_dom_html_element_set_tab_index(WebKitDOMHTMLElement* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    WebCore::HTMLElement* item = WebKit::core(self);
    item->setTabIndexForBindings(value);
}

gboolean webkit_dom_html_element_get_draggable(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), FALSE);
    WebCore::HTMLElement* item = WebKit::core(self);
    gboolean result = item->draggable();
    return result;
}

void webkit_dom_html_element_set_draggable(WebKitDOMHTMLElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    WebCore::HTMLElement* item = WebKit::core(self);
    item->setDraggable(value);
}

gchar* webkit_dom_html_element_get_webkitdropzone(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), 0);
    WebCore::HTMLElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::webkitdropzoneAttr));
    return result;
}

void webkit_dom_html_element_set_webkitdropzone(WebKitDOMHTMLElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::webkitdropzoneAttr, convertedValue);
}

gboolean webkit_dom_html_element_get_hidden(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), FALSE);
    WebCore::HTMLElement* item = WebKit::core(self);
    gboolean result = item->hasAttributeWithoutSynchronization(WebCore::HTMLNames::hiddenAttr);
    return result;
}

void webkit_dom_html_element_set_hidden(WebKitDOMHTMLElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    WebCore::HTMLElement* item = WebKit::core(self);
    item->setBooleanAttribute(WebCore::HTMLNames::hiddenAttr, value);
}

gchar* webkit_dom_html_element_get_access_key(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), 0);
    WebCore::HTMLElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->attributeWithoutSynchronization(WebCore::HTMLNames::accesskeyAttr));
    return result;
}

void webkit_dom_html_element_set_access_key(WebKitDOMHTMLElement* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    g_return_if_fail(value);
    WebCore::HTMLElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setAttributeWithoutSynchronization(WebCore::HTMLNames::accesskeyAttr, convertedValue);
}

gchar* webkit_dom_html_element_get_inner_text(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), 0);
    WebCore::HTMLElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->innerText());
    return result;
}

void webkit_dom_html_element_set_inner_text(WebKitDOMHTMLElement* self, const gchar* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    g_return_if_fail(value);
    g_return_if_fail(!error || !*error);
    WebCore::HTMLElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    auto result = item->setInnerText(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

gchar* webkit_dom_html_element_get_outer_text(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), 0);
    WebCore::HTMLElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->outerText());
    return result;
}

void webkit_dom_html_element_set_outer_text(WebKitDOMHTMLElement* self, const gchar* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    g_return_if_fail(value);
    g_return_if_fail(!error || !*error);
    WebCore::HTMLElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    auto result = item->setOuterText(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

gchar* webkit_dom_html_element_get_content_editable(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), 0);
    WebCore::HTMLElement* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->contentEditable());
    return result;
}

void webkit_dom_html_element_set_content_editable(WebKitDOMHTMLElement* self, const gchar* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    g_return_if_fail(value);
    g_return_if_fail(!error || !*error);
    WebCore::HTMLElement* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    auto result = item->setContentEditable(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

gboolean webkit_dom_html_element_get_is_content_editable(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), FALSE);
    WebCore::HTMLElement* item = WebKit::core(self);
    gboolean result = item->isContentEditable();
    return result;
}

gboolean webkit_dom_html_element_get_spellcheck(WebKitDOMHTMLElement* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), FALSE);
    WebCore::HTMLElement* item = WebKit::core(self);
    gboolean result = item->spellcheck();
    return result;
}

void webkit_dom_html_element_set_spellcheck(WebKitDOMHTMLElement* self, gboolean value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    WebCore::HTMLElement* item = WebKit::core(self);
    item->setSpellcheck(value);
}

G_GNUC_END_IGNORE_DEPRECATIONS;
