/*
 * Copyright (C) 2013 Samsung Electronics Inc. All rights reserved.
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

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitCredential_h
#define WebKitCredential_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_CREDENTIAL (webkit_credential_get_type())

typedef struct _WebKitCredential WebKitCredential;

/**
 * WebKitCredentialPersistence:
 * @WEBKIT_CREDENTIAL_PERSISTENCE_NONE: Credential does not persist
 * @WEBKIT_CREDENTIAL_PERSISTENCE_FOR_SESSION: Credential persists for session only
 * @WEBKIT_CREDENTIAL_PERSISTENCE_PERMANENT: Credential persists permanently
 *
 * Enum values representing the duration for which a credential persists.
 *
 * Since: 2.2
 */
typedef enum {
    WEBKIT_CREDENTIAL_PERSISTENCE_NONE,
    WEBKIT_CREDENTIAL_PERSISTENCE_FOR_SESSION,
    WEBKIT_CREDENTIAL_PERSISTENCE_PERMANENT
} WebKitCredentialPersistence;

WEBKIT_API GType
webkit_credential_get_type             (void);

WEBKIT_API WebKitCredential *
webkit_credential_new                  (const gchar                 *username,
                                        const gchar                 *password,
                                        WebKitCredentialPersistence  persistence);

WEBKIT_API WebKitCredential *
webkit_credential_copy                 (WebKitCredential            *credential);

WEBKIT_API void
webkit_credential_free                 (WebKitCredential            *credential);

WEBKIT_API const gchar *
webkit_credential_get_username         (WebKitCredential            *credential);

WEBKIT_API const gchar *
webkit_credential_get_password         (WebKitCredential            *credential);

WEBKIT_API gboolean
webkit_credential_has_password         (WebKitCredential            *credential);

WEBKIT_API WebKitCredentialPersistence
webkit_credential_get_persistence      (WebKitCredential            *credential);

G_END_DECLS

#endif
