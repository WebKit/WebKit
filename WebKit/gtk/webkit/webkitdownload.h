/*
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
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

#ifndef webkitdownload_h
#define webkitdownload_h

#include <webkit/webkitdefines.h>

#include <glib-object.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_DOWNLOAD            (webkit_download_get_type())
#define WEBKIT_DOWNLOAD(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_DOWNLOAD, WebKitDownload))
#define WEBKIT_DOWNLOAD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_DOWNLOAD, WebKitDownloadClass))
#define WEBKIT_IS_DOWNLOAD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_DOWNLOAD))
#define WEBKIT_IS_DOWNLOAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_DOWNLOAD))
#define WEBKIT_DOWNLOAD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_DOWNLOAD, WebKitDownloadClass))

typedef enum {
    WEBKIT_DOWNLOAD_STATUS_ERROR = -1,
    WEBKIT_DOWNLOAD_STATUS_CREATED = 0,
    WEBKIT_DOWNLOAD_STATUS_STARTED,
    WEBKIT_DOWNLOAD_STATUS_CANCELLED,
    WEBKIT_DOWNLOAD_STATUS_FINISHED
} WebKitDownloadStatus;

typedef enum {
    WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER,
    WEBKIT_DOWNLOAD_ERROR_DESTINATION,
    WEBKIT_DOWNLOAD_ERROR_NETWORK
} WebKitDownloadError;

typedef struct _WebKitDownloadPrivate WebKitDownloadPrivate;

struct _WebKitDownload {
    GObject parent_instance;

    WebKitDownloadPrivate *priv;
};

struct _WebKitDownloadClass {
    GObjectClass parent_class;

    /* Padding for future expansion */
    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_download_get_type                    (void);

WEBKIT_API WebKitDownload*
webkit_download_new                         (WebKitNetworkRequest *request);

WEBKIT_API void
webkit_download_start                       (WebKitDownload       *download);

WEBKIT_API void
webkit_download_cancel                      (WebKitDownload       *download);

WEBKIT_API const gchar*
webkit_download_get_uri                     (WebKitDownload       *download);

WEBKIT_API WebKitNetworkRequest*
webkit_download_get_network_request         (WebKitDownload       *download);

WEBKIT_API const gchar*
webkit_download_get_suggested_filename      (WebKitDownload       *download);

WEBKIT_API const gchar*
webkit_download_get_destination_uri         (WebKitDownload       *download);

WEBKIT_API void
webkit_download_set_destination_uri         (WebKitDownload       *download,
                                             const gchar          *destination_uri);

WEBKIT_API gdouble
webkit_download_get_progress                (WebKitDownload       *download);

WEBKIT_API gdouble
webkit_download_get_elapsed_time            (WebKitDownload       *download);

WEBKIT_API guint64
webkit_download_get_total_size              (WebKitDownload       *download);

WEBKIT_API guint64
webkit_download_get_current_size            (WebKitDownload       *download);

WEBKIT_API WebKitDownloadStatus
webkit_download_get_status                  (WebKitDownload       *download);

G_END_DECLS

#endif
