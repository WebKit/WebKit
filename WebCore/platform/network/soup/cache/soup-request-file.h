/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Red Hat, Inc.
 * Copyright (C) 2010 Igalia, S.L.
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

#ifndef WEBKIT_SOUP_REQUEST_FILE_H
#define WEBKIT_SOUP_REQUEST_FILE_H 1

#include "soup-request.h"

G_BEGIN_DECLS

#define WEBKIT_TYPE_SOUP_REQUEST_FILE            (webkit_soup_request_file_get_type ())
#define WEBKIT_SOUP_REQUEST_FILE(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), WEBKIT_TYPE_SOUP_REQUEST_FILE, WebKitSoupRequestFile))
#define WEBKIT_SOUP_REQUEST_FILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_SOUP_REQUEST_FILE, WebKitSoupRequestFileClass))
#define WEBKIT_IS_SOUP_REQUEST_FILE(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), WEBKIT_TYPE_SOUP_REQUEST_FILE))
#define WEBKIT_IS_SOUP_REQUEST_FILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), WEBKIT_TYPE_SOUP_REQUEST_FILE))
#define WEBKIT_SOUP_REQUEST_FILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), WEBKIT_TYPE_SOUP_REQUEST_FILE, WebKitSoupRequestFileClass))

typedef struct _WebKitSoupRequestFilePrivate WebKitSoupRequestFilePrivate;

typedef struct {
	WebKitSoupRequest parent;

	WebKitSoupRequestFilePrivate *priv;
} WebKitSoupRequestFile;

typedef struct {
	WebKitSoupRequestClass parent;
} WebKitSoupRequestFileClass;

GType webkit_soup_request_file_get_type (void);

GFile  *webkit_soup_request_file_get_file (WebKitSoupRequestFile *file);

G_END_DECLS

#endif /* WEBKIT_SOUP_REQUEST_FILE_H */
