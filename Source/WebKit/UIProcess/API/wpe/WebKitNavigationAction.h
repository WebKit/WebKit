/*
 * Copyright (C) 2014 Igalia S.L.
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

#if !defined(__WEBKIT_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit.h> can be included directly."
#endif

#ifndef WebKitNavigationAction_h
#define WebKitNavigationAction_h

#include <glib-object.h>
#include <wpe/WebKitDefines.h>
#include <wpe/WebKitURIRequest.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_NAVIGATION_ACTION (webkit_navigation_action_get_type())

/**
 * WebKitNavigationType:
 * @WEBKIT_NAVIGATION_TYPE_LINK_CLICKED: The navigation was triggered by clicking a link.
 * @WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED: The navigation was triggered by submitting a form.
 * @WEBKIT_NAVIGATION_TYPE_BACK_FORWARD: The navigation was triggered by navigating forward or backward.
 * @WEBKIT_NAVIGATION_TYPE_RELOAD: The navigation was triggered by reloading.
 * @WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED: The navigation was triggered by resubmitting a form.
 * @WEBKIT_NAVIGATION_TYPE_OTHER: The navigation was triggered by some other action.
 *
 * Enum values used to denote the various navigation types.
 */
typedef enum {
    WEBKIT_NAVIGATION_TYPE_LINK_CLICKED,
    WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED,
    WEBKIT_NAVIGATION_TYPE_BACK_FORWARD,
    WEBKIT_NAVIGATION_TYPE_RELOAD,
    WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED,
    WEBKIT_NAVIGATION_TYPE_OTHER
} WebKitNavigationType;

typedef struct _WebKitNavigationAction WebKitNavigationAction;


WEBKIT_API GType
webkit_navigation_action_get_type            (void);

WEBKIT_API WebKitNavigationAction *
webkit_navigation_action_copy                (WebKitNavigationAction *navigation);

WEBKIT_API void
webkit_navigation_action_free                (WebKitNavigationAction *navigation);

WEBKIT_API WebKitNavigationType
webkit_navigation_action_get_navigation_type (WebKitNavigationAction *navigation);

WEBKIT_API guint
webkit_navigation_action_get_mouse_button    (WebKitNavigationAction *navigation);

WEBKIT_API guint
webkit_navigation_action_get_modifiers       (WebKitNavigationAction *navigation);

WEBKIT_API WebKitURIRequest *
webkit_navigation_action_get_request         (WebKitNavigationAction *navigation);

WEBKIT_API gboolean
webkit_navigation_action_is_user_gesture     (WebKitNavigationAction *navigation);

WEBKIT_API gboolean
webkit_navigation_action_is_redirect         (WebKitNavigationAction *navigation);

G_END_DECLS

#endif
