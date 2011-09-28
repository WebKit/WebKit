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

#include "config.h"
#include "WebKitError.h"

#include "WebKitPrivate.h"
#include <WebCore/ErrorsGtk.h>

GQuark webkit_network_error_quark(void)
{
    return g_quark_from_static_string(WebCore::errorDomainNetwork);
}

COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_NETWORK_ERROR_FAILED, NetworkErrorFailed);
COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_NETWORK_ERROR_TRANSPORT, NetworkErrorTransport);
COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_NETWORK_ERROR_UNKNOWN_PROTOCOL, NetworkErrorUnknownProtocol);
COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_NETWORK_ERROR_CANCELLED, NetworkErrorCancelled);
COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST, NetworkErrorFileDoesNotExist);
