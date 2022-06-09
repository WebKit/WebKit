/*
 * Copyright (C) 2012 Igalia S.L.
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

#if !defined(__WEBKIT_WEB_EXTENSION_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit-web-extension.h> can be included directly."
#endif

#ifndef WebKitWebPage_h
#define WebKitWebPage_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>
#include <webkit2/WebKitFrame.h>
#include <webkit2/WebKitUserMessage.h>
#include <webkit2/WebKitWebEditor.h>
#include <webkitdom/webkitdom.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_PAGE            (webkit_web_page_get_type())
#define WEBKIT_WEB_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_PAGE, WebKitWebPage))
#define WEBKIT_IS_WEB_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_PAGE))
#define WEBKIT_WEB_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_PAGE, WebKitWebPageClass))
#define WEBKIT_IS_WEB_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_PAGE))
#define WEBKIT_WEB_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_PAGE, WebKitWebPageClass))

typedef struct _WebKitWebPage        WebKitWebPage;
typedef struct _WebKitWebPageClass   WebKitWebPageClass;
typedef struct _WebKitWebPagePrivate WebKitWebPagePrivate;

/* Forward declarations */
typedef struct _WebKitWebEditor      WebKitWebEditor;

/**
 * WebKitFormSubmissionStep:
 * @WEBKIT_FORM_SUBMISSION_WILL_SEND_DOM_EVENT: indicates the form's
 * DOM submit event is about to be emitted.
 * @WEBKIT_FORM_SUBMISSION_WILL_COMPLETE: indicates the form is about
 * to be submitted.
 *
 * Used to indicate a particular stage in form submission. See
 * #WebKitWebPage::will-submit-form.
 *
 * Since: 2.20
 */
typedef enum {
    WEBKIT_FORM_SUBMISSION_WILL_SEND_DOM_EVENT,
    WEBKIT_FORM_SUBMISSION_WILL_COMPLETE,
} WebKitFormSubmissionStep;

struct _WebKitWebPage {
    GObject parent;

    WebKitWebPagePrivate *priv;
};

struct _WebKitWebPageClass {
    GObjectClass parent_class;
};

WEBKIT_API GType
webkit_web_page_get_type                    (void);

WEBKIT_API WebKitDOMDocument *
webkit_web_page_get_dom_document            (WebKitWebPage *web_page);

WEBKIT_API guint64
webkit_web_page_get_id                      (WebKitWebPage *web_page);

WEBKIT_API const gchar *
webkit_web_page_get_uri                     (WebKitWebPage *web_page);

WEBKIT_API WebKitFrame *
webkit_web_page_get_main_frame              (WebKitWebPage *web_page);

WEBKIT_API WebKitWebEditor *
webkit_web_page_get_editor                  (WebKitWebPage *web_page);

WEBKIT_API void
webkit_web_page_send_message_to_view        (WebKitWebPage      *web_page,
                                             WebKitUserMessage  *message,
                                             GCancellable       *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer            user_data);

WEBKIT_API WebKitUserMessage *
webkit_web_page_send_message_to_view_finish (WebKitWebPage      *web_page,
                                             GAsyncResult       *result,
                                             GError            **error);

G_END_DECLS

#endif
