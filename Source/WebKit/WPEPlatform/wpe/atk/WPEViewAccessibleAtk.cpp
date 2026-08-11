/*
 * Copyright (C) 2025 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEViewAccessibleAtk.h"

#if USE(ATK)
#include "WPEAccessibilityAtk.h"
#include <wtf/glib/WTFGType.h>

struct _WPEViewAccessibleAtkPrivate {
    WPEView* view;
};

static void wpeViewAccessibleInterfaceInit(WPEViewAccessibleInterface*);

WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(
    WPEViewAccessibleAtk, wpe_view_accessible_atk, ATK_TYPE_SOCKET, AtkSocket,
    G_IMPLEMENT_INTERFACE(WPE_TYPE_VIEW_ACCESSIBLE, wpeViewAccessibleInterfaceInit))

static void viewDestroyedCallback(gpointer userData, GObject*)
{
    WPEViewAccessibleAtk* accessible = WPE_VIEW_ACCESSIBLE_ATK(userData);
    accessible->priv->view = nullptr;
    atk_object_notify_state_change(ATK_OBJECT(accessible), ATK_STATE_DEFUNCT, TRUE);
}

static void wpeViewAccessibleAtkDispose(GObject* object)
{
    WPEViewAccessibleAtk* accessible = WPE_VIEW_ACCESSIBLE_ATK(object);
    if (accessible->priv->view) {
        g_object_weak_unref(G_OBJECT(accessible->priv->view), viewDestroyedCallback, accessible);
        accessible->priv->view = nullptr;
    }

    G_OBJECT_CLASS(wpe_view_accessible_atk_parent_class)->dispose(object);
}

static void wpeViewAccessibleAtkInitialize(AtkObject* atkObject, gpointer data)
{
    if (ATK_OBJECT_CLASS(wpe_view_accessible_atk_parent_class)->initialize)
        ATK_OBJECT_CLASS(wpe_view_accessible_atk_parent_class)->initialize(atkObject, data);

    WPEViewAccessibleAtk* accessible = WPE_VIEW_ACCESSIBLE_ATK(atkObject);
    accessible->priv->view = WPE_VIEW(data);
    g_object_weak_ref(G_OBJECT(accessible->priv->view), viewDestroyedCallback, accessible);
    atk_object_set_role(atkObject, ATK_ROLE_FILLER);

    g_signal_connect_object(accessible->priv->view, "notify::has-focus", G_CALLBACK(+[](WPEViewAccessibleAtk* accessible) {
        atk_object_notify_state_change(ATK_OBJECT(accessible), ATK_STATE_FOCUSED, wpe_view_get_has_focus(accessible->priv->view));
    }), accessible, G_CONNECT_SWAPPED);
    g_signal_connect_object(accessible->priv->view, "notify::visible", G_CALLBACK(+[](WPEViewAccessibleAtk* accessible) {
        atk_object_notify_state_change(ATK_OBJECT(accessible), ATK_STATE_VISIBLE, wpe_view_get_visible(accessible->priv->view));
    }), accessible, G_CONNECT_SWAPPED);
    g_signal_connect_object(accessible->priv->view, "notify::mapped", G_CALLBACK(+[](WPEViewAccessibleAtk* accessible) {
        atk_object_notify_state_change(ATK_OBJECT(accessible), ATK_STATE_SHOWING, wpe_view_get_mapped(accessible->priv->view));
    }), accessible, G_CONNECT_SWAPPED);
}

static AtkStateSet* wpeViewAccessibleAtkRefStateSet(AtkObject* atkObject)
{
    WPEViewAccessibleAtk* accessible = WPE_VIEW_ACCESSIBLE_ATK(atkObject);
    if (!accessible->priv->view) {
        // If the view is no longer alive, save some remote calls (because of AtkSocket's implementation of ref_state_set())
        // and just return that this AtkObject is defunct.
        AtkStateSet* stateSet = atk_state_set_new();
        atk_state_set_add_state(stateSet, ATK_STATE_DEFUNCT);
        return stateSet;
    }

    // Use the implementation of AtkSocket if the view is still alive.
    AtkStateSet* stateSet = ATK_OBJECT_CLASS(wpe_view_accessible_atk_parent_class)->ref_state_set(atkObject);
    if (!atk_socket_is_occupied(ATK_SOCKET(atkObject)))
        atk_state_set_add_state(stateSet, ATK_STATE_TRANSIENT);

    atk_state_set_add_state(stateSet, ATK_STATE_FOCUSABLE);
    if (wpe_view_get_has_focus(accessible->priv->view))
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSED);

    if (wpe_view_get_visible(accessible->priv->view)) {
        atk_state_set_add_state(stateSet, ATK_STATE_VISIBLE);
        if (wpe_view_get_mapped(accessible->priv->view))
            atk_state_set_add_state(stateSet, ATK_STATE_SHOWING);
    }

    return stateSet;
}

static gint wpeViewAccessibleAtkGetIndexInParent(AtkObject* atkObject)
{
    AtkObject* atkParent = atk_object_get_parent(atkObject);
    if (!atkParent)
        return -1;

    guint count = atk_object_get_n_accessible_children(atkParent);
    for (guint i = 0; i < count; ++i) {
        AtkObject* child = atk_object_ref_accessible_child(atkParent, i);
        bool childIsObject = child == atkObject;
        g_object_unref(child);
        if (childIsObject)
            return i;
    }

    return -1;
}

static void wpe_view_accessible_atk_class_init(WPEViewAccessibleAtkClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->dispose = wpeViewAccessibleAtkDispose;

    AtkObjectClass* atkObjectClass = ATK_OBJECT_CLASS(klass);
    atkObjectClass->initialize = wpeViewAccessibleAtkInitialize;
    atkObjectClass->ref_state_set = wpeViewAccessibleAtkRefStateSet;
    atkObjectClass->get_index_in_parent = wpeViewAccessibleAtkGetIndexInParent;
}

static void wpeViewAccessibleAtkBind(WPEViewAccessible* accessible, const char* plugID)
{
    atk_socket_embed(ATK_SOCKET(accessible), const_cast<char*>(plugID));
    atk_object_notify_state_change(ATK_OBJECT(accessible), ATK_STATE_TRANSIENT, FALSE);
}

static void wpeViewAccessibleInterfaceInit(WPEViewAccessibleInterface* interface)
{
    interface->bind = wpeViewAccessibleAtkBind;
}

WPEViewAccessible* wpeViewAccessibleAtkNew(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);

    WPE::accessibilityAtkInit();

    auto* accessible = WPE_VIEW_ACCESSIBLE(g_object_new(WPE_TYPE_VIEW_ACCESSIBLE_ATK, nullptr));
    atk_object_initialize(ATK_OBJECT(accessible), view);
    return accessible;
}

#endif // USE(ATK)
