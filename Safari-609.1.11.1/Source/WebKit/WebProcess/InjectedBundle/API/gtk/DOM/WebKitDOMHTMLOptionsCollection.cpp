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
#include "WebKitDOMHTMLOptionsCollection.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/Document.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMHTMLCollectionPrivate.h"
#include "WebKitDOMHTMLOptionElementPrivate.h"
#include "WebKitDOMHTMLOptionsCollectionPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMHTMLOptionsCollection* kit(WebCore::HTMLOptionsCollection* obj)
{
    return WEBKIT_DOM_HTML_OPTIONS_COLLECTION(kit(static_cast<WebCore::HTMLCollection*>(obj)));
}

WebCore::HTMLOptionsCollection* core(WebKitDOMHTMLOptionsCollection* request)
{
    return request ? static_cast<WebCore::HTMLOptionsCollection*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMHTMLOptionsCollection* wrapHTMLOptionsCollection(WebCore::HTMLOptionsCollection* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_HTML_OPTIONS_COLLECTION(g_object_new(WEBKIT_DOM_TYPE_HTML_OPTIONS_COLLECTION, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMHTMLOptionsCollection, webkit_dom_html_options_collection, WEBKIT_DOM_TYPE_HTML_COLLECTION)

enum {
    DOM_HTML_OPTIONS_COLLECTION_PROP_0,
    DOM_HTML_OPTIONS_COLLECTION_PROP_SELECTED_INDEX,
    DOM_HTML_OPTIONS_COLLECTION_PROP_LENGTH,
};

static void webkit_dom_html_options_collection_set_property(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLOptionsCollection* self = WEBKIT_DOM_HTML_OPTIONS_COLLECTION(object);

    switch (propertyId) {
    case DOM_HTML_OPTIONS_COLLECTION_PROP_SELECTED_INDEX:
        webkit_dom_html_options_collection_set_selected_index(self, g_value_get_long(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_options_collection_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMHTMLOptionsCollection* self = WEBKIT_DOM_HTML_OPTIONS_COLLECTION(object);

    switch (propertyId) {
    case DOM_HTML_OPTIONS_COLLECTION_PROP_SELECTED_INDEX:
        g_value_set_long(value, webkit_dom_html_options_collection_get_selected_index(self));
        break;
    case DOM_HTML_OPTIONS_COLLECTION_PROP_LENGTH:
        g_value_set_ulong(value, webkit_dom_html_options_collection_get_length(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_html_options_collection_class_init(WebKitDOMHTMLOptionsCollectionClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->set_property = webkit_dom_html_options_collection_set_property;
    gobjectClass->get_property = webkit_dom_html_options_collection_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_OPTIONS_COLLECTION_PROP_SELECTED_INDEX,
        g_param_spec_long(
            "selected-index",
            "HTMLOptionsCollection:selected-index",
            "read-write glong HTMLOptionsCollection:selected-index",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(
        gobjectClass,
        DOM_HTML_OPTIONS_COLLECTION_PROP_LENGTH,
        g_param_spec_ulong(
            "length",
            "HTMLOptionsCollection:length",
            "read-only gulong HTMLOptionsCollection:length",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_html_options_collection_init(WebKitDOMHTMLOptionsCollection* request)
{
    UNUSED_PARAM(request);
}

WebKitDOMNode* webkit_dom_html_options_collection_named_item(WebKitDOMHTMLOptionsCollection* self, const gchar* name)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_OPTIONS_COLLECTION(self), 0);
    g_return_val_if_fail(name, 0);
    WebCore::HTMLOptionsCollection* item = WebKit::core(self);
    WTF::String convertedName = WTF::String::fromUTF8(name);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->namedItem(convertedName));
    return WebKit::kit(gobjectResult.get());
}

glong webkit_dom_html_options_collection_get_selected_index(WebKitDOMHTMLOptionsCollection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_OPTIONS_COLLECTION(self), 0);
    WebCore::HTMLOptionsCollection* item = WebKit::core(self);
    glong result = item->selectedIndex();
    return result;
}

void webkit_dom_html_options_collection_set_selected_index(WebKitDOMHTMLOptionsCollection* self, glong value)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_OPTIONS_COLLECTION(self));
    WebCore::HTMLOptionsCollection* item = WebKit::core(self);
    item->setSelectedIndex(value);
}

gulong webkit_dom_html_options_collection_get_length(WebKitDOMHTMLOptionsCollection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_OPTIONS_COLLECTION(self), 0);
    WebCore::HTMLOptionsCollection* item = WebKit::core(self);
    gulong result = item->length();
    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS;
