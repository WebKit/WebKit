/*
 * Copyright (C) 2010, 2011, 2012 Igalia S.L.
 * Copyright (C) 2013 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitAccessibleHyperlink.h"

#if ENABLE(ACCESSIBILITY)

#include "AXObjectCache.h"
#include "AccessibilityObject.h"
#include "Editing.h"
#include "NotImplemented.h"
#include "Position.h"
#include "Range.h"
#include "RenderListMarker.h"
#include "RenderObject.h"
#include "TextIterator.h"
#include "WebKitAccessible.h"
#include "WebKitAccessibleUtil.h"
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

using namespace WebCore;

static void webkit_accessible_hyperlink_atk_action_interface_init(AtkActionIface*);

struct _WebKitAccessibleHyperlinkPrivate {
    WebKitAccessible* hyperlinkImpl;

    // We cache these values so we can return them as const values.
    CString actionName;
    CString actionKeyBinding;
};

WEBKIT_DEFINE_TYPE_WITH_CODE(
    WebKitAccessibleHyperlink, webkit_accessible_hyperlink, ATK_TYPE_HYPERLINK,
    G_IMPLEMENT_INTERFACE(ATK_TYPE_ACTION, webkit_accessible_hyperlink_atk_action_interface_init))

enum {
    PROP_0,

    PROP_HYPERLINK_IMPL
};

static gboolean webkitAccessibleHyperlinkActionDoAction(AtkAction* action, gint)
{
    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(action);
    returnValIfWebKitAccessibleIsInvalid(accessibleHyperlink->priv->hyperlinkImpl, FALSE);

    if (!ATK_IS_ACTION(accessibleHyperlink->priv->hyperlinkImpl))
        return FALSE;

    auto& coreObject = webkitAccessibleGetAccessibilityObject(accessibleHyperlink->priv->hyperlinkImpl);
    return coreObject.performDefaultAction();
}

static gint webkitAccessibleHyperlinkActionGetNActions(AtkAction* action)
{
    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(action);
    returnValIfWebKitAccessibleIsInvalid(accessibleHyperlink->priv->hyperlinkImpl, 0);

    return ATK_IS_ACTION(accessibleHyperlink->priv->hyperlinkImpl) ? 1 : 0;
}

static const gchar* webkitAccessibleHyperlinkActionGetDescription(AtkAction* action, gint)
{
    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(action);
    returnValIfWebKitAccessibleIsInvalid(accessibleHyperlink->priv->hyperlinkImpl, nullptr);

    // TODO: Need a way to provide/localize action descriptions.
    notImplemented();
    return "";
}

static const gchar* webkitAccessibleHyperlinkActionGetKeybinding(AtkAction* action, gint)
{
    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(action);
    returnValIfWebKitAccessibleIsInvalid(accessibleHyperlink->priv->hyperlinkImpl, nullptr);

    if (!ATK_IS_ACTION(accessibleHyperlink->priv->hyperlinkImpl))
        return nullptr;

    auto& coreObject = webkitAccessibleGetAccessibilityObject(accessibleHyperlink->priv->hyperlinkImpl);
    accessibleHyperlink->priv->actionKeyBinding = coreObject.accessKey().string().utf8();
    return accessibleHyperlink->priv->actionKeyBinding.data();
}

static const gchar* webkitAccessibleHyperlinkActionGetName(AtkAction* action, gint)
{
    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(action);
    returnValIfWebKitAccessibleIsInvalid(accessibleHyperlink->priv->hyperlinkImpl, nullptr);

    if (!ATK_IS_ACTION(accessibleHyperlink->priv->hyperlinkImpl))
        return nullptr;

    auto& coreObject = webkitAccessibleGetAccessibilityObject(accessibleHyperlink->priv->hyperlinkImpl);
    accessibleHyperlink->priv->actionName = coreObject.actionVerb().utf8();
    return accessibleHyperlink->priv->actionName.data();
}

static void webkit_accessible_hyperlink_atk_action_interface_init(AtkActionIface* iface)
{
    iface->do_action = webkitAccessibleHyperlinkActionDoAction;
    iface->get_n_actions = webkitAccessibleHyperlinkActionGetNActions;
    iface->get_description = webkitAccessibleHyperlinkActionGetDescription;
    iface->get_keybinding = webkitAccessibleHyperlinkActionGetKeybinding;
    iface->get_name = webkitAccessibleHyperlinkActionGetName;
}

static gchar* webkitAccessibleHyperlinkGetURI(AtkHyperlink* link, gint index)
{
    // FIXME: Do NOT support more than one instance of an AtkObject
    // implementing AtkHyperlinkImpl in every instance of AtkHyperLink
    if (index)
        return nullptr;

    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(link);
    returnValIfWebKitAccessibleIsInvalid(accessibleHyperlink->priv->hyperlinkImpl, nullptr);

    auto& coreObject = webkitAccessibleGetAccessibilityObject(accessibleHyperlink->priv->hyperlinkImpl);
    return !coreObject.url().isNull() ? g_strdup(coreObject.url().string().utf8().data()) : nullptr;
}

static AtkObject* webkitAccessibleHyperlinkGetObject(AtkHyperlink* link, gint index)
{
    // FIXME: Do NOT support more than one instance of an AtkObject
    // implementing AtkHyperlinkImpl in every instance of AtkHyperLink
    if (index)
        return nullptr;

    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(link);
    returnValIfWebKitAccessibleIsInvalid(accessibleHyperlink->priv->hyperlinkImpl, 0);

    return ATK_OBJECT(accessibleHyperlink->priv->hyperlinkImpl);
}

static gint rangeLengthForObject(AccessibilityObject& obj, Range* range)
{
    // This is going to be the actual length in most of the cases
    int baseLength = TextIterator::rangeLength(range, true);

    // Check whether the current hyperlink belongs to a list item.
    // If so, we need to consider the length of the item's marker
    AccessibilityObject* parent = obj.parentObjectUnignored();
    if (!parent || !parent->isAccessibilityRenderObject() || !parent->isListItem())
        return baseLength;

    // Even if we don't expose list markers to Assistive
    // Technologies, we need to have a way to measure their length
    // for those cases when it's needed to take it into account
    // separately (as in getAccessibilityObjectForOffset)
    AccessibilityObject* markerObj = parent->firstChild();
    if (!markerObj)
        return baseLength;

    RenderObject* renderer = markerObj->renderer();
    if (!is<RenderListMarker>(renderer))
        return baseLength;

    auto& marker = downcast<RenderListMarker>(*renderer);
    return baseLength + marker.text().length() + marker.suffix().length();
}

static gint webkitAccessibleHyperlinkGetStartIndex(AtkHyperlink* link)
{
    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(link);
    returnValIfWebKitAccessibleIsInvalid(accessibleHyperlink->priv->hyperlinkImpl, 0);

    auto& coreObject = webkitAccessibleGetAccessibilityObject(accessibleHyperlink->priv->hyperlinkImpl);
    AccessibilityObject* parentUnignored = coreObject.parentObjectUnignored();
    if (!parentUnignored)
        return 0;

    Node* node = coreObject.node();
    if (!node)
        return 0;

    Node* parentNode = parentUnignored->node();
    if (!parentNode)
        return 0;

    auto range = Range::create(node->document(), firstPositionInOrBeforeNode(parentNode), firstPositionInOrBeforeNode(node));
    return rangeLengthForObject(coreObject, range.ptr());
}

static gint webkitAccessibleHyperlinkGetEndIndex(AtkHyperlink* link)
{
    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(link);
    returnValIfWebKitAccessibleIsInvalid(accessibleHyperlink->priv->hyperlinkImpl, 0);

    auto& coreObject = webkitAccessibleGetAccessibilityObject(accessibleHyperlink->priv->hyperlinkImpl);
    AccessibilityObject* parentUnignored = coreObject.parentObjectUnignored();
    if (!parentUnignored)
        return 0;

    Node* node = coreObject.node();
    if (!node)
        return 0;

    Node* parentNode = parentUnignored->node();
    if (!parentNode)
        return 0;

    auto range = Range::create(node->document(), firstPositionInOrBeforeNode(parentNode), lastPositionInOrAfterNode(node));
    return rangeLengthForObject(coreObject, range.ptr());
}

static gboolean webkitAccessibleHyperlinkIsValid(AtkHyperlink* link)
{
    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(link);
    returnValIfWebKitAccessibleIsInvalid(accessibleHyperlink->priv->hyperlinkImpl, FALSE);

    // Link is valid for the whole object's lifetime
    return TRUE;
}

static gint webkitAccessibleHyperlinkGetNAnchors(AtkHyperlink* link)
{
    // FIXME Do NOT support more than one instance of an AtkObject
    // implementing AtkHyperlinkImpl in every instance of AtkHyperLink
    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(link);
    returnValIfWebKitAccessibleIsInvalid(accessibleHyperlink->priv->hyperlinkImpl, 0);

    return 1;
}

static gboolean webkitAccessibleHyperlinkIsSelectedLink(AtkHyperlink* link)
{
    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(link);
    returnValIfWebKitAccessibleIsInvalid(accessibleHyperlink->priv->hyperlinkImpl, FALSE);

    // Not implemented: this function is deprecated in ATK now
    notImplemented();
    return FALSE;
}

static void webkitAccessibleHyperlinkGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* pspec)
{
    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(object);

    switch (propId) {
    case PROP_HYPERLINK_IMPL:
        g_value_set_object(value, accessibleHyperlink->priv->hyperlinkImpl);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
}

static void webkitAccessibleHyperlinkSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* pspec)
{
    auto* accessibleHyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(object);

    switch (propId) {
    case PROP_HYPERLINK_IMPL:
        accessibleHyperlink->priv->hyperlinkImpl = WEBKIT_ACCESSIBLE(g_value_get_object(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
}

static void webkit_accessible_hyperlink_class_init(WebKitAccessibleHyperlinkClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->set_property = webkitAccessibleHyperlinkSetProperty;
    gobjectClass->get_property = webkitAccessibleHyperlinkGetProperty;

    AtkHyperlinkClass* atkHyperlinkClass = ATK_HYPERLINK_CLASS(klass);
    atkHyperlinkClass->get_uri = webkitAccessibleHyperlinkGetURI;
    atkHyperlinkClass->get_object = webkitAccessibleHyperlinkGetObject;
    atkHyperlinkClass->get_start_index = webkitAccessibleHyperlinkGetStartIndex;
    atkHyperlinkClass->get_end_index = webkitAccessibleHyperlinkGetEndIndex;
    atkHyperlinkClass->is_valid = webkitAccessibleHyperlinkIsValid;
    atkHyperlinkClass->get_n_anchors = webkitAccessibleHyperlinkGetNAnchors;
    atkHyperlinkClass->is_selected_link = webkitAccessibleHyperlinkIsSelectedLink;

    g_object_class_install_property(gobjectClass, PROP_HYPERLINK_IMPL,
        g_param_spec_object("hyperlink-impl",
            "Hyperlink implementation",
            "The associated WebKitAccessible instance.",
            WEBKIT_TYPE_ACCESSIBLE,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
}

WebKitAccessibleHyperlink* webkitAccessibleHyperlinkGetOrCreate(AtkHyperlinkImpl* hyperlinkImpl)
{
    g_return_val_if_fail(ATK_IS_HYPERLINK_IMPL(hyperlinkImpl), nullptr);
    g_return_val_if_fail(WEBKIT_IS_ACCESSIBLE(hyperlinkImpl), nullptr);

    if (auto* currentHyperLink = g_object_get_data(G_OBJECT(hyperlinkImpl), "webkit-accessible-hyperlink-object"))
        return WEBKIT_ACCESSIBLE_HYPERLINK(g_object_ref(currentHyperLink));

    auto* hyperlink = WEBKIT_ACCESSIBLE_HYPERLINK(g_object_new(WEBKIT_TYPE_ACCESSIBLE_HYPERLINK, "hyperlink-impl", hyperlinkImpl, nullptr));
    g_object_set_data_full(G_OBJECT(hyperlinkImpl), "webkit-accessible-hyperlink-object", hyperlink, g_object_unref);
    return hyperlink;
}

#endif // ENABLE(ACCESSIBILITY)
