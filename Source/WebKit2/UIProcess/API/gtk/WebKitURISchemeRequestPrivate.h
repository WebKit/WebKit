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

#pragma once

#include "LegacyCustomProtocolManagerProxy.h"
#include "WebKitURISchemeRequest.h"
#include "WebKitWebContext.h"
#include <WebCore/ResourceRequest.h>

WebKitURISchemeRequest* webkitURISchemeRequestCreate(uint64_t requestID, WebKitWebContext*, const WebCore::ResourceRequest&, WebKit::LegacyCustomProtocolManagerProxy&);
void webkitURISchemeRequestCancel(WebKitURISchemeRequest*);
WebKit::LegacyCustomProtocolManagerProxy* webkitURISchemeRequestGetManager(WebKitURISchemeRequest*);
void webkitURISchemeRequestInvalidate(WebKitURISchemeRequest*);
