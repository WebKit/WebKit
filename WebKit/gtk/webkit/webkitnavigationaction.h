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

#ifndef WEBKIT_NAVIGATION_ACTION_H
#define WEBKIT_NAVIGATION_ACTION_H

#include <glib-object.h>
#include <glib.h>

#include <webkit/webkitdefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_NAVIGATION_ACTION            (webkit_navigation_action_get_type())
#define WEBKIT_NAVIGATION_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_NAVIGATION_ACTION, WebKitNavigationAction))
#define WEBKIT_NAVIGATION_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_NAVIGATION_ACTION, WebKitNavigationActionClass))
#define WEBKIT_IS_NAVIGATION_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_NAVIGATION_ACTION))
#define WEBKIT_IS_NAVIGATION_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_NAVIGATION_ACTION))
#define WEBKIT_NAVIGATION_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_NAVIGATION_ACTION, WebKitNavigationActionClass))

typedef struct _WebKitNavigationActionPrivate WebKitNavigationActionPrivate;

struct _WebKitNavigationAction {
    GObject parent_instance;

    WebKitNavigationActionPrivate* priv;
};

struct _WebKitNavigationActionClass {
    GObjectClass parent_class;
};

WEBKIT_API GType
webkit_navigation_action_get_type (void);

WEBKIT_API gint
webkit_navigation_action_get_button (WebKitNavigationAction* action);

WEBKIT_API gint
webkit_navigation_action_get_modifier_flags (WebKitNavigationAction* action);

WEBKIT_API gint
webkit_navigation_action_get_navigation_type (WebKitNavigationAction* action);

WEBKIT_API G_CONST_RETURN gchar*
webkit_navigation_action_get_original_url (WebKitNavigationAction* action);

G_END_DECLS

#endif
