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
#include "WebKitDOMDOMSelection.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include <WebCore/JSExecState.h>
#include "WebKitDOMDOMSelectionPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMRangePrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_DOM_SELECTION_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_DOM_SELECTION, WebKitDOMDOMSelectionPrivate)

typedef struct _WebKitDOMDOMSelectionPrivate {
    RefPtr<WebCore::DOMSelection> coreObject;
} WebKitDOMDOMSelectionPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMDOMSelection* kit(WebCore::DOMSelection* obj)
{
    if (!obj)
        return 0;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_DOM_SELECTION(ret);

    return wrapDOMSelection(obj);
}

WebCore::DOMSelection* core(WebKitDOMDOMSelection* request)
{
    return request ? static_cast<WebCore::DOMSelection*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMDOMSelection* wrapDOMSelection(WebCore::DOMSelection* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_DOM_SELECTION(g_object_new(WEBKIT_DOM_TYPE_DOM_SELECTION, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMDOMSelection, webkit_dom_dom_selection, WEBKIT_DOM_TYPE_OBJECT)

enum {
    DOM_DOM_SELECTION_PROP_0,
    DOM_DOM_SELECTION_PROP_ANCHOR_NODE,
    DOM_DOM_SELECTION_PROP_ANCHOR_OFFSET,
    DOM_DOM_SELECTION_PROP_FOCUS_NODE,
    DOM_DOM_SELECTION_PROP_FOCUS_OFFSET,
    DOM_DOM_SELECTION_PROP_IS_COLLAPSED,
    DOM_DOM_SELECTION_PROP_RANGE_COUNT,
    DOM_DOM_SELECTION_PROP_TYPE,
    DOM_DOM_SELECTION_PROP_BASE_NODE,
    DOM_DOM_SELECTION_PROP_BASE_OFFSET,
    DOM_DOM_SELECTION_PROP_EXTENT_NODE,
    DOM_DOM_SELECTION_PROP_EXTENT_OFFSET,
};

static void webkit_dom_dom_selection_finalize(GObject* object)
{
    WebKitDOMDOMSelectionPrivate* priv = WEBKIT_DOM_DOM_SELECTION_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMDOMSelectionPrivate();
    G_OBJECT_CLASS(webkit_dom_dom_selection_parent_class)->finalize(object);
}

static void webkit_dom_dom_selection_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMDOMSelection* self = WEBKIT_DOM_DOM_SELECTION(object);

    switch (propertyId) {
    case DOM_DOM_SELECTION_PROP_ANCHOR_NODE:
        g_value_set_object(value, webkit_dom_dom_selection_get_anchor_node(self));
        break;
    case DOM_DOM_SELECTION_PROP_ANCHOR_OFFSET:
        g_value_set_ulong(value, webkit_dom_dom_selection_get_anchor_offset(self));
        break;
    case DOM_DOM_SELECTION_PROP_FOCUS_NODE:
        g_value_set_object(value, webkit_dom_dom_selection_get_focus_node(self));
        break;
    case DOM_DOM_SELECTION_PROP_FOCUS_OFFSET:
        g_value_set_ulong(value, webkit_dom_dom_selection_get_focus_offset(self));
        break;
    case DOM_DOM_SELECTION_PROP_IS_COLLAPSED:
        g_value_set_boolean(value, webkit_dom_dom_selection_get_is_collapsed(self));
        break;
    case DOM_DOM_SELECTION_PROP_RANGE_COUNT:
        g_value_set_ulong(value, webkit_dom_dom_selection_get_range_count(self));
        break;
    case DOM_DOM_SELECTION_PROP_TYPE:
        g_value_take_string(value, webkit_dom_dom_selection_get_selection_type(self));
        break;
    case DOM_DOM_SELECTION_PROP_BASE_NODE:
        g_value_set_object(value, webkit_dom_dom_selection_get_base_node(self));
        break;
    case DOM_DOM_SELECTION_PROP_BASE_OFFSET:
        g_value_set_ulong(value, webkit_dom_dom_selection_get_base_offset(self));
        break;
    case DOM_DOM_SELECTION_PROP_EXTENT_NODE:
        g_value_set_object(value, webkit_dom_dom_selection_get_extent_node(self));
        break;
    case DOM_DOM_SELECTION_PROP_EXTENT_OFFSET:
        g_value_set_ulong(value, webkit_dom_dom_selection_get_extent_offset(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static GObject* webkit_dom_dom_selection_constructor(GType type, guint constructPropertiesCount, GObjectConstructParam* constructProperties)
{
    GObject* object = G_OBJECT_CLASS(webkit_dom_dom_selection_parent_class)->constructor(type, constructPropertiesCount, constructProperties);

    WebKitDOMDOMSelectionPrivate* priv = WEBKIT_DOM_DOM_SELECTION_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::DOMSelection*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);

    return object;
}

static void webkit_dom_dom_selection_class_init(WebKitDOMDOMSelectionClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMDOMSelectionPrivate));
    gobjectClass->constructor = webkit_dom_dom_selection_constructor;
    gobjectClass->finalize = webkit_dom_dom_selection_finalize;
    gobjectClass->get_property = webkit_dom_dom_selection_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_DOM_SELECTION_PROP_ANCHOR_NODE,
        g_param_spec_object(
            "anchor-node",
            "DOMSelection:anchor-node",
            "read-only WebKitDOMNode* DOMSelection:anchor-node",
            WEBKIT_DOM_TYPE_NODE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOM_SELECTION_PROP_ANCHOR_OFFSET,
        g_param_spec_ulong(
            "anchor-offset",
            "DOMSelection:anchor-offset",
            "read-only gulong DOMSelection:anchor-offset",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOM_SELECTION_PROP_FOCUS_NODE,
        g_param_spec_object(
            "focus-node",
            "DOMSelection:focus-node",
            "read-only WebKitDOMNode* DOMSelection:focus-node",
            WEBKIT_DOM_TYPE_NODE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOM_SELECTION_PROP_FOCUS_OFFSET,
        g_param_spec_ulong(
            "focus-offset",
            "DOMSelection:focus-offset",
            "read-only gulong DOMSelection:focus-offset",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOM_SELECTION_PROP_IS_COLLAPSED,
        g_param_spec_boolean(
            "is-collapsed",
            "DOMSelection:is-collapsed",
            "read-only gboolean DOMSelection:is-collapsed",
            FALSE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOM_SELECTION_PROP_RANGE_COUNT,
        g_param_spec_ulong(
            "range-count",
            "DOMSelection:range-count",
            "read-only gulong DOMSelection:range-count",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOM_SELECTION_PROP_TYPE,
        g_param_spec_string(
            "type",
            "DOMSelection:type",
            "read-only gchar* DOMSelection:type",
            "",
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOM_SELECTION_PROP_BASE_NODE,
        g_param_spec_object(
            "base-node",
            "DOMSelection:base-node",
            "read-only WebKitDOMNode* DOMSelection:base-node",
            WEBKIT_DOM_TYPE_NODE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOM_SELECTION_PROP_BASE_OFFSET,
        g_param_spec_ulong(
            "base-offset",
            "DOMSelection:base-offset",
            "read-only gulong DOMSelection:base-offset",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOM_SELECTION_PROP_EXTENT_NODE,
        g_param_spec_object(
            "extent-node",
            "DOMSelection:extent-node",
            "read-only WebKitDOMNode* DOMSelection:extent-node",
            WEBKIT_DOM_TYPE_NODE,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_DOM_SELECTION_PROP_EXTENT_OFFSET,
        g_param_spec_ulong(
            "extent-offset",
            "DOMSelection:extent-offset",
            "read-only gulong DOMSelection:extent-offset",
            0, G_MAXULONG, 0,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_dom_selection_init(WebKitDOMDOMSelection* request)
{
    WebKitDOMDOMSelectionPrivate* priv = WEBKIT_DOM_DOM_SELECTION_GET_PRIVATE(request);
    new (priv) WebKitDOMDOMSelectionPrivate();
}

void webkit_dom_dom_selection_collapse(WebKitDOMDOMSelection* self, WebKitDOMNode* node, gulong offset)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(node));
    WebCore::DOMSelection* item = WebKit::core(self);
    WebCore::Node* convertedNode = WebKit::core(node);
    item->collapse(convertedNode, offset);
}

void webkit_dom_dom_selection_collapse_to_end(WebKitDOMDOMSelection* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self));
    g_return_if_fail(!error || !*error);
    WebCore::DOMSelection* item = WebKit::core(self);
    auto result = item->collapseToEnd();
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_dom_selection_collapse_to_start(WebKitDOMDOMSelection* self, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self));
    g_return_if_fail(!error || !*error);
    WebCore::DOMSelection* item = WebKit::core(self);
    auto result = item->collapseToStart();
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

void webkit_dom_dom_selection_delete_from_document(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self));
    WebCore::DOMSelection* item = WebKit::core(self);
    item->deleteFromDocument();
}

gboolean webkit_dom_dom_selection_contains_node(WebKitDOMDOMSelection* self, WebKitDOMNode* node, gboolean allowPartial)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self), FALSE);
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(node), FALSE);
    WebCore::DOMSelection* item = WebKit::core(self);
    WebCore::Node* convertedNode = WebKit::core(node);
    gboolean result = item->containsNode(*convertedNode, allowPartial);
    return result;
}

void webkit_dom_dom_selection_select_all_children(WebKitDOMDOMSelection* self, WebKitDOMNode* node)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(node));
    WebCore::DOMSelection* item = WebKit::core(self);
    WebCore::Node* convertedNode = WebKit::core(node);
    item->selectAllChildren(*convertedNode);
}

void webkit_dom_dom_selection_extend(WebKitDOMDOMSelection* self, WebKitDOMNode* node, gulong offset, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(node));
    g_return_if_fail(!error || !*error);
    WebCore::DOMSelection* item = WebKit::core(self);
    WebCore::Node* convertedNode = WebKit::core(node);
    auto result = item->extend(*convertedNode, offset);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

WebKitDOMRange* webkit_dom_dom_selection_get_range_at(WebKitDOMDOMSelection* self, gulong index, GError** error)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self), 0);
    g_return_val_if_fail(!error || !*error, 0);
    WebCore::DOMSelection* item = WebKit::core(self);
    auto result = item->getRangeAt(index);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
        return nullptr;
    }
    return WebKit::kit(result.releaseReturnValue().ptr());
}

void webkit_dom_dom_selection_remove_all_ranges(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self));
    WebCore::DOMSelection* item = WebKit::core(self);
    item->removeAllRanges();
}

void webkit_dom_dom_selection_add_range(WebKitDOMDOMSelection* self, WebKitDOMRange* range)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self));
    g_return_if_fail(WEBKIT_DOM_IS_RANGE(range));
    WebCore::DOMSelection* item = WebKit::core(self);
    WebCore::Range* convertedRange = WebKit::core(range);
    item->addRange(*convertedRange);
}

void webkit_dom_dom_selection_set_base_and_extent(WebKitDOMDOMSelection* self, WebKitDOMNode* baseNode, gulong baseOffset, WebKitDOMNode* extentNode, gulong extentOffset)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(baseNode));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(extentNode));
    WebCore::DOMSelection* item = WebKit::core(self);
    WebCore::Node* convertedBaseNode = WebKit::core(baseNode);
    WebCore::Node* convertedExtentNode = WebKit::core(extentNode);
    item->setBaseAndExtent(convertedBaseNode, baseOffset, convertedExtentNode, extentOffset);
}

void webkit_dom_dom_selection_set_position(WebKitDOMDOMSelection* self, WebKitDOMNode* node, gulong offset)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self));
    g_return_if_fail(WEBKIT_DOM_IS_NODE(node));
    WebCore::DOMSelection* item = WebKit::core(self);
    WebCore::Node* convertedNode = WebKit::core(node);
    item->setPosition(convertedNode, offset);
}

void webkit_dom_dom_selection_empty(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self));
    WebCore::DOMSelection* item = WebKit::core(self);
    item->empty();
}

void webkit_dom_dom_selection_modify(WebKitDOMDOMSelection* self, const gchar* alter, const gchar* direction, const gchar* granularity)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self));
    g_return_if_fail(alter);
    g_return_if_fail(direction);
    g_return_if_fail(granularity);
    WebCore::DOMSelection* item = WebKit::core(self);
    WTF::String convertedAlter = WTF::String::fromUTF8(alter);
    WTF::String convertedDirection = WTF::String::fromUTF8(direction);
    WTF::String convertedGranularity = WTF::String::fromUTF8(granularity);
    item->modify(convertedAlter, convertedDirection, convertedGranularity);
}

WebKitDOMNode* webkit_dom_dom_selection_get_anchor_node(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self), 0);
    WebCore::DOMSelection* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->anchorNode());
    return WebKit::kit(gobjectResult.get());
}

gulong webkit_dom_dom_selection_get_anchor_offset(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self), 0);
    WebCore::DOMSelection* item = WebKit::core(self);
    gulong result = item->anchorOffset();
    return result;
}

WebKitDOMNode* webkit_dom_dom_selection_get_focus_node(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self), 0);
    WebCore::DOMSelection* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->focusNode());
    return WebKit::kit(gobjectResult.get());
}

gulong webkit_dom_dom_selection_get_focus_offset(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self), 0);
    WebCore::DOMSelection* item = WebKit::core(self);
    gulong result = item->focusOffset();
    return result;
}

gboolean webkit_dom_dom_selection_get_is_collapsed(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self), FALSE);
    WebCore::DOMSelection* item = WebKit::core(self);
    gboolean result = item->isCollapsed();
    return result;
}

gulong webkit_dom_dom_selection_get_range_count(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self), 0);
    WebCore::DOMSelection* item = WebKit::core(self);
    gulong result = item->rangeCount();
    return result;
}

gchar* webkit_dom_dom_selection_get_selection_type(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self), 0);
    WebCore::DOMSelection* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->type());
    return result;
}

WebKitDOMNode* webkit_dom_dom_selection_get_base_node(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self), 0);
    WebCore::DOMSelection* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->baseNode());
    return WebKit::kit(gobjectResult.get());
}

gulong webkit_dom_dom_selection_get_base_offset(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self), 0);
    WebCore::DOMSelection* item = WebKit::core(self);
    gulong result = item->baseOffset();
    return result;
}

WebKitDOMNode* webkit_dom_dom_selection_get_extent_node(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self), 0);
    WebCore::DOMSelection* item = WebKit::core(self);
    RefPtr<WebCore::Node> gobjectResult = WTF::getPtr(item->extentNode());
    return WebKit::kit(gobjectResult.get());
}

gulong webkit_dom_dom_selection_get_extent_offset(WebKitDOMDOMSelection* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_SELECTION(self), 0);
    WebCore::DOMSelection* item = WebKit::core(self);
    gulong result = item->extentOffset();
    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS;
