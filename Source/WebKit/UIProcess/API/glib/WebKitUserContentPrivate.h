/*
 * Copyright (C) 2014, 2018 Igalia S.L.
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

#ifndef WebKitUserContentPrivate_h
#define WebKitUserContentPrivate_h

#include "APIContentRuleList.h"
#include "APIUserContentWorld.h"
#include "APIUserScript.h"
#include "APIUserStyleSheet.h"
#include "WebKitUserContent.h"
#include <WebCore/UserScript.h>
#include <WebCore/UserStyleSheet.h>

API::UserScript& webkitUserScriptGetUserScript(WebKitUserScript*);
API::UserStyleSheet& webkitUserStyleSheetGetUserStyleSheet(WebKitUserStyleSheet*);
API::UserContentWorld& webkitUserContentWorld(const char*);
API::ContentRuleList& webkitUserContentFilterGetContentRuleList(WebKitUserContentFilter*);
WebKitUserContentFilter* webkitUserContentFilterCreate(RefPtr<API::ContentRuleList>&&);

#endif // WebKitUserContentPrivate_h
