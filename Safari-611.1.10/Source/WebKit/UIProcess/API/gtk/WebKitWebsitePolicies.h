/*
 * Copyright (C) 2020 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#pragma once

#include <gio/gio.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEBSITE_POLICIES            (webkit_website_policies_get_type())
#define WEBKIT_WEBSITE_POLICIES(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEBSITE_POLICIES, WebKitWebsitePolicies))
#define WEBKIT_IS_WEBSITE_POLICIES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEBSITE_POLICIES))
#define WEBKIT_WEBSITE_POLICIES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEBSITE_POLICIES, WebKitWebsitePoliciesClass))
#define WEBKIT_IS_WEBSITE_POLICIES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEBSITE_POLICIES))
#define WEBKIT_WEBSITE_POLICIES_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEBSITE_POLICIES, WebKitWebsitePoliciesClass))

typedef struct _WebKitWebsitePolicies        WebKitWebsitePolicies;
typedef struct _WebKitWebsitePoliciesClass   WebKitWebsitePoliciesClass;
typedef struct _WebKitWebsitePoliciesPrivate WebKitWebsitePoliciesPrivate;

struct _WebKitWebsitePolicies {
    GObject parent;

    WebKitWebsitePoliciesPrivate *priv;
};

struct _WebKitWebsitePoliciesClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

/**
 * WebKitAutoplayPolicy:
 * @WEBKIT_AUTOPLAY_ALLOW: Do not restrict autoplay.
 * @WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND: Allow videos to autoplay if
 *     they have no audio track, or if their audio track is muted.
 * @WEBKIT_AUTOPLAY_DENY: Never allow autoplay.
 *
 * Enum values used to specify autoplay policies.
 *
 * Since: 2.30
 */
typedef enum {
    WEBKIT_AUTOPLAY_ALLOW,
    WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND,
    WEBKIT_AUTOPLAY_DENY
} WebKitAutoplayPolicy;

WEBKIT_API GType
webkit_website_policies_get_type                              (void);

WEBKIT_API WebKitWebsitePolicies *
webkit_website_policies_new                                   (void);

WEBKIT_API WebKitWebsitePolicies *
webkit_website_policies_new_with_policies                     (const gchar *first_policy_name,
                                                               ...);

WEBKIT_API WebKitAutoplayPolicy
webkit_website_policies_get_autoplay_policy                   (WebKitWebsitePolicies *policies);

G_END_DECLS
