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
#include "WebKitDOMDocument.h"

#include "ConvertToUTF8String.h"
#include "GObjectEventListener.h"
#include "WebKitDOMAttrPrivate.h"
#include "WebKitDOMCDATASectionPrivate.h"
#include "WebKitDOMCSSStyleDeclarationPrivate.h"
#include "WebKitDOMCommentPrivate.h"
#include "WebKitDOMDOMImplementationPrivate.h"
#include "WebKitDOMDOMWindowPrivate.h"
#include "WebKitDOMDocumentFragmentPrivate.h"
#include "WebKitDOMDocumentPrivate.h"
#include "WebKitDOMDocumentTypePrivate.h"
#include "WebKitDOMDocumentUnstable.h"
#include "WebKitDOMElementPrivate.h"
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMEventTarget.h"
#include "WebKitDOMHTMLCollectionPrivate.h"
#include "WebKitDOMHTMLElementPrivate.h"
#include "WebKitDOMHTMLHeadElementPrivate.h"
#include "WebKitDOMHTMLScriptElementPrivate.h"
#include "WebKitDOMNodeFilterPrivate.h"
#include "WebKitDOMNodeIteratorPrivate.h"
#include "WebKitDOMNodeListPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMProcessingInstructionPrivate.h"
#include "WebKitDOMRangePrivate.h"
#include "WebKitDOMStyleSheetListPrivate.h"
#include "WebKitDOMTextPrivate.h"
#include "WebKitDOMTreeWalkerPrivate.h"
#include "WebKitDOMXPathExpressionPrivate.h"
#include "WebKitDOMXPathNSResolverPrivate.h"
#include "WebKitDOMXPathResultPrivate.h"
#include <WebCore/CSSImportRule.h>
#include <WebCore/DOMException.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/FullscreenManager.h>
#include <WebCore/JSExecState.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

static gboolean webkit_dom_document_dispatch_event(WebKitDOMEventTarget* target, WebKitDOMEvent* event, GError** error)
{
    WebCore::Event* coreEvent = WebKit::core(event);
    if (!coreEvent)
        return false;
    WebCore::Document* coreTarget = static_cast<WebCore::Document*>(WEBKIT_DOM_OBJECT(target)->coreObject);

    auto result = coreTarget->dispatchEventForBindings(*coreEvent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

static gboolean webkit_dom_document_add_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::Document* coreTarget = static_cast<WebCore::Document*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::addEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

static gboolean webkit_dom_document_remove_event_listener(WebKitDOMEventTarget* target, const char* eventName, GClosure* handler, gboolean useCapture)
{
    WebCore::Document* coreTarget = static_cast<WebCore::Document*>(WEBKIT_DOM_OBJECT(target)->coreObject);
    return WebKit::GObjectEventListener::removeEventListener(G_OBJECT(target), coreTarget, eventName, handler, useCapture);
}

void webkitDOMDocumentDOMEventTargetInit(WebKitDOMEventTargetIface* iface)
{
    iface->dispatch_event = webkit_dom_document_dispatch_event;
    iface->add_event_listener = webkit_dom_document_add_event_listener;
    iface->remove_event_listener = webkit_dom_document_remove_event_listener;
}

enum {
    DOM_DOCUMENT_PROP_0,
    DOM_DOCUMENT_PROP_DOCTYPE,
    DOM_DOCUMENT_PROP_IMPLEMENTATION,
    DOM_DOCUMENT_PROP_DOCUMENT_ELEMENT,
    DOM_DOCUMENT_PROP_INPUT_ENCODING,
    DOM_DOCUMENT_PROP_XML_ENCODING,
    DOM_DOCUMENT_PROP_XML_VERSION,
    DOM_DOCUMENT_PROP_XML_STANDALONE,
    DOM_DOCUMENT_PROP_DOCUMENT_URI,
    DOM_DOCUMENT_PROP_DEFAULT_VIEW,
    DOM_DOCUMENT_PROP_STYLE_SHEETS,
    DOM_DOCUMENT_PROP_CONTENT_TYPE,
    DOM_DOCUMENT_PROP_TITLE,
    DOM_DOCUMENT_PROP_DIR,
    DOM_DOCUMENT_PROP_DESIGN_MODE,
    DOM_DOCUMENT_PROP_REFERRER,
    DOM_DOCUMENT_PROP_DOMAIN,
    DOM_DOCUMENT_PROP_URL,
    DOM_DOCUMENT_PROP_COOKIE,
    DOM_DOCUMENT_PROP_BODY,
    DOM_DOCUMENT_PROP_HEAD,
    DOM_DOCUMENT_PROP_IMAGES,
    DOM_DOCUMENT_PROP_APPLETS,
    DOM_DOCUMENT_PROP_LINKS,
    DOM_DOCUMENT_PROP_FORMS,
    DOM_DOCUMENT_PROP_ANCHORS,
    DOM_DOCUMENT_PROP_EMBEDS,
    DOM_DOCUMENT_PROP_PLUGINS,
    DOM_DOCUMENT_PROP_SCRIPTS,
    DOM_DOCUMENT_PROP_LAST_MODIFIED,
    DOM_DOCUMENT_PROP_CHARSET,
    DOM_DOCUMENT_PROP_READY_STATE,
    DOM_DOCUMENT_PROP_CHARACTER_SET,
    DOM_DOCUMENT_PROP_PREFERRED_STYLESHEET_SET,
    DOM_DOCUMENT_PROP_SELECTED_STYLESHEET_SET,
    DOM_DOCUMENT_PROP_ACTIVE_ELEMENT,
    DOM_DOCUMENT_PROP_COMPAT_MODE,
    DOM_DOCUMENT_PROP_WEBKIT_IS_FULL_SCREEN,
    DOM_DOCUMENT_PROP_WEBKIT_FULL_SCREEN_KEYBOARD_INPUT_ALLOWED,
    DOM_DOCUMENT_PROP_WEBKIT_CURRENT_FULL_SCREEN_ELEMENT,
    DOM_DOCUMENT_PROP_WEBKIT_FULLSCREEN_ENABLED,
    DOM_DOCUMENT_PROP_WEBKIT_FULLSCREEN_ELEMENT,
    DOM_DOCUMENT_PROP_POINTER_LOCK_ELEMENT,
    DOM_DOCUMENT_PROP_VISIBILITY_STATE,
    DOM_DOCUMENT_PROP_HIDDEN,
    DOM_DOCUMENT_PROP_CURRENT_SCRIPT,
    DOM_DOCUMENT_PROP_ORIGIN,
    DOM_DOCUMENT_PROP_SCROLLING_ELEMENT,
    DOM_DOCUMENT_PROP_CHILDREN,
    DOM_DOCUMENT_PROP_FIRST_ELEMENT_CHILD,
    DOM_DOCUMENT_PROP_LAST_ELEMENT_CHILD,
    DOM_DOCUMENT_PROP_CHILD_ELEMENT_COUNT,
};

static void webkit_dom_document_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMDocument* self = WEBKIT_DOM_DOCUMENT(object);

    switch (propertyId) {
    case DOM_DOCUMENT_PROP_XML_VERSION:
        webkit_dom_document_set_xml_version(self, g_value_get_string(value), nullptr);
        break;
    case DOM_DOCUMENT_PROP_XML_STANDALONE:
        webkit_dom_document_set_xml_standalone(self, g_value_get_boolean(value), nullptr);
        break;
    case DOM_DOCUMENT_PROP_DOCUMENT_URI:
        webkit_dom_document_set_document_uri(self, g_value_get_string(value));
        break;
    case DOM_DOCUMENT_PROP_TITLE:
        webkit_dom_document_set_title(self, g_value_get_string(value));
        break;
    case DOM_DOCUMENT_PROP_DIR:
        webkit_dom_document_set_dir(self, g_value_get_string(value));
        break;
    case DOM_DOCUMENT_PROP_DESIGN_MODE:
        webkit_dom_document_set_design_mode(self, g_value_get_string(value));
        break;
    case DOM_DOCUMENT_PROP_COOKIE:
        webkit_dom_document_set_cookie(self, g_value_get_string(value), nullptr);
        break;
    case DOM_DOCUMENT_PROP_CHARSET:
        webkit_dom_document_set_charset(self, g_value_get_string(value));
        break;
    case DOM_DOCUMENT_PROP_SELECTED_STYLESHEET_SET:
        g_warning("%s: The selected-stylesheet-set property has been removed and no longer works.", __func__);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_document_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMDocument* self = WEBKIT_DOM_DOCUMENT(object);

    switch (propertyId) {
    case DOM_DOCUMENT_PROP_DOCTYPE:
        g_value_set_object(value, webkit_dom_document_get_doctype(self));
        break;
    case DOM_DOCUMENT_PROP_IMPLEMENTATION:
        g_value_set_object(value, webkit_dom_document_get_implementation(self));
        break;
    case DOM_DOCUMENT_PROP_DOCUMENT_ELEMENT:
        g_value_set_object(value, webkit_dom_document_get_document_element(self));
        break;
    case DOM_DOCUMENT_PROP_INPUT_ENCODING:
        g_value_take_string(value, webkit_dom_document_get_input_encoding(self));
        break;
    case DOM_DOCUMENT_PROP_XML_ENCODING:
        g_value_take_string(value, webkit_dom_document_get_xml_encoding(self));
        break;
    case DOM_DOCUMENT_PROP_XML_VERSION:
        g_value_take_string(value, webkit_dom_document_get_xml_version(self));
        break;
    case DOM_DOCUMENT_PROP_XML_STANDALONE:
        g_value_set_boolean(value, webkit_dom_document_get_xml_standalone(self));
        break;
    case DOM_DOCUMENT_PROP_DOCUMENT_URI:
        g_value_take_string(value, webkit_dom_document_get_document_uri(self));
        break;
    case DOM_DOCUMENT_PROP_DEFAULT_VIEW:
        g_value_set_object(value, webkit_dom_document_get_default_view(self));
        break;
    case DOM_DOCUMENT_PROP_STYLE_SHEETS:
        g_value_set_object(value, webkit_dom_document_get_style_sheets(self));
        break;
    case DOM_DOCUMENT_PROP_CONTENT_TYPE:
        g_value_take_string(value, webkit_dom_document_get_content_type(self));
        break;
    case DOM_DOCUMENT_PROP_TITLE:
        g_value_take_string(value, webkit_dom_document_get_title(self));
        break;
    case DOM_DOCUMENT_PROP_DIR:
        g_value_take_string(value, webkit_dom_document_get_dir(self));
        break;
    case DOM_DOCUMENT_PROP_DESIGN_MODE:
        g_value_take_string(value, webkit_dom_document_get_design_mode(self));
        break;
    case DOM_DOCUMENT_PROP_REFERRER:
        g_value_take_string(value, webkit_dom_document_get_referrer(self));
        break;
    case DOM_DOCUMENT_PROP_DOMAIN:
        g_value_take_string(value, webkit_dom_document_get_domain(self));
        break;
    case DOM_DOCUMENT_PROP_URL:
        g_value_take_string(value, webkit_dom_document_get_url(self));
        break;
    case DOM_DOCUMENT_PROP_COOKIE:
        g_value_take_string(value, webkit_dom_document_get_cookie(self, nullptr));
        break;
    case DOM_DOCUMENT_PROP_BODY:
        g_value_set_object(value, webkit_dom_document_get_body(self));
        break;
    case DOM_DOCUMENT_PROP_HEAD:
        g_value_set_object(value, webkit_dom_document_get_head(self));
        break;
    case DOM_DOCUMENT_PROP_IMAGES:
        g_value_set_object(value, webkit_dom_document_get_images(self));
        break;
    case DOM_DOCUMENT_PROP_APPLETS:
        g_value_set_object(value, webkit_dom_document_get_applets(self));
        break;
    case DOM_DOCUMENT_PROP_LINKS:
        g_value_set_object(value, webkit_dom_document_get_links(self));
        break;
    case DOM_DOCUMENT_PROP_FORMS:
        g_value_set_object(value, webkit_dom_document_get_forms(self));
        break;
    case DOM_DOCUMENT_PROP_ANCHORS:
        g_value_set_object(value, webkit_dom_document_get_anchors(self));
        break;
    case DOM_DOCUMENT_PROP_EMBEDS:
        g_value_set_object(value, webkit_dom_document_get_embeds(self));
        break;
    case DOM_DOCUMENT_PROP_PLUGINS:
        g_value_set_object(value, webkit_dom_document_get_plugins(self));
        break;
    case DOM_DOCUMENT_PROP_SCRIPTS:
        g_value_set_object(value, webkit_dom_document_get_scripts(self));
        break;
    case DOM_DOCUMENT_PROP_LAST_MODIFIED:
        g_value_take_string(value, webkit_dom_document_get_last_modified(self));
        break;
    case DOM_DOCUMENT_PROP_CHARSET:
        g_value_take_string(value, webkit_dom_document_get_charset(self));
        break;
    case DOM_DOCUMENT_PROP_READY_STATE:
        g_value_take_string(value, webkit_dom_document_get_ready_state(self));
        break;
    case DOM_DOCUMENT_PROP_CHARACTER_SET:
        g_value_take_string(value, webkit_dom_document_get_character_set(self));
        break;
    case DOM_DOCUMENT_PROP_PREFERRED_STYLESHEET_SET:
        g_warning("%s: The preferred-stylesheet-set property has been removed and no longer works.", __func__);
        break;
    case DOM_DOCUMENT_PROP_SELECTED_STYLESHEET_SET:
        g_warning("%s: The selected-stylesheet-set property has been removed and no longer works.", __func__);
        break;
    case DOM_DOCUMENT_PROP_ACTIVE_ELEMENT:
        g_value_set_object(value, webkit_dom_document_get_active_element(self));
        break;
    case DOM_DOCUMENT_PROP_COMPAT_MODE:
        g_value_take_string(value, webkit_dom_document_get_compat_mode(self));
        break;
    case DOM_DOCUMENT_PROP_WEBKIT_IS_FULL_SCREEN:
        g_value_set_boolean(value, webkit_dom_document_get_webkit_is_fullscreen(self));
        break;
    case DOM_DOCUMENT_PROP_WEBKIT_FULL_SCREEN_KEYBOARD_INPUT_ALLOWED:
        g_value_set_boolean(value, webkit_dom_document_get_webkit_fullscreen_keyboard_input_allowed(self));
        break;
    case DOM_DOCUMENT_PROP_WEBKIT_CURRENT_FULL_SCREEN_ELEMENT:
        g_value_set_object(value, webkit_dom_document_get_webkit_current_fullscreen_element(self));
        break;
    case DOM_DOCUMENT_PROP_WEBKIT_FULLSCREEN_ENABLED:
        g_value_set_boolean(value, webkit_dom_document_get_webkit_fullscreen_enabled(self));
        break;
    case DOM_DOCUMENT_PROP_WEBKIT_FULLSCREEN_ELEMENT:
        g_value_set_object(value, webkit_dom_document_get_webkit_fullscreen_element(self));
        break;
    case DOM_DOCUMENT_PROP_POINTER_LOCK_ELEMENT:
        g_value_set_object(value, webkit_dom_document_get_pointer_lock_element(self));
        break;
    case DOM_DOCUMENT_PROP_VISIBILITY_STATE:
        g_value_take_string(value, webkit_dom_document_get_visibility_state(self));
        break;
    case DOM_DOCUMENT_PROP_HIDDEN:
        g_value_set_boolean(value, webkit_dom_document_get_hidden(self));
        break;
    case DOM_DOCUMENT_PROP_CURRENT_SCRIPT:
        g_value_set_object(value, webkit_dom_document_get_current_script(self));
        break;
    case DOM_DOCUMENT_PROP_ORIGIN:
        g_value_take_string(value, webkit_dom_document_get_origin(self));
        break;
    case DOM_DOCUMENT_PROP_SCROLLING_ELEMENT:
        g_value_set_object(value, webkit_dom_document_get_scrolling_element(self));
        break;
    case DOM_DOCUMENT_PROP_CHILDREN:
        g_value_set_object(value, webkit_dom_document_get_children(self));
        break;
    case DOM_DOCUMENT_PROP_FIRST_ELEMENT_CHILD:
        g_value_set_object(value, webkit_dom_document_get_first_element_child(self));
        break;
    case DOM_DOCUMENT_PROP_LAST_ELEMENT_CHILD:
        g_value_set_object(value, webkit_dom_document_get_last_element_child(self));
        break;
    case DOM_DOCUMENT_PROP_CHILD_ELEMENT_COUNT:
        g_value_set_ulong(value, webkit_dom_document_get_child_element_count(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

void webkitDOMDocumentInstallProperties(GObjectClass* gobjectClass)
{
    gobjectClass->set_property = webkit_dom_document_set_property;
    gobjectClass->get_property = webkit_dom_document_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_DOCTYPE,
        g_param_spec_object(
            "doctype",
            "Document:doctype",
            "read-only WebKitDOMDocumentType* Document:doctype",
            WEBKIT_DOM_TYPE_DOCUMENT_TYPE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_IMPLEMENTATION,
        g_param_spec_object(
            "implementation",
            "Document:implementation",
            "read-only WebKitDOMDOMImplementation* Document:implementation",
            WEBKIT_DOM_TYPE_DOM_IMPLEMENTATION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_DOCUMENT_ELEMENT,
        g_param_spec_object(
            "document-element",
            "Document:document-element",
            "read-only WebKitDOMElement* Document:document-element",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_INPUT_ENCODING,
        g_param_spec_string(
            "input-encoding",
            "Document:input-encoding",
            "read-only gchar* Document:input-encoding",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_XML_ENCODING,
        g_param_spec_string(
            "xml-encoding",
            "Document:xml-encoding",
            "read-only gchar* Document:xml-encoding",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_XML_VERSION,
        g_param_spec_string(
            "xml-version",
            "Document:xml-version",
            "read-write gchar* Document:xml-version",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_XML_STANDALONE,
        g_param_spec_boolean(
            "xml-standalone",
            "Document:xml-standalone",
            "read-write gboolean Document:xml-standalone",
            FALSE,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_DOCUMENT_URI,
        g_param_spec_string(
            "document-uri",
            "Document:document-uri",
            "read-write gchar* Document:document-uri",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_DEFAULT_VIEW,
        g_param_spec_object(
            "default-view",
            "Document:default-view",
            "read-only WebKitDOMDOMWindow* Document:default-view",
            WEBKIT_DOM_TYPE_DOM_WINDOW,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_STYLE_SHEETS,
        g_param_spec_object(
            "style-sheets",
            "Document:style-sheets",
            "read-only WebKitDOMStyleSheetList* Document:style-sheets",
            WEBKIT_DOM_TYPE_STYLE_SHEET_LIST,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_CONTENT_TYPE,
        g_param_spec_string(
            "content-type",
            "Document:content-type",
            "read-only gchar* Document:content-type",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_TITLE,
        g_param_spec_string(
            "title",
            "Document:title",
            "read-write gchar* Document:title",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_DIR,
        g_param_spec_string(
            "dir",
            "Document:dir",
            "read-write gchar* Document:dir",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_DESIGN_MODE,
        g_param_spec_string(
            "design-mode",
            "Document:design-mode",
            "read-write gchar* Document:design-mode",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_REFERRER,
        g_param_spec_string(
            "referrer",
            "Document:referrer",
            "read-only gchar* Document:referrer",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_DOMAIN,
        g_param_spec_string(
            "domain",
            "Document:domain",
            "read-only gchar* Document:domain",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_URL,
        g_param_spec_string(
            "url",
            "Document:url",
            "read-only gchar* Document:url",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_COOKIE,
        g_param_spec_string(
            "cookie",
            "Document:cookie",
            "read-write gchar* Document:cookie",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_BODY,
        g_param_spec_object(
            "body",
            "Document:body",
            "read-only WebKitDOMHTMLElement* Document:body",
            WEBKIT_DOM_TYPE_HTML_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_HEAD,
        g_param_spec_object(
            "head",
            "Document:head",
            "read-only WebKitDOMHTMLHeadElement* Document:head",
            WEBKIT_DOM_TYPE_HTML_HEAD_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_IMAGES,
        g_param_spec_object(
            "images",
            "Document:images",
            "read-only WebKitDOMHTMLCollection* Document:images",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_APPLETS,
        g_param_spec_object(
            "applets",
            "Document:applets",
            "read-only WebKitDOMHTMLCollection* Document:applets",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_LINKS,
        g_param_spec_object(
            "links",
            "Document:links",
            "read-only WebKitDOMHTMLCollection* Document:links",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_FORMS,
        g_param_spec_object(
            "forms",
            "Document:forms",
            "read-only WebKitDOMHTMLCollection* Document:forms",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_ANCHORS,
        g_param_spec_object(
            "anchors",
            "Document:anchors",
            "read-only WebKitDOMHTMLCollection* Document:anchors",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_EMBEDS,
        g_param_spec_object(
            "embeds",
            "Document:embeds",
            "read-only WebKitDOMHTMLCollection* Document:embeds",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_PLUGINS,
        g_param_spec_object(
            "plugins",
            "Document:plugins",
            "read-only WebKitDOMHTMLCollection* Document:plugins",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_SCRIPTS,
        g_param_spec_object(
            "scripts",
            "Document:scripts",
            "read-only WebKitDOMHTMLCollection* Document:scripts",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_LAST_MODIFIED,
        g_param_spec_string(
            "last-modified",
            "Document:last-modified",
            "read-only gchar* Document:last-modified",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_CHARSET,
        g_param_spec_string(
            "charset",
            "Document:charset",
            "read-write gchar* Document:charset",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_READY_STATE,
        g_param_spec_string(
            "ready-state",
            "Document:ready-state",
            "read-only gchar* Document:ready-state",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_CHARACTER_SET,
        g_param_spec_string(
            "character-set",
            "Document:character-set",
            "read-only gchar* Document:character-set",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_PREFERRED_STYLESHEET_SET,
        g_param_spec_string(
            "preferred-stylesheet-set",
            "Document:preferred-stylesheet-set",
            "read-only gchar* Document:preferred-stylesheet-set",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_SELECTED_STYLESHEET_SET,
        g_param_spec_string(
            "selected-stylesheet-set",
            "Document:selected-stylesheet-set",
            "read-write gchar* Document:selected-stylesheet-set",
            "",
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_ACTIVE_ELEMENT,
        g_param_spec_object(
            "active-element",
            "Document:active-element",
            "read-only WebKitDOMElement* Document:active-element",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_COMPAT_MODE,
        g_param_spec_string(
            "compat-mode",
            "Document:compat-mode",
            "read-only gchar* Document:compat-mode",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_WEBKIT_IS_FULL_SCREEN,
        g_param_spec_boolean(
            "webkit-is-full-screen",
            "Document:webkit-is-full-screen",
            "read-only gboolean Document:webkit-is-full-screen",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_WEBKIT_FULL_SCREEN_KEYBOARD_INPUT_ALLOWED,
        g_param_spec_boolean(
            "webkit-full-screen-keyboard-input-allowed",
            "Document:webkit-full-screen-keyboard-input-allowed",
            "read-only gboolean Document:webkit-full-screen-keyboard-input-allowed",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_WEBKIT_CURRENT_FULL_SCREEN_ELEMENT,
        g_param_spec_object(
            "webkit-current-full-screen-element",
            "Document:webkit-current-full-screen-element",
            "read-only WebKitDOMElement* Document:webkit-current-full-screen-element",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_WEBKIT_FULLSCREEN_ENABLED,
        g_param_spec_boolean(
            "webkit-fullscreen-enabled",
            "Document:webkit-fullscreen-enabled",
            "read-only gboolean Document:webkit-fullscreen-enabled",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_WEBKIT_FULLSCREEN_ELEMENT,
        g_param_spec_object(
            "webkit-fullscreen-element",
            "Document:webkit-fullscreen-element",
            "read-only WebKitDOMElement* Document:webkit-fullscreen-element",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_POINTER_LOCK_ELEMENT,
        g_param_spec_object(
            "pointer-lock-element",
            "Document:pointer-lock-element",
            "read-only WebKitDOMElement* Document:pointer-lock-element",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_VISIBILITY_STATE,
        g_param_spec_string(
            "visibility-state",
            "Document:visibility-state",
            "read-only gchar* Document:visibility-state",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_HIDDEN,
        g_param_spec_boolean(
            "hidden",
            "Document:hidden",
            "read-only gboolean Document:hidden",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_CURRENT_SCRIPT,
        g_param_spec_object(
            "current-script",
            "Document:current-script",
            "read-only WebKitDOMHTMLScriptElement* Document:current-script",
            WEBKIT_DOM_TYPE_HTML_SCRIPT_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_ORIGIN,
        g_param_spec_string(
            "origin",
            "Document:origin",
            "read-only gchar* Document:origin",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_SCROLLING_ELEMENT,
        g_param_spec_object(
            "scrolling-element",
            "Document:scrolling-element",
            "read-only WebKitDOMElement* Document:scrolling-element",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_CHILDREN,
        g_param_spec_object(
            "children",
            "Document:children",
            "read-only WebKitDOMHTMLCollection* Document:children",
            WEBKIT_DOM_TYPE_HTML_COLLECTION,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_FIRST_ELEMENT_CHILD,
        g_param_spec_object(
            "first-element-child",
            "Document:first-element-child",
            "read-only WebKitDOMElement* Document:first-element-child",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_LAST_ELEMENT_CHILD,
        g_param_spec_object(
            "last-element-child",
            "Document:last-element-child",
            "read-only WebKitDOMElement* Document:last-element-child",
            WEBKIT_DOM_TYPE_ELEMENT,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOCUMENT_PROP_CHILD_ELEMENT_COUNT,
        g_param_spec_ulong(
            "child-element-count",
            "Document:child-element-count",
            "read-only gulong Document:child-element-count",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

}

WebKitDOMElement* webkit_dom_document_create_element(WebKitDOMDocument* self, const gchar* tagName, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(tagName, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    auto result = item->createElementForBindings(WTF::AtomString::fromUTF8(tagName));
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMDocumentFragment* webkit_dom_document_create_document_fragment(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::DocumentFragment> gobjectResult = WTF::getPtr(item->createDocumentFragment());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMText* webkit_dom_document_create_text_node(WebKitDOMDocument* self, const gchar* data)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(data, 0);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedData = WTF::String::fromUTF8(data);
    RefPtr<WebCore::Text> gobjectResult = WTF::getPtr(item->createTextNode(WTFMove(convertedData)));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMComment* webkit_dom_document_create_comment(WebKitDOMDocument* self, const gchar* data)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(data, 0);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedData = WTF::String::fromUTF8(data);
    RefPtr<WebCore::Comment> gobjectResult = WTF::getPtr(item->createComment(WTFMove(convertedData)));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMCDATASection* webkit_dom_document_create_cdata_section(WebKitDOMDocument* self, const gchar* data, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(data, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    auto result = item->createCDATASection(WTF::String::fromUTF8(data));
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMProcessingInstruction* webkit_dom_document_create_processing_instruction(WebKitDOMDocument* self, const gchar* target, const gchar* data, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(target, 0);
    g_return_val_if_fail(data, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    auto result = item->createProcessingInstruction(WTF::String::fromUTF8(target), WTF::String::fromUTF8(data));
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMAttr* webkit_dom_document_create_attribute(WebKitDOMDocument* self, const gchar* name, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(name, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    auto result = item->createAttribute(WTF::AtomString::fromUTF8(name));
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMHTMLCollection* webkit_dom_document_get_elements_by_tag_name_as_html_collection(WebKitDOMDocument* self, const gchar* tagname)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(tagname, 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->getElementsByTagName(WTF::AtomString::fromUTF8(tagname)));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_document_import_node(WebKitDOMDocument* self, WebKitDOMNode* importedNode, gboolean deep, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(importedNode), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    WebCore::Node* convertedImportedNode = WebKit::core(importedNode);
    auto result = item->importNode(*convertedImportedNode, deep);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMElement* webkit_dom_document_create_element_ns(WebKitDOMDocument* self, const gchar* namespaceURI, const gchar* qualifiedName, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(qualifiedName, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    auto convertedNamespaceURI = WTF::AtomString::fromUTF8(namespaceURI);
    auto convertedQualifiedName = WTF::AtomString::fromUTF8(qualifiedName);
    auto result = item->createElementNS(convertedNamespaceURI, convertedQualifiedName);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMAttr* webkit_dom_document_create_attribute_ns(WebKitDOMDocument* self, const gchar* namespaceURI, const gchar* qualifiedName, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(qualifiedName, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    auto convertedNamespaceURI = WTF::AtomString::fromUTF8(namespaceURI);
    auto convertedQualifiedName = WTF::AtomString::fromUTF8(qualifiedName);
    auto result = item->createAttributeNS(convertedNamespaceURI, convertedQualifiedName);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMHTMLCollection* webkit_dom_document_get_elements_by_tag_name_ns_as_html_collection(WebKitDOMDocument* self, const gchar* namespaceURI, const gchar* localName)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(namespaceURI, 0);
    g_return_val_if_fail(localName, 0);
    WebCore::Document* item = WebKit::core(self);
    auto convertedNamespaceURI = WTF::AtomString::fromUTF8(namespaceURI);
    auto convertedLocalName = WTF::AtomString::fromUTF8(localName);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->getElementsByTagNameNS(convertedNamespaceURI, convertedLocalName));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNode* webkit_dom_document_adopt_node(WebKitDOMDocument* self, WebKitDOMNode* source, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(source), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    WebCore::Node* convertedSource = WebKit::core(source);
    auto result = item->adoptNode(*convertedSource);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMEvent* webkit_dom_document_create_event(WebKitDOMDocument* self, const gchar* eventType, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(eventType, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedEventType = WTF::String::fromUTF8(eventType);
    auto result = item->createEvent(convertedEventType);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMRange* webkit_dom_document_create_range(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::Range> gobjectResult = WTF::getPtr(item->createRange());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMNodeIterator* webkit_dom_document_create_node_iterator(WebKitDOMDocument* self, WebKitDOMNode* root, gulong whatToShow, WebKitDOMNodeFilter* filter, gboolean expandEntityReferences, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(root), 0);
    g_return_val_if_fail(!filter || WEBKIT_DOM_IS_NODE_FILTER(filter), 0);
    UNUSED_PARAM(error);
    WebCore::Document* item = WebKit::core(self);
    WebCore::Node* convertedRoot = WebKit::core(root);
    RefPtr<WebCore::NodeFilter> convertedFilter = WebKit::core(item, filter);
    RefPtr<WebCore::NodeIterator> gobjectResult = WTF::getPtr(item->createNodeIterator(*convertedRoot, whatToShow, WTF::getPtr(convertedFilter), expandEntityReferences));
IGNORE_GCC_WARNINGS_BEGIN("use-after-free")
    return WebKit::kit(gobjectResult.get());
IGNORE_GCC_WARNINGS_END
}

WebKitDOMTreeWalker* webkit_dom_document_create_tree_walker(WebKitDOMDocument* self, WebKitDOMNode* root, gulong whatToShow, WebKitDOMNodeFilter* filter, gboolean expandEntityReferences, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(root), 0);
    g_return_val_if_fail(!filter || WEBKIT_DOM_IS_NODE_FILTER(filter), 0);
    UNUSED_PARAM(error);
    WebCore::Document* item = WebKit::core(self);
    WebCore::Node* convertedRoot = WebKit::core(root);
    RefPtr<WebCore::NodeFilter> convertedFilter = WebKit::core(item, filter);
    RefPtr<WebCore::TreeWalker> gobjectResult = WTF::getPtr(item->createTreeWalker(*convertedRoot, whatToShow, WTF::getPtr(convertedFilter), expandEntityReferences));
IGNORE_GCC_WARNINGS_BEGIN("use-after-free")
    return WebKit::kit(gobjectResult.get());
IGNORE_GCC_WARNINGS_END
}

WebKitDOMCSSStyleDeclaration* webkit_dom_document_get_override_style(WebKitDOMDocument*, WebKitDOMElement*, const gchar*)
{
    return nullptr;
}

WebKitDOMXPathExpression* webkit_dom_document_create_expression(WebKitDOMDocument* self, const gchar* expression, WebKitDOMXPathNSResolver* resolver, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(expression, 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_XPATH_NS_RESOLVER(resolver), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedExpression = WTF::String::fromUTF8(expression);
    RefPtr<WebCore::XPathNSResolver> convertedResolver = WebKit::core(resolver);
    auto result = item->createExpression(convertedExpression, WTFMove(convertedResolver));
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMXPathNSResolver* webkit_dom_document_create_ns_resolver(WebKitDOMDocument* self, WebKitDOMNode* nodeResolver)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(nodeResolver), 0);
    WebCore::Document* item = WebKit::core(self);
    WebCore::Node* convertedNodeResolver = WebKit::core(nodeResolver);
    if (!convertedNodeResolver)
        return nullptr;
    RefPtr<WebCore::XPathNSResolver> gobjectResult = WTF::getPtr(item->createNSResolver(*convertedNodeResolver));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMXPathResult* webkit_dom_document_evaluate(WebKitDOMDocument* self, const gchar* expression, WebKitDOMNode* contextNode, WebKitDOMXPathNSResolver* resolver, gushort type, WebKitDOMXPathResult* inResult, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(expression, 0);
    g_return_val_if_fail(contextNode && WEBKIT_DOM_IS_NODE(contextNode), 0);
    g_return_val_if_fail(!resolver || WEBKIT_DOM_IS_XPATH_NS_RESOLVER(resolver), 0);
    g_return_val_if_fail(!inResult || WEBKIT_DOM_IS_XPATH_RESULT(inResult), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedExpression = WTF::String::fromUTF8(expression);
    WebCore::Node* convertedContextNode = WebKit::core(contextNode);
    RefPtr<WebCore::XPathNSResolver> convertedResolver = WebKit::core(resolver);
    WebCore::XPathResult* convertedInResult = WebKit::core(inResult);
    auto result = item->evaluate(convertedExpression, *convertedContextNode, WTFMove(convertedResolver), type, convertedInResult);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

gboolean webkit_dom_document_exec_command(WebKitDOMDocument* self, const gchar* command, gboolean userInterface, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), FALSE);
    g_return_val_if_fail(command, FALSE);
    g_return_val_if_fail(value, FALSE);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedCommand = WTF::String::fromUTF8(command);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    auto result = item->execCommand(convertedCommand, userInterface, convertedValue);
    return result.hasException() ? false : result.returnValue();
}

gboolean webkit_dom_document_query_command_enabled(WebKitDOMDocument* self, const gchar* command)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), FALSE);
    g_return_val_if_fail(command, FALSE);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedCommand = WTF::String::fromUTF8(command);
    auto result = item->queryCommandEnabled(convertedCommand);
    return result.hasException() ? false : result.returnValue();
}

gboolean webkit_dom_document_query_command_indeterm(WebKitDOMDocument* self, const gchar* command)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), FALSE);
    g_return_val_if_fail(command, FALSE);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedCommand = WTF::String::fromUTF8(command);
    auto result = item->queryCommandIndeterm(convertedCommand);
    return result.hasException() ? false : result.returnValue();
}

gboolean webkit_dom_document_query_command_state(WebKitDOMDocument* self, const gchar* command)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), FALSE);
    g_return_val_if_fail(command, FALSE);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedCommand = WTF::String::fromUTF8(command);
    auto result = item->queryCommandState(convertedCommand);
    return result.hasException() ? false : result.returnValue();
}

gboolean webkit_dom_document_query_command_supported(WebKitDOMDocument* self, const gchar* command)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), FALSE);
    g_return_val_if_fail(command, FALSE);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedCommand = WTF::String::fromUTF8(command);
    auto result = item->queryCommandSupported(convertedCommand);
    return result.hasException() ? false : result.returnValue();
}

gchar* webkit_dom_document_query_command_value(WebKitDOMDocument* self, const gchar* command)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(command, 0);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedCommand = WTF::String::fromUTF8(command);
    auto stringValue = item->queryCommandValue(convertedCommand);
    gchar* result = convertToUTF8String(stringValue.hasException() ? String() : stringValue.returnValue());
    return result;
}

WebKitDOMNodeList* webkit_dom_document_get_elements_by_name(WebKitDOMDocument* self, const gchar* elementName)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(elementName, 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::NodeList> gobjectResult = WTF::getPtr(item->getElementsByName(WTF::AtomString::fromUTF8(elementName)));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMElement* webkit_dom_document_element_from_point(WebKitDOMDocument* self, glong x, glong y)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->elementFromPoint(x, y));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMRange* webkit_dom_document_caret_range_from_point(WebKitDOMDocument* self, glong x, glong y)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::Range> gobjectResult = WTF::getPtr(item->caretRangeFromPoint(x, y));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMCSSStyleDeclaration* webkit_dom_document_create_css_style_declaration(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::CSSStyleDeclaration> gobjectResult = WTF::getPtr(item->createCSSStyleDeclaration());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMHTMLCollection* webkit_dom_document_get_elements_by_class_name_as_html_collection(WebKitDOMDocument* self, const gchar* classNames)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(classNames, 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->getElementsByClassName(WTF::AtomString::fromUTF8(classNames)));
    return WebKit::kit(gobjectResult.get());
}

gboolean webkit_dom_document_has_focus(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), FALSE);
    WebCore::Document* item = WebKit::core(self);
    gboolean result = item->hasFocus();
    return result;
}

void webkit_dom_document_webkit_cancel_fullscreen(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOCUMENT(self));
#if ENABLE(FULLSCREEN_API)
    WebCore::Document* item = WebKit::core(self);
    item->fullscreenManager().cancelFullscreen();
#endif
}

void webkit_dom_document_webkit_exit_fullscreen(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOCUMENT(self));
#if ENABLE(FULLSCREEN_API)
    WebCore::Document* item = WebKit::core(self);
    item->fullscreenManager().exitFullscreen(nullptr);
#endif
}

void webkit_dom_document_exit_pointer_lock(WebKitDOMDocument* self)
{
#if ENABLE(POINTER_LOCK)
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOCUMENT(self));
    WebCore::Document* item = WebKit::core(self);
    item->exitPointerLock();
#else
    UNUSED_PARAM(self);
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Pointer Lock")
#endif /* ENABLE(POINTER_LOCK) */
}

WebKitDOMElement* webkit_dom_document_get_element_by_id(WebKitDOMDocument* self, const gchar* elementId)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(elementId, 0);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedElementId = WTF::String::fromUTF8(elementId);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->getElementById(convertedElementId));
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMElement* webkit_dom_document_query_selector(WebKitDOMDocument* self, const gchar* selectors, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(selectors, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedSelectors = WTF::String::fromUTF8(selectors);
    auto result = item->querySelector(convertedSelectors);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue());
}

WebKitDOMNodeList* webkit_dom_document_query_selector_all(WebKitDOMDocument* self, const gchar* selectors, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(selectors, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedSelectors = WTF::String::fromUTF8(selectors);
    auto result = item->querySelectorAll(convertedSelectors);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMDocumentType* webkit_dom_document_get_doctype(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::DocumentType> gobjectResult = WTF::getPtr(item->doctype());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMDOMImplementation* webkit_dom_document_get_implementation(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::DOMImplementation> gobjectResult = WTF::getPtr(item->implementation());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMElement* webkit_dom_document_get_document_element(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->documentElement());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_document_get_input_encoding(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->characterSetWithUTF8Fallback());
    return result;
}

gchar* webkit_dom_document_get_xml_encoding(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->xmlEncoding());
    return result;
}

gchar* webkit_dom_document_get_xml_version(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->xmlVersion());
    return result;
}

void webkit_dom_document_set_xml_version(WebKitDOMDocument* self, const gchar* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOCUMENT(self));
    g_return_if_fail(value);
    g_return_if_fail(!error || !*error);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    auto result = item->setXMLVersion(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

gboolean webkit_dom_document_get_xml_standalone(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), FALSE);
    WebCore::Document* item = WebKit::core(self);
    gboolean result = item->xmlStandalone();
    return result;
}

void webkit_dom_document_set_xml_standalone(WebKitDOMDocument* self, gboolean value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOCUMENT(self));
    g_return_if_fail(!error || !*error);
    WebKit::core(self)->setXMLStandalone(value);
}

gchar* webkit_dom_document_get_document_uri(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->documentURI());
    return result;
}

void webkit_dom_document_set_document_uri(WebKitDOMDocument* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOCUMENT(self));
    g_return_if_fail(value);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setDocumentURI(convertedValue);
}

WebKitDOMDOMWindow* webkit_dom_document_get_default_view(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    return WebKit::kit(item->windowProxy());
}

WebKitDOMStyleSheetList* webkit_dom_document_get_style_sheets(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::StyleSheetList> gobjectResult = WTF::getPtr(item->styleSheets());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_document_get_content_type(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->contentType());
    return result;
}

gchar* webkit_dom_document_get_title(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->title());
    return result;
}

void webkit_dom_document_set_title(WebKitDOMDocument* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOCUMENT(self));
    g_return_if_fail(value);
    WebCore::Document* item = WebKit::core(self);
    item->setTitle(WTF::String::fromUTF8(value));
}

gchar* webkit_dom_document_get_dir(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->dir());
    return result;
}

void webkit_dom_document_set_dir(WebKitDOMDocument* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOCUMENT(self));
    g_return_if_fail(value);
    WebCore::Document* item = WebKit::core(self);
    item->setDir(WTF::AtomString::fromUTF8(value));
}

gchar* webkit_dom_document_get_design_mode(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->designMode());
    return result;
}

void webkit_dom_document_set_design_mode(WebKitDOMDocument* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOCUMENT(self));
    g_return_if_fail(value);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setDesignMode(convertedValue);
}

gchar* webkit_dom_document_get_referrer(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->referrer());
    return result;
}

gchar* webkit_dom_document_get_domain(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->domain());
    return result;
}

gchar* webkit_dom_document_get_url(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->urlForBindings());
    return result;
}

gchar* webkit_dom_document_get_cookie(WebKitDOMDocument* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Document* item = WebKit::core(self);
    auto result = item->cookie();
    if (result.hasException())
        return nullptr;
    return convertToUTF8String(result.releaseReturnValue());
}

void webkit_dom_document_set_cookie(WebKitDOMDocument* self, const gchar* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOCUMENT(self));
    g_return_if_fail(value);
    g_return_if_fail(!error || !*error);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    auto result = item->setCookie(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

WebKitDOMHTMLElement* webkit_dom_document_get_body(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::HTMLElement> gobjectResult = WTF::getPtr(item->bodyOrFrameset());
    return WebKit::kit(gobjectResult.get());
}

void webkit_dom_document_set_body(WebKitDOMDocument* self, WebKitDOMHTMLElement* value, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOCUMENT(self));
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(value));
    g_return_if_fail(!error || !*error);
    WebCore::Document* item = WebKit::core(self);
    WebCore::HTMLElement* convertedValue = WebKit::core(value);
    auto result = item->setBodyOrFrameset(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

WebKitDOMHTMLHeadElement* webkit_dom_document_get_head(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::HTMLHeadElement> gobjectResult = WTF::getPtr(item->head());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMHTMLCollection* webkit_dom_document_get_images(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->images());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMHTMLCollection* webkit_dom_document_get_applets(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->applets());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMHTMLCollection* webkit_dom_document_get_links(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->links());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMHTMLCollection* webkit_dom_document_get_forms(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->forms());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMHTMLCollection* webkit_dom_document_get_anchors(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->anchors());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMHTMLCollection* webkit_dom_document_get_embeds(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->embeds());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMHTMLCollection* webkit_dom_document_get_plugins(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->plugins());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMHTMLCollection* webkit_dom_document_get_scripts(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->scripts());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_document_get_last_modified(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->lastModified());
    return result;
}

gchar* webkit_dom_document_get_charset(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->charset());
    return result;
}

void webkit_dom_document_set_charset(WebKitDOMDocument* self, const gchar* value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOCUMENT(self));
    g_return_if_fail(value);
    WebCore::Document* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    item->setCharset(convertedValue);
}

gchar* webkit_dom_document_get_ready_state(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);

    auto readyState = WebKit::core(self)->readyState();
    switch (readyState) {
    case WebCore::Document::Loading:
        return convertToUTF8String("loading"_s);
    case WebCore::Document::Interactive:
        return convertToUTF8String("interactive"_s);
    case WebCore::Document::Complete:
        return convertToUTF8String("complete"_s);
    }
    return 0;
}

gchar* webkit_dom_document_get_character_set(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->characterSetWithUTF8Fallback());
    return result;
}

gchar* webkit_dom_document_get_preferred_stylesheet_set(WebKitDOMDocument* self)
{
    g_warning("%s: this function has been removed and does nothing", __func__);
    return nullptr;
}

gchar* webkit_dom_document_get_selected_stylesheet_set(WebKitDOMDocument* self)
{
    g_warning("%s: this function has been removed and does nothing", __func__);
    return nullptr;
}

void webkit_dom_document_set_selected_stylesheet_set(WebKitDOMDocument* self, const gchar* value)
{
    g_warning("%s: this function has been removed and does nothing", __func__);
}

WebKitDOMElement* webkit_dom_document_get_active_element(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->activeElement());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_document_get_compat_mode(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->compatMode());
    return result;
}

gboolean webkit_dom_document_get_webkit_is_fullscreen(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), FALSE);
#if ENABLE(FULLSCREEN_API)
    WebCore::Document* item = WebKit::core(self);
    gboolean result = item->fullscreenManager().isFullscreen();
    return result;
#else
    return FALSE;
#endif
}

gboolean webkit_dom_document_get_webkit_fullscreen_keyboard_input_allowed(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), FALSE);
#if ENABLE(FULLSCREEN_API)
    WebCore::Document* item = WebKit::core(self);
    gboolean result = item->fullscreenManager().isFullscreenKeyboardInputAllowed();
    return result;
#else
    return FALSE;
#endif
}

WebKitDOMElement* webkit_dom_document_get_webkit_current_fullscreen_element(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
#if ENABLE(FULLSCREEN_API)
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->fullscreenManager().currentFullscreenElement());
    return WebKit::kit(gobjectResult.get());
#else
    return NULL;
#endif
}

gboolean webkit_dom_document_get_webkit_fullscreen_enabled(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), FALSE);
#if ENABLE(FULLSCREEN_API)
    WebCore::Document* item = WebKit::core(self);
    gboolean result = item->fullscreenManager().isFullscreenEnabled();
    return result;
#else
    return FALSE;
#endif
}

WebKitDOMElement* webkit_dom_document_get_webkit_fullscreen_element(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
#if ENABLE(FULLSCREEN_API)
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->fullscreenManager().fullscreenElement());
    return WebKit::kit(gobjectResult.get());
#else
    return NULL;
#endif
}

WebKitDOMElement* webkit_dom_document_get_pointer_lock_element(WebKitDOMDocument* self)
{
#if ENABLE(POINTER_LOCK)
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->pointerLockElement());
    return WebKit::kit(gobjectResult.get());
#else
    UNUSED_PARAM(self);
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Pointer Lock")
    return 0;
#endif /* ENABLE(POINTER_LOCK) */
}

gchar* webkit_dom_document_get_visibility_state(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    switch (item->visibilityState()) {
    case WebCore::VisibilityState::Hidden:
        return convertToUTF8String("hidden"_s);
    case WebCore::VisibilityState::Visible:
        return convertToUTF8String("visible"_s);
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

gboolean webkit_dom_document_get_hidden(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), FALSE);
    WebCore::Document* item = WebKit::core(self);
    gboolean result = item->hidden();
    return result;
}

WebKitDOMHTMLScriptElement* webkit_dom_document_get_current_script(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    WebCore::Element* element = item->currentScript();
    if (!is<WebCore::HTMLScriptElement>(element))
        return nullptr;
    return WebKit::kit(downcast<WebCore::HTMLScriptElement>(element));
}

gchar* webkit_dom_document_get_origin(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->securityOrigin().toString());
    return result;
}

WebKitDOMElement* webkit_dom_document_get_scrolling_element(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->scrollingElementForAPI());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMHTMLCollection* webkit_dom_document_get_children(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::HTMLCollection> gobjectResult = WTF::getPtr(item->children());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMElement* webkit_dom_document_get_first_element_child(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->firstElementChild());
    return WebKit::kit(gobjectResult.get());
}

WebKitDOMElement* webkit_dom_document_get_last_element_child(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    RefPtr<WebCore::Element> gobjectResult = WTF::getPtr(item->lastElementChild());
    return WebKit::kit(gobjectResult.get());
}

gulong webkit_dom_document_get_child_element_count(WebKitDOMDocument* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), 0);
    WebCore::Document* item = WebKit::core(self);
    gulong result = item->childElementCount();
    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS;
