/*
 * Copyright (C) 2019 Igalia S.L.
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

#include "WebKitAccessibleApplication.h"

#if defined(HAVE_ACCESSIBILITY) && HAVE_ACCESSIBILITY

struct _WebKitAccessibleApplicationPrivate {
    AtkObject* child;
};

static void webkitAccessibleApplicationWindowInterfaceInit(AtkWindowIface*)
{
}

G_DEFINE_TYPE_WITH_CODE(WebKitAccessibleApplication, webkit_accessible_application, ATK_TYPE_OBJECT,
    G_ADD_PRIVATE(WebKitAccessibleApplication)
    G_IMPLEMENT_INTERFACE(ATK_TYPE_WINDOW, webkitAccessibleApplicationWindowInterfaceInit))

static void webkitAccessibleApplicationFinalize(GObject* object)
{
    auto* accessible = WEBKIT_ACCESSIBLE_APPLICATION(object);
    webkitAccessibleApplicationSetChild(accessible, nullptr);

    G_OBJECT_CLASS(webkit_accessible_application_parent_class)->finalize(object);
}

static void webkitAccessibleApplicationInitialize(AtkObject* atkObject, gpointer data)
{
    ATK_OBJECT_CLASS(webkit_accessible_application_parent_class)->initialize(atkObject, data);
    atkObject->role = ATK_ROLE_APPLICATION;
    atkObject->accessible_parent = nullptr;
}

static gint webkitAccessibleApplicationGetNChildren(AtkObject* atkObject)
{
    auto* accessible = WEBKIT_ACCESSIBLE_APPLICATION(atkObject);
    return accessible->priv->child ? 1 : 0;
}

static AtkObject* webkitAccessibleApplicationRefChild(AtkObject* atkObject, int i)
{
    if (i)
        return nullptr;

    auto* accessible = WEBKIT_ACCESSIBLE_APPLICATION(atkObject);
    return accessible->priv->child ? ATK_OBJECT(g_object_ref(accessible->priv->child)) : nullptr;
}

static const char* webkitAccessibleApplicationGetName(AtkObject*)
{
    return g_get_prgname();
}

static void webkit_accessible_application_class_init(WebKitAccessibleApplicationClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->finalize = webkitAccessibleApplicationFinalize;

    AtkObjectClass* atkObjectClass = ATK_OBJECT_CLASS(klass);
    atkObjectClass->initialize = webkitAccessibleApplicationInitialize;
    atkObjectClass->get_n_children = webkitAccessibleApplicationGetNChildren;
    atkObjectClass->ref_child = webkitAccessibleApplicationRefChild;
    atkObjectClass->get_name = webkitAccessibleApplicationGetName;
    atkObjectClass->get_parent = nullptr;
}

static void webkit_accessible_application_init(WebKitAccessibleApplication* accessible)
{
    accessible->priv = static_cast<WebKitAccessibleApplicationPrivate*>(webkit_accessible_application_get_instance_private(accessible));
}

WebKitAccessibleApplication* webkitAccessibleApplicationNew()
{
    auto* accessible = ATK_OBJECT(g_object_new(WEBKIT_TYPE_ACCESSIBLE_APPLICATION, nullptr));
    atk_object_initialize(accessible, nullptr);
    return WEBKIT_ACCESSIBLE_APPLICATION(accessible);
}

void webkitAccessibleApplicationSetChild(WebKitAccessibleApplication* accessible, AtkObject* child)
{
    g_return_if_fail(WEBKIT_IS_ACCESSIBLE_APPLICATION(accessible));

    if (accessible->priv->child == child)
        return;

    if (accessible->priv->child) {
        g_signal_emit_by_name(accessible, "children-changed::remove", 0, accessible->priv->child);
        atk_object_set_parent(accessible->priv->child, nullptr);
    }

    accessible->priv->child = child;

    if (accessible->priv->child) {
        atk_object_set_parent(child, ATK_OBJECT(accessible));
        g_signal_emit_by_name(accessible, "children-changed::add", 0, child);
    }
}

#endif // HAVE(ACCESSIBILITY)
