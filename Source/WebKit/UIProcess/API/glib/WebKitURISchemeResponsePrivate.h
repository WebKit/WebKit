/*
 * Copyright (C) 2021 Zixing Liu
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

#pragma once

#include "WebKitURISchemeResponse.h"
#include <wtf/text/CString.h>

int WebKitURISchemeResponseGetStatusCode(const WebKitURISchemeResponse*);
GInputStream* WebKitURISchemeResponseGetStream(const WebKitURISchemeResponse*);
const CString& WebKitURISchemeResponseGetStatusMessage(const WebKitURISchemeResponse*);
const CString& WebKitURISchemeResponseGetContentType(const WebKitURISchemeResponse*);
uint64_t WebKitURISchemeResponseGetStreamLength(const WebKitURISchemeResponse*);
