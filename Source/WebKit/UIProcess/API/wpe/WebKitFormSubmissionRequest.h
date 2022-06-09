/*
 * Copyright (C) 2012 Igalia S.L.
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

#ifndef WebKitFormSubmissionRequest_h
#define WebKitFormSubmissionRequest_h

#include <glib-object.h>
#include <wpe/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_FORM_SUBMISSION_REQUEST            (webkit_form_submission_request_get_type())
#define WEBKIT_FORM_SUBMISSION_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_FORM_SUBMISSION_REQUEST, WebKitFormSubmissionRequest))
#define WEBKIT_IS_FORM_SUBMISSION_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_FORM_SUBMISSION_REQUEST))
#define WEBKIT_FORM_SUBMISSION_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_FORM_SUBMISSION_REQUEST, WebKitFormSubmissionRequestClass))
#define WEBKIT_IS_FORM_SUBMISSION_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_FORM_SUBMISSION_REQUEST))
#define WEBKIT_FORM_SUBMISSION_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_FORM_SUBMISSION_REQUEST, WebKitFormSubmissionRequestClass))

typedef struct _WebKitFormSubmissionRequest        WebKitFormSubmissionRequest;
typedef struct _WebKitFormSubmissionRequestClass   WebKitFormSubmissionRequestClass;
typedef struct _WebKitFormSubmissionRequestPrivate WebKitFormSubmissionRequestPrivate;

struct _WebKitFormSubmissionRequest {
    GObject parent;

    /*< private >*/
    WebKitFormSubmissionRequestPrivate *priv;
};

struct _WebKitFormSubmissionRequestClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_form_submission_request_get_type         (void);

WEBKIT_API gboolean
webkit_form_submission_request_list_text_fields (WebKitFormSubmissionRequest  *request,
                                                 GPtrArray                   **field_names,
                                                 GPtrArray                   **field_values);

WEBKIT_API void
webkit_form_submission_request_submit           (WebKitFormSubmissionRequest  *request);

G_END_DECLS

#endif
