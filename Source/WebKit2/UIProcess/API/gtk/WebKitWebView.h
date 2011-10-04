/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef WebKitWebView_h
#define WebKitWebView_h

#include <webkit2/WebKitWebContext.h>
#include <webkit2/WebKitWebLoaderClient.h>
#include <webkit2/WebKitWebViewBase.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_VIEW            (webkit_web_view_get_type())
#define WEBKIT_WEB_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_VIEW, WebKitWebView))
#define WEBKIT_WEB_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_VIEW, WebKitWebViewClass))
#define WEBKIT_IS_WEB_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_VIEW))
#define WEBKIT_IS_WEB_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_VIEW))
#define WEBKIT_WEB_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_VIEW, WebKitWebViewClass))

typedef struct _WebKitWebView        WebKitWebView;
typedef struct _WebKitWebViewClass   WebKitWebViewClass;
typedef struct _WebKitWebViewPrivate WebKitWebViewPrivate;

struct _WebKitWebView {
    WebKitWebViewBase parent;

    /*< private >*/
    WebKitWebViewPrivate *priv;
};

struct _WebKitWebViewClass {
    WebKitWebViewBaseClass parent;

    /* Padding for future expansion */
    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
    void (*_webkit_reserved4) (void);
    void (*_webkit_reserved5) (void);
    void (*_webkit_reserved6) (void);
    void (*_webkit_reserved7) (void);
};

WK_EXPORT GType
webkit_web_view_get_type            (void);

WK_EXPORT GtkWidget *
webkit_web_view_new                 (void);

WK_EXPORT GtkWidget *
webkit_web_view_new_with_context    (WebKitWebContext      *context);

WK_EXPORT WebKitWebContext *
webkit_web_view_get_context         (WebKitWebView         *web_view);

WK_EXPORT WebKitWebLoaderClient *
webkit_web_view_get_loader_client   (WebKitWebView         *web_view);

WK_EXPORT void
webkit_web_view_set_loader_client   (WebKitWebView         *web_view,
                                     WebKitWebLoaderClient *loader_client);

WK_EXPORT void
webkit_web_view_load_uri            (WebKitWebView         *web_view,
                                     const gchar           *uri);

WK_EXPORT void
webkit_web_view_load_alternate_html (WebKitWebView         *web_view,
                                     const gchar           *content,
                                     const gchar           *base_uri,
                                     const gchar           *unreachable_uri);

WK_EXPORT void
webkit_web_view_go_back             (WebKitWebView         *web_view);

WK_EXPORT void
webkit_web_view_go_forward          (WebKitWebView         *web_view);

G_END_DECLS

#endif
