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
#include "WPEApplicationAccessibleAtk.h"

#if USE(ATK)
#include "WPEToplevelPrivate.h"
#include <wtf/Vector.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

struct _WPEApplicationAccessibleAtkPrivate {
    Vector<WPEToplevel*> toplevels;
};

WEBKIT_DEFINE_FINAL_TYPE(WPEApplicationAccessibleAtk, wpe_application_accessible_atk, ATK_TYPE_OBJECT, AtkObject)

static void wpeApplicationAccessibleAtkConstructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_application_accessible_atk_parent_class)->constructed(object);

    auto* accessible = WPE_APPLICATION_ACCESSIBLE_ATK(object);
    GUniquePtr<GList> toplevels(wpe_toplevel_list());
    for (GList* iter = toplevels.get(); iter; iter = g_list_next(iter))
        accessible->priv->toplevels.append(WPE_TOPLEVEL(iter->data));
}

static void wpeApplicationAccessibleAtkInitialize(AtkObject* atkObject, gpointer userData)
{
    ATK_OBJECT_CLASS(wpe_application_accessible_atk_parent_class)->initialize(atkObject, userData);

    atkObject->role = ATK_ROLE_APPLICATION;
    atkObject->accessible_parent = nullptr;
}

static gint wpeApplicationAccessibleAtkGetNChildren(AtkObject* atkObject)
{
    auto* accessible = WPE_APPLICATION_ACCESSIBLE_ATK(atkObject);
    return accessible->priv->toplevels.size();
}

static AtkObject* wpeApplicationAccessibleAtkRefChild(AtkObject* atkObject, int index)
{
    auto* accessible = WPE_APPLICATION_ACCESSIBLE_ATK(atkObject);
    if (index < 0 || static_cast<size_t>(index) >= accessible->priv->toplevels.size())
        return nullptr;

    auto* toplevel = accessible->priv->toplevels[index];
    return ATK_OBJECT(g_object_ref(wpeToplevelGetOrCreateAccessibleAtk(toplevel)));
}

static const char* wpeApplicationAccessibleAtkGetName(AtkObject*)
{
    return g_get_prgname();
}

static void wpe_application_accessible_atk_class_init(WPEApplicationAccessibleAtkClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = wpeApplicationAccessibleAtkConstructed;

    AtkObjectClass* atkObjectClass = ATK_OBJECT_CLASS(klass);
    atkObjectClass->initialize = wpeApplicationAccessibleAtkInitialize;
    atkObjectClass->get_n_children = wpeApplicationAccessibleAtkGetNChildren;
    atkObjectClass->ref_child = wpeApplicationAccessibleAtkRefChild;
    atkObjectClass->get_name = wpeApplicationAccessibleAtkGetName;
    atkObjectClass->get_parent = nullptr;
}

AtkObject* wpeApplicationAccessibleAtkNew()
{
    auto* accessible = ATK_OBJECT(g_object_new(WPE_TYPE_APPLICATION_ACCESSIBLE_ATK, nullptr));
    atk_object_initialize(accessible, nullptr);
    return accessible;
}

void wpeApplicationAccessibleAtkToplevelCreated(WPEApplicationAccessibleAtk* accessible, WPEToplevel* toplevel)
{
    accessible->priv->toplevels.append(toplevel);
    auto* toplevelAccessible = wpeToplevelGetOrCreateAccessibleAtk(toplevel);
    atk_object_set_parent(toplevelAccessible, ATK_OBJECT(accessible));
    g_signal_emit_by_name(accessible, "children-changed::add", accessible->priv->toplevels.size() - 1, toplevelAccessible, nullptr);
}

void wpeApplicationAccessibleAtkToplevelDestroyed(WPEApplicationAccessibleAtk* accessible, WPEToplevel* toplevel)
{
    auto* toplevelAccessible = wpeToplevelGetAccessibleAtk(toplevel);
    if (!toplevelAccessible)
        return;

    auto index = accessible->priv->toplevels.find(toplevel);
    if (index == notFound)
        return;

    accessible->priv->toplevels.removeAt(index);
    g_signal_emit_by_name(accessible, "children-changed::remove", index, toplevelAccessible, nullptr);
    atk_object_set_parent(toplevelAccessible, nullptr);
}

int wpeApplicationAccessibleAtkGetToplevelIndex(WPEApplicationAccessibleAtk* accessible, WPEToplevel* toplevel)
{
    auto index = accessible->priv->toplevels.find(toplevel);
    return index == notFound ? -1 : index;
}

#endif // USE(ATK)
