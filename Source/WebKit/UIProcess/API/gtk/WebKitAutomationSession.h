/*
 * Copyright (C) 2017 Igalia S.L.
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

#ifndef WebKitAutomationSession_h
#define WebKitAutomationSession_h

#include <glib-object.h>
#include <webkit2/WebKitApplicationInfo.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_AUTOMATION_SESSION            (webkit_automation_session_get_type())
#define WEBKIT_AUTOMATION_SESSION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_AUTOMATION_SESSION, WebKitAutomationSession))
#define WEBKIT_IS_AUTOMATION_SESSION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_AUTOMATION_SESSION))
#define WEBKIT_AUTOMATION_SESSION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_AUTOMATION_SESSION, WebKitAutomationSessionClass))
#define WEBKIT_IS_AUTOMATION_SESSION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_AUTOMATION_SESSION))
#define WEBKIT_AUTOMATION_SESSION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_AUTOMATION_SESSION, WebKitAutomationSessionClass))

typedef struct _WebKitAutomationSession        WebKitAutomationSession;
typedef struct _WebKitAutomationSessionClass   WebKitAutomationSessionClass;
typedef struct _WebKitAutomationSessionPrivate WebKitAutomationSessionPrivate;

struct _WebKitAutomationSession {
    GObject parent;

    WebKitAutomationSessionPrivate *priv;
};

struct _WebKitAutomationSessionClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_automation_session_get_type             (void);

WEBKIT_API const char *
webkit_automation_session_get_id               (WebKitAutomationSession *session);

WEBKIT_API void
webkit_automation_session_set_application_info (WebKitAutomationSession *session,
                                                WebKitApplicationInfo   *info);

WEBKIT_API WebKitApplicationInfo *
webkit_automation_session_get_application_info (WebKitAutomationSession *session);

G_END_DECLS

#endif /* WebKitAutomationSession_h */
