/*
 * Copyright (C) 2015 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebContextMenuItemGlib.h"

#include "APIObject.h"
#include <gio/gio.h>

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#endif

namespace WebKit {
using namespace WebCore;

WebContextMenuItemGlib::WebContextMenuItemGlib(ContextMenuItemType type, ContextMenuAction action, const String& title, bool enabled, bool checked)
    : WebContextMenuItemData(type, action, title, enabled, checked)
{
    ASSERT(type != SubmenuType);
    createActionIfNeeded();
}

WebContextMenuItemGlib::WebContextMenuItemGlib(const WebContextMenuItemData& data)
    : WebContextMenuItemData(data.type() == SubmenuType ? ActionType : data.type(), data.action(), data.title(), data.enabled(), data.checked())
{
    createActionIfNeeded();
}

WebContextMenuItemGlib::WebContextMenuItemGlib(const WebContextMenuItemGlib& data, Vector<WebContextMenuItemGlib>&& submenu)
    : WebContextMenuItemData(ActionType, data.action(), data.title(), data.enabled(), false)
{
    m_gAction = data.gAction();
    m_submenuItems = WTFMove(submenu);
#if PLATFORM(GTK)
    m_gtkAction = data.gtkAction();
#endif
}

static bool isGActionChecked(GAction* action)
{
    if (!g_action_get_state_type(action))
        return false;

    ASSERT(g_variant_type_equal(g_action_get_state_type(action), G_VARIANT_TYPE_BOOLEAN));
    GRefPtr<GVariant> state = adoptGRef(g_action_get_state(action));
    return g_variant_get_boolean(state.get());
}

WebContextMenuItemGlib::WebContextMenuItemGlib(GAction* action, const String& title, GVariant* target)
    : WebContextMenuItemData(g_action_get_state_type(action) ? CheckableActionType : ActionType, ContextMenuItemBaseApplicationTag, title, g_action_get_enabled(action), isGActionChecked(action))
    , m_gAction(action)
    , m_gActionTarget(target)
{
    createActionIfNeeded();
}

#if PLATFORM(GTK)
WebContextMenuItemGlib::WebContextMenuItemGlib(GtkAction* action)
    : WebContextMenuItemData(GTK_IS_TOGGLE_ACTION(action) ? CheckableActionType : ActionType, ContextMenuItemBaseApplicationTag, String::fromUTF8(gtk_action_get_label(action)), gtk_action_get_sensitive(action), GTK_IS_TOGGLE_ACTION(action) ? gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action)) : false)
{
    m_gtkAction = action;
    createActionIfNeeded();
    g_object_set_data_full(G_OBJECT(m_gAction.get()), "webkit-gtk-action", g_object_ref(m_gtkAction), g_object_unref);
}
#endif

WebContextMenuItemGlib::~WebContextMenuItemGlib()
{
}

GUniquePtr<char> WebContextMenuItemGlib::buildActionName() const
{
#if PLATFORM(GTK)
    if (m_gtkAction)
        return GUniquePtr<char>(g_strdup(gtk_action_get_name(m_gtkAction)));
#endif

    static uint64_t actionID = 0;
    return GUniquePtr<char>(g_strdup_printf("action-%" PRIu64, ++actionID));
}

void WebContextMenuItemGlib::createActionIfNeeded()
{
    if (type() == SeparatorType)
        return;

    if (!m_gAction) {
        auto actionName = buildActionName();
        if (type() == CheckableActionType)
            m_gAction = adoptGRef(G_ACTION(g_simple_action_new_stateful(actionName.get(), nullptr, g_variant_new_boolean(checked()))));
        else
            m_gAction = adoptGRef(G_ACTION(g_simple_action_new(actionName.get(), nullptr)));
        g_simple_action_set_enabled(G_SIMPLE_ACTION(m_gAction.get()), enabled());
    }

#if PLATFORM(GTK)
    // Create the GtkAction for backwards compatibility only.
    if (!m_gtkAction) {
        if (type() == CheckableActionType) {
            m_gtkAction = GTK_ACTION(gtk_toggle_action_new(g_action_get_name(m_gAction.get()), title().utf8().data(), nullptr, nullptr));
            gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(m_gtkAction), checked());
        } else
            m_gtkAction = gtk_action_new(g_action_get_name(m_gAction.get()), title().utf8().data(), 0, nullptr);
        gtk_action_set_sensitive(m_gtkAction, enabled());
        g_object_set_data_full(G_OBJECT(m_gAction.get()), "webkit-gtk-action", m_gtkAction, g_object_unref);
    }

    g_signal_connect_object(m_gAction.get(), "activate", G_CALLBACK(gtk_action_activate), m_gtkAction, G_CONNECT_SWAPPED);
#endif
}

} // namespace WebKit
