/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "WebKitContextMenuGAction.h"

#if ENABLE(CONTEXT_MENUS)
#include "WebContextMenuItemData.h"
#include "WebContextMenuProxy.h"
#include "WebPageProxy.h"
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

static void webkitContextMenuGActionGActionInterfaceInit(GActionInterface*);

struct _WebKitContextMenuGActionPrivate {
    GUniquePtr<char> name;
    WebContextMenuItemData item;
    GRefPtr<GVariant> state;
    WeakPtr<WebPageProxy> page;
#if PLATFORM(GTK) && !USE(GTK4)
    GRefPtr<GtkAction> gtkAction;
#endif
};

WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(WebKitContextMenuGAction, webkit_context_menu_gaction, G_TYPE_OBJECT, GObject,
    G_IMPLEMENT_INTERFACE(G_TYPE_ACTION, webkitContextMenuGActionGActionInterfaceInit))

enum {
    PROP_0,
    PROP_NAME,
    PROP_PARAMETER_TYPE,
    PROP_ENABLED,
    PROP_STATE_TYPE,
    PROP_STATE
};

static const char* webkitContextMenuGActionGetName(GAction* action)
{
    auto* priv = WEBKIT_CONTEXT_MENU_GACTION(action)->priv;
    return priv->name.get();
}

static const GVariantType* webkitContextMenuGActionGetParameterType(GAction*)
{
    return nullptr;
}

static gboolean webkitContextMenuGActionGetEnabled(GAction* action)
{
    auto* priv = WEBKIT_CONTEXT_MENU_GACTION(action)->priv;
    return priv->item.enabled();
}

static const GVariantType* webkitContextMenuGActionGetStateType(GAction* action)
{
    auto* priv = WEBKIT_CONTEXT_MENU_GACTION(action)->priv;
    return priv->state ? g_variant_get_type(priv->state.get()) : nullptr;
}

static GVariant* webkitContextMenuGActionGetState(GAction* action)
{
    auto* priv = WEBKIT_CONTEXT_MENU_GACTION(action)->priv;
    return priv->state ? g_variant_ref(priv->state.get()) : nullptr;
}

static GVariant* webkitContextMenuGActionGetStateHint(GAction*)
{
    return nullptr;
}

static void webkitContextMenuGActionChangeState(GAction* action, GVariant* value)
{
    RELEASE_ASSERT(value && g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN));
    auto* priv = WEBKIT_CONTEXT_MENU_GACTION(action)->priv;
    GRefPtr<GVariant> state(value);
    if (priv->state) {
        RELEASE_ASSERT(g_variant_is_of_type(value, g_variant_get_type(priv->state.get())));
        if (g_variant_equal(priv->state.get(), state.get()))
            return;
    }

    priv->state = WTF::move(state);
    g_object_notify(G_OBJECT(action), "state");
}

static void webkitContextMenuGActionActivate(GAction* action, GVariant*)
{
    auto* priv = WEBKIT_CONTEXT_MENU_GACTION(action)->priv;
    RefPtr<WebPageProxy> page = priv->page.get();
    if (!page)
        return;

    auto* proxy = page->activeContextMenu();
    if (!proxy)
        return;

    if (!priv->item.enabled())
        return;

    if (priv->state)
        g_action_change_state(action, g_variant_new_boolean(!g_variant_get_boolean(priv->state.get())));

#if PLATFORM(GTK) && !USE(GTK4)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (priv->gtkAction)
        gtk_action_activate(priv->gtkAction.get());
ALLOW_DEPRECATED_DECLARATIONS_END
#endif

    page->contextMenuItemSelected(priv->item, proxy->frameInfo());
}

static void webkitContextMenuGActionGActionInterfaceInit(GActionInterface* iface)
{
    iface->get_name = webkitContextMenuGActionGetName;
    iface->get_parameter_type = webkitContextMenuGActionGetParameterType;
    iface->get_enabled = webkitContextMenuGActionGetEnabled;
    iface->get_state_type = webkitContextMenuGActionGetStateType;
    iface->get_state = webkitContextMenuGActionGetState;
    iface->get_state_hint = webkitContextMenuGActionGetStateHint;
    iface->change_state = webkitContextMenuGActionChangeState;
    iface->activate = webkitContextMenuGActionActivate;
}

static void webkitContextMenuGActionGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    auto* action = G_ACTION(object);

    switch (propId) {
    case PROP_NAME:
        g_value_set_string(value, webkitContextMenuGActionGetName(action));
        break;
    case PROP_PARAMETER_TYPE:
        g_value_set_boxed(value, webkitContextMenuGActionGetParameterType(action));
        break;
    case PROP_ENABLED:
        g_value_set_boolean(value, webkitContextMenuGActionGetEnabled(action));
        break;
    case PROP_STATE_TYPE:
        g_value_set_boxed(value, webkitContextMenuGActionGetStateType(action));
        break;
    case PROP_STATE:
        g_value_take_variant(value, webkitContextMenuGActionGetState(action));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_context_menu_gaction_class_init(WebKitContextMenuGActionClass* actionClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(actionClass);
    objectClass->get_property = webkitContextMenuGActionGetProperty;

    g_object_class_override_property(objectClass, PROP_NAME, "name");
    g_object_class_override_property(objectClass, PROP_PARAMETER_TYPE, "parameter-type");
    g_object_class_override_property(objectClass, PROP_ENABLED, "enabled");
    g_object_class_override_property(objectClass, PROP_STATE_TYPE, "state-type");
    g_object_class_override_property(objectClass, PROP_STATE, "state");
}

GAction* webkitContextMenuGActionNew(const char* name, const WebContextMenuItemData& item)
{
    RELEASE_ASSERT(item.type() == WebCore::ContextMenuItemType::Action || item.type() == WebCore::ContextMenuItemType::CheckableAction);
    auto* action = WEBKIT_CONTEXT_MENU_GACTION(g_object_new(WEBKIT_TYPE_CONTEXT_MENU_GACTION, nullptr));
    if (name)
        action->priv->name.reset(g_strdup(name));
    else {
        static uint64_t actionID = 0;
        action->priv->name.reset(g_strdup_printf("action-%" PRIu64, ++actionID));
    }
    if (item.type() == WebCore::ContextMenuItemType::CheckableAction)
        action->priv->state = g_variant_new_boolean(item.checked());
    action->priv->item = item;

    return G_ACTION(action);
}

void webkitContextMenuGActionSetPage(WebKitContextMenuGAction* action, WebPageProxy* page)
{
    RELEASE_ASSERT(WEBKIT_IS_CONTEXT_MENU_GACTION(action));
    action->priv->page = page;
}

#if PLATFORM(GTK) && !USE(GTK4)
void webkitContextMenuGActionSetGtkAction(WebKitContextMenuGAction* action, GtkAction* gtkAction)
{
    RELEASE_ASSERT(WEBKIT_IS_CONTEXT_MENU_GACTION(action));
    action->priv->gtkAction = gtkAction;
}
#endif

#endif // ENABLE(CONTEXT_MENUS)
