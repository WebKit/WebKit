/*
 * Copyright (C) 2008 Jasper Bryant-Greene <jasper@unix.geek.nz>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "webkitnavigationaction.h"

extern "C" {

G_DEFINE_TYPE(WebKitNavigationAction, webkit_navigation_action, G_TYPE_OBJECT);

struct _WebKitNavigationActionPrivate {
    gint button;
    gint modifierFlags;
    gint navigationType;
    gchar* originalURL;
};

#define WEBKIT_NAVIGATION_ACTION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_NAVIGATION_ACTION, WebKitNavigationActionPrivate))

static void webkit_navigation_action_finalize(GObject* object)
{
    WebKitNavigationAction* action = WEBKIT_NAVIGATION_ACTION(object);
    WebKitNavigationActionPrivate* priv = action->priv;

    g_free(priv->originalURL);

    G_OBJECT_CLASS(webkit_navigation_action_parent_class)->finalize(object);
}

static void webkit_navigation_action_class_init(WebKitNavigationActionClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = webkit_navigation_action_finalize;

    g_type_class_add_private(klass, sizeof(WebKitNavigationActionPrivate));
}

static void webkit_navigation_action_init(WebKitNavigationAction* action)
{
    WebKitNavigationActionPrivate* priv = WEBKIT_NAVIGATION_ACTION_GET_PRIVATE(action);
    action->priv = priv;
}

gint webkit_navigation_action_get_button(WebKitNavigationAction* action)
{
    g_return_val_if_fail(WEBKIT_IS_NAVIGATION_ACTION(action), 0);
    return action->priv->button;
}

gint webkit_navigation_action_get_modifier_flags(WebKitNavigationAction* action)
{
    g_return_val_if_fail(WEBKIT_IS_NAVIGATION_ACTION(action), 0);
    return action->priv->modifierFlags;
}

gint webkit_navigation_action_get_navigation_type(WebKitNavigationAction* action)
{
    g_return_val_if_fail(WEBKIT_IS_NAVIGATION_ACTION(action), 0);
    return action->priv->navigationType;
}

G_CONST_RETURN gchar* webkit_navigation_action_get_original_url(WebKitNavigationAction* action)
{
    g_return_val_if_fail(WEBKIT_IS_NAVIGATION_ACTION(action), NULL);
    return action->priv->originalURL;
}

}
