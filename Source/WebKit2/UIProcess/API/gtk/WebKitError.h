/*
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2008 Luca Bruno <lethalman88@gmail.com>
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

#ifndef WebKitError_h
#define WebKitError_h

#include <WebKit2/WKBase.h>
#include <glib.h>

G_BEGIN_DECLS

#define WEBKIT_NETWORK_ERROR webkit_network_error_quark ()

typedef enum {
    WEBKIT_NETWORK_ERROR_FAILED = 399,
    WEBKIT_NETWORK_ERROR_TRANSPORT = 300,
    WEBKIT_NETWORK_ERROR_UNKNOWN_PROTOCOL = 301,
    WEBKIT_NETWORK_ERROR_CANCELLED = 302,
    WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST = 303
} WebKitNetworkError;

WK_EXPORT GQuark
webkit_network_error_quark (void);

G_END_DECLS

#endif
