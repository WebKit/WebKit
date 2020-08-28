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
#include "WebKitDOMRange.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include <WebCore/JSExecState.h>
#include <WebCore/TextIterator.h>
#include "WebKitDOMDocumentFragmentPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMRangePrivate.h"
#include "ConvertToUTF8String.h"
#include "WebKitDOMRangeUnstable.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_RANGE_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_RANGE, WebKitDOMRangePrivate)

typedef struct _WebKitDOMRangePrivate {
    RefPtr<WebCore::Range> coreObject;
} WebKitDOMRangePrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMRange* kit(WebCore::Range* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_RANGE(ret);

    return wrapRange(obj);
}

WebCore::Range* core(WebKitDOMRange* request)
{
    return request ? static_cast<WebCore::Range*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMRange* wrapRange(WebCore::Range* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_RANGE(g_object_new(WEBKIT_DOM_TYPE_RANGE, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMRange, webkit_dom_range, WEBKIT_DOM_TYPE_OBJECT)

enum {
    DOM_RANGE_PROP_0,
    DOM_RANGE_PROP_START_CONTAINER,
    DOM_RANGE_PROP_START_OFFSET,
    DOM_RANGE_PROP_END_CONTAINER,
    DOM_RANGE_PROP_END_OFFSET,
    DOM_RANGE_PROP_COLLAPSED,
    DOM_RANGE_PROP_COMMON_ANCESTOR_CONTAINER,
    DOM_RANGE_PROP_TEXT,
};

static void webkit_dom_range_finalize(GObject* object)
{
    WebKitDOMRangePrivate* priv = WEBKIT_DOM_RANGE_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMRangePrivate();
    G_OBJECT_CLASS(webkit_dom_range_parent_class)->finalize(object);
}

static void webkit_dom_range_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMRange* self = WEBKIT_DOM_RANGE(object);

    switch (propertyId) {
    case DOM_RANGE_PROP_START_CONTAINER:
        g_value_set_object(value, webkit_dom_range_get_start_container(self, nullptr));
        break;
    case DOM_RANGE_PROP_START_OFFSET:
        g_value_set_long(value, webkit_dom_range_get_start_offset(self, nullptr));
        break;
    case DOM_RANGE_PROP_END_CONTAINER:
        g_value_set_object(value, webkit_dom_range_get_end_container(self, nullptr));
        break;
    case DOM_RANGE_PROP_END_OFFSET:
        g_value_set_long(value, webkit_dom_range_get_end_offset(self, nullptr));
        break;
    case DOM_RANGE_PROP_COLLAPSED:
        g_value_set_boolean(value, webkit_dom_range_get_collapsed(self, nullptr));
        break;
    case DOM_RANGE_PROP_COMMON_ANCESTOR_CONTAINER:
        g_value_set_object(value, webkit_dom_range_get_common_ancestor_container(self, nullptr));
        break;
    case DOM_RANGE_PROP_TEXT:
        g_value_take_string(value, webkit_dom_range_get_text(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GObject* webkit_dom_range_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_range_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMRangePrivate* priv = WEBKIT_DOM_RANGE_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::Range*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_range_class_init(WebKitDOMRangeClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMRangePrivate));
    gobjectClass->constructor = webkit_dom_range_constructor;
    gobjectClass->finalize = webkit_dom_range_finalize;
    gobjectClass->get_property = webkit_dom_range_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_RANGE_PROP_START_CONTAINER,
        g_param_spec_object(
            "start-container",
            "Range:start-container",
            "read-only WebKitDOMNode* Range:start-container",
            WEBKIT_DOM_TYPE_NODE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_RANGE_PROP_START_OFFSET,
        g_param_spec_long(
            "start-offset",
            "Range:start-offset",
            "read-only glong Range:start-offset",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_RANGE_PROP_END_CONTAINER,
        g_param_spec_object(
            "end-container",
            "Range:end-container",
            "read-only WebKitDOMNode* Range:end-container",
            WEBKIT_DOM_TYPE_NODE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_RANGE_PROP_END_OFFSET,
        g_param_spec_long(
            "end-offset",
            "Range:end-offset",
            "read-only glong Range:end-offset",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_RANGE_PROP_COLLAPSED,
        g_param_spec_boolean(
            "collapsed",
            "Range:collapsed",
            "read-only gboolean Range:collapsed",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_RANGE_PROP_COMMON_ANCESTOR_CONTAINER,
        g_param_spec_object(
            "common-ancestor-container",
            "Range:common-ancestor-container",
            "read-only WebKitDOMNode* Range:common-ancestor-container",
            WEBKIT_DOM_TYPE_NODE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_RANGE_PROP_TEXT,
        g_param_spec_string(
            "text",
            "Range:text",
            "read-only gchar* Range:text",
            "",
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_range_init(WebKitDOMRange* request)
{
    WebKitDOMRangePrivate* priv = WEBKIT_DOM_RANGE_GET_PRIVATE(request);
    new (priv) WebKitDOMRangePrivate();
}

void webkit_dom_range_set_start(WebKitDOMRange* self, WebKitDOMNode* refNode, glong offset, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(refNode));
    g_return_if_fail(!error || !*error);
    WebCore::Range* item = WebKit::core(self);
    WebCore::Node* convertedRefNode = WebKit::core(refNode);
    auto result = item->setStart(*convertedRefNode, offset);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_range_set_end(WebKitDOMRange* self, WebKitDOMNode* refNode, glong offset, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(refNode));
    g_return_if_fail(!error || !*error);
    WebCore::Range* item = WebKit::core(self);
    WebCore::Node* convertedRefNode = WebKit::core(refNode);
    auto result = item->setEnd(*convertedRefNode, offset);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_range_set_start_before(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(refNode));
    g_return_if_fail(!error || !*error);
    WebCore::Range* item = WebKit::core(self);
    WebCore::Node* convertedRefNode = WebKit::core(refNode);
    auto result = item->setStartBefore(*convertedRefNode);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_range_set_start_after(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(refNode));
    g_return_if_fail(!error || !*error);
    WebCore::Range* item = WebKit::core(self);
    WebCore::Node* convertedRefNode = WebKit::core(refNode);
    auto result = item->setStartAfter(*convertedRefNode);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_range_set_end_before(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(refNode));
    g_return_if_fail(!error || !*error);
    WebCore::Range* item = WebKit::core(self);
    WebCore::Node* convertedRefNode = WebKit::core(refNode);
    auto result = item->setEndBefore(*convertedRefNode);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_range_set_end_after(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(refNode));
    g_return_if_fail(!error || !*error);
    WebCore::Range* item = WebKit::core(self);
    WebCore::Node* convertedRefNode = WebKit::core(refNode);
    auto result = item->setEndAfter(*convertedRefNode);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_range_collapse(WebKitDOMRange* self, gboolean toStart, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    UNUSED_PARAM(error);
    WebCore::Range* item = WebKit::core(self);
    item->collapse(toStart);
}

void webkit_dom_range_select_node(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(refNode));
    g_return_if_fail(!error || !*error);
    WebCore::Range* item = WebKit::core(self);
    WebCore::Node* convertedRefNode = WebKit::core(refNode);
    auto result = item->selectNode(*convertedRefNode);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_range_select_node_contents(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(refNode));
    g_return_if_fail(!error || !*error);
    WebCore::Range* item = WebKit::core(self);
    WebCore::Node* convertedRefNode = WebKit::core(refNode);
    auto result = item->selectNodeContents(*convertedRefNode);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

gshort webkit_dom_range_compare_boundary_points(WebKitDOMRange* self, gushort how, WebKitDOMRange* sourceRange, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(sourceRange), 0);
    g_return_val_if_fail(!error || !*error, 0);
    auto result = WebKit::core(self)->compareBoundaryPoints(how, *WebKit::core(sourceRange));
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return 0;
    }
    return result.releaseReturnValue();
}

void webkit_dom_range_delete_contents(WebKitDOMRange* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    g_return_if_fail(!error || !*error);
    WebCore::Range* item = WebKit::core(self);
    auto result = item->deleteContents();
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

WebKitDOMDocumentFragment* webkit_dom_range_extract_contents(WebKitDOMRange* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Range* item = WebKit::core(self);
    auto result = item->extractContents();
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

WebKitDOMDocumentFragment* webkit_dom_range_clone_contents(WebKitDOMRange* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Range* item = WebKit::core(self);
    auto result = item->cloneContents();
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

void webkit_dom_range_insert_node(WebKitDOMRange* self, WebKitDOMNode* newNode, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(newNode));
    g_return_if_fail(!error || !*error);
    WebCore::Range* item = WebKit::core(self);
    WebCore::Node* convertedNewNode = WebKit::core(newNode);
    auto result = item->insertNode(*convertedNewNode);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_range_surround_contents(WebKitDOMRange* self, WebKitDOMNode* newParent, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(newParent));
    g_return_if_fail(!error || !*error);
    WebCore::Range* item = WebKit::core(self);
    WebCore::Node* convertedNewParent = WebKit::core(newParent);
    auto result = item->surroundContents(*convertedNewParent);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

WebKitDOMRange* webkit_dom_range_clone_range(WebKitDOMRange* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    UNUSED_PARAM(error);
    WebCore::Range* item = WebKit::core(self);
    RefPtr<WebCore::Range> gobjectResult = WTF::getPtr(item->cloneRange());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_range_to_string(WebKitDOMRange* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    UNUSED_PARAM(error);
    WebCore::Range* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->toString());
    return result;
}

void webkit_dom_range_detach(WebKitDOMRange* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    UNUSED_PARAM(error);
    WebCore::Range* item = WebKit::core(self);
    item->detach();
}

WebKitDOMDocumentFragment* webkit_dom_range_create_contextual_fragment(WebKitDOMRange* self, const gchar* html, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    g_return_val_if_fail(html, 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Range* item = WebKit::core(self);
    WTF::String convertedHtml = WTF::String::fromUTF8(html);
    auto result = item->createContextualFragment(convertedHtml);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

gshort webkit_dom_range_compare_node(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(refNode), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Range* item = WebKit::core(self);
    WebCore::Node* convertedRefNode = WebKit::core(refNode);
    auto result = item->compareNode(*convertedRefNode);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return 0;
    }
    return result.releaseReturnValue();
}

gboolean webkit_dom_range_intersects_node(WebKitDOMRange* self, WebKitDOMNode* refNode, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), FALSE);
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(refNode), FALSE);
    g_return_val_if_fail(!error || !*error, FALSE);
    return WebKit::core(self)->intersectsNode(*WebKit::core(refNode));
}

gshort webkit_dom_range_compare_point(WebKitDOMRange* self, WebKitDOMNode* refNode, glong offset, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(refNode), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::Range* item = WebKit::core(self);
    WebCore::Node* convertedRefNode = WebKit::core(refNode);
    auto result = item->comparePoint(*convertedRefNode, offset);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return 0;
    }
    return result.releaseReturnValue();
}

gboolean webkit_dom_range_is_point_in_range(WebKitDOMRange* self, WebKitDOMNode* refNode, glong offset, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), FALSE);
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(refNode), FALSE);
    g_return_val_if_fail(!error || !*error, FALSE);
    WebCore::Range* item = WebKit::core(self);
    WebCore::Node* convertedRefNode = WebKit::core(refNode);
    auto result = item->isPointInRange(*convertedRefNode, offset);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return false;
    }
    return result.releaseReturnValue();
}

void webkit_dom_range_expand(WebKitDOMRange* self, const gchar* unit, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(self));
    g_return_if_fail(unit);
    g_return_if_fail(!error || !*error);
    WebCore::Range* item = WebKit::core(self);
    WTF::String convertedUnit = WTF::String::fromUTF8(unit);
    auto result = item->expand(convertedUnit);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

WebKitDOMNode* webkit_dom_range_get_start_container(WebKitDOMRange* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    UNUSED_PARAM(error);
    WebCore::Range* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->startContainer());
    return WebKit::kit(gobjectResult.get());
}

glong webkit_dom_range_get_start_offset(WebKitDOMRange* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    UNUSED_PARAM(error);
    WebCore::Range* item = WebKit::core(self);
    glong result = item->startOffset();
    return result;
}

WebKitDOMNode* webkit_dom_range_get_end_container(WebKitDOMRange* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    UNUSED_PARAM(error);
    WebCore::Range* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->endContainer());
    return WebKit::kit(gobjectResult.get());
}

glong webkit_dom_range_get_end_offset(WebKitDOMRange* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    UNUSED_PARAM(error);
    WebCore::Range* item = WebKit::core(self);
    glong result = item->endOffset();
    return result;
}

gboolean webkit_dom_range_get_collapsed(WebKitDOMRange* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), FALSE);
    UNUSED_PARAM(error);
    WebCore::Range* item = WebKit::core(self);
    gboolean result = item->collapsed();
    return result;
}

WebKitDOMNode* webkit_dom_range_get_common_ancestor_container(WebKitDOMRange* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    UNUSED_PARAM(error);
    WebCore::Range* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->commonAncestorContainer());
    return WebKit::kit(gobjectResult.get());
}

gchar* webkit_dom_range_get_text(WebKitDOMRange* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_RANGE(self), 0);
    auto range = makeSimpleRange(*WebKit::core(self));
    range.start.document().updateLayout();
    return convertToUTF8String(plainText(range));
}

G_GNUC_END_IGNORE_DEPRECATIONS;
