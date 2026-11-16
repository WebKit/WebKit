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
#include "WPEToplevelAccessibleAtk.h"

#if USE(ATK)
#include "WPEApplicationAccessibleAtk.h"
#include "WPEToplevelPrivate.h"
#include "WPEViewAccessibleAtk.h"
#include <wtf/glib/WTFGType.h>

struct _WPEToplevelAccessibleAtkPrivate {
    WPEToplevel* toplevel;
};

static void wpeToplevelAccessibleAtkComponentInterfaceInit(AtkComponentIface*);
static void wpeToplevelAccessibleAtkWindowInterfaceInit(AtkWindowIface*);

WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(
    WPEToplevelAccessibleAtk, wpe_toplevel_accessible_atk, ATK_TYPE_OBJECT, AtkObject,
    G_IMPLEMENT_INTERFACE(ATK_TYPE_COMPONENT, wpeToplevelAccessibleAtkComponentInterfaceInit)
    G_IMPLEMENT_INTERFACE(ATK_TYPE_WINDOW, wpeToplevelAccessibleAtkWindowInterfaceInit))

static void toplevelDestroyedCallback(gpointer userData, GObject*)
{
    WPEToplevelAccessibleAtk* accessible = WPE_TOPLEVEL_ACCESSIBLE_ATK(userData);
    accessible->priv->toplevel = nullptr;
    atk_object_notify_state_change(ATK_OBJECT(accessible), ATK_STATE_DEFUNCT, TRUE);
}

static void wpeToplevelAccessibleAtkDispose(GObject* object)
{
    WPEToplevelAccessibleAtk* accessible = WPE_TOPLEVEL_ACCESSIBLE_ATK(object);
    if (accessible->priv->toplevel) {
        g_object_weak_unref(G_OBJECT(accessible->priv->toplevel), toplevelDestroyedCallback, accessible);
        accessible->priv->toplevel = nullptr;
    }

    G_OBJECT_CLASS(wpe_toplevel_accessible_atk_parent_class)->dispose(object);
}

static void wpeToplevelAccessibleAtkInitialize(AtkObject* atkObject, gpointer data)
{
    if (ATK_OBJECT_CLASS(wpe_toplevel_accessible_atk_parent_class)->initialize)
        ATK_OBJECT_CLASS(wpe_toplevel_accessible_atk_parent_class)->initialize(atkObject, data);

    WPEToplevelAccessibleAtk* accessible = WPE_TOPLEVEL_ACCESSIBLE_ATK(atkObject);
    accessible->priv->toplevel = WPE_TOPLEVEL(data);
    g_object_weak_ref(G_OBJECT(accessible->priv->toplevel), toplevelDestroyedCallback, accessible);
    atk_object_set_role(atkObject, ATK_ROLE_FRAME);
}

static AtkStateSet* wpeToplevelAccessibleAtkRefStateSet(AtkObject* atkObject)
{
    WPEToplevelAccessibleAtk* accessible = WPE_TOPLEVEL_ACCESSIBLE_ATK(atkObject);
    AtkStateSet* stateSet = ATK_OBJECT_CLASS(wpe_toplevel_accessible_atk_parent_class)->ref_state_set(atkObject);
    if (!accessible->priv->toplevel)
        atk_state_set_add_state(stateSet, ATK_STATE_DEFUNCT);
    else {
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSABLE);

        if (wpe_toplevel_get_state(accessible->priv->toplevel) & WPE_TOPLEVEL_STATE_ACTIVE) {
            atk_state_set_add_state(stateSet, ATK_STATE_ACTIVE);
            atk_state_set_add_state(stateSet, ATK_STATE_VISIBLE);
            atk_state_set_add_state(stateSet, ATK_STATE_SHOWING);
        }
    }

    return stateSet;
}

static gint wpeToplevelAccessibleAtkGetIndexInParent(AtkObject* atkObject)
{
    WPEToplevelAccessibleAtk* accessible = WPE_TOPLEVEL_ACCESSIBLE_ATK(atkObject);
    if (!accessible->priv->toplevel)
        return -1;

    auto* atkRoot = atk_get_root();
    if (!WPE_IS_APPLICATION_ACCESSIBLE_ATK(atkRoot))
        return -1;

    return wpeApplicationAccessibleAtkGetToplevelIndex(WPE_APPLICATION_ACCESSIBLE_ATK(atkRoot), accessible->priv->toplevel);
}

static gint wpeToplevelAccessibleAtkGetNChildren(AtkObject* atkObject)
{
    WPEToplevelAccessibleAtk* accessible = WPE_TOPLEVEL_ACCESSIBLE_ATK(atkObject);
    if (!accessible->priv->toplevel)
        return 0;

    return wpe_toplevel_get_n_views(accessible->priv->toplevel);
}

static AtkObject* wpeToplevelAccessibleAtkRefChild(AtkObject* atkObject, gint index)
{
    WPEToplevelAccessibleAtk* accessible = WPE_TOPLEVEL_ACCESSIBLE_ATK(atkObject);
    if (!accessible->priv->toplevel)
        return nullptr;

    if (index < 0 || static_cast<unsigned>(index) >= wpe_toplevel_get_n_views(accessible->priv->toplevel))
        return nullptr;

    auto view = wpeToplevelGetView(accessible->priv->toplevel, index);
    if (!view)
        return nullptr;

    auto* viewAccessible = wpe_view_get_accessible(view.get());
    if (!WPE_IS_VIEW_ACCESSIBLE_ATK(viewAccessible))
        return nullptr;

    return ATK_OBJECT(g_object_ref(viewAccessible));
}

static void wpe_toplevel_accessible_atk_class_init(WPEToplevelAccessibleAtkClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->dispose = wpeToplevelAccessibleAtkDispose;

    AtkObjectClass* atkObjectClass = ATK_OBJECT_CLASS(klass);
    atkObjectClass->initialize = wpeToplevelAccessibleAtkInitialize;
    atkObjectClass->ref_state_set = wpeToplevelAccessibleAtkRefStateSet;
    atkObjectClass->get_index_in_parent = wpeToplevelAccessibleAtkGetIndexInParent;
    atkObjectClass->get_n_children = wpeToplevelAccessibleAtkGetNChildren;
    atkObjectClass->ref_child = wpeToplevelAccessibleAtkRefChild;
}

static void wpeToplevelAccessibleAtkGetSize(AtkComponent* atkComponent, gint* width, gint* height)
{
    WPEToplevelAccessibleAtk* accessible = WPE_TOPLEVEL_ACCESSIBLE_ATK(atkComponent);
    if (accessible->priv->toplevel)
        wpe_toplevel_get_size(accessible->priv->toplevel, width, height);
}

static void wpeToplevelAccessibleAtkComponentInterfaceInit(AtkComponentIface* interface)
{
    interface->get_size = wpeToplevelAccessibleAtkGetSize;
}

static void wpeToplevelAccessibleAtkWindowInterfaceInit(AtkWindowIface*)
{
}

AtkObject* wpeToplevelAccessibleAtkNew(WPEToplevel* toplevel)
{
    auto* accessible = ATK_OBJECT(g_object_new(WPE_TYPE_TOPLEVEL_ACCESSIBLE_ATK, nullptr));
    atk_object_initialize(accessible, toplevel);
    return accessible;
}

#endif // USE(ATK)
