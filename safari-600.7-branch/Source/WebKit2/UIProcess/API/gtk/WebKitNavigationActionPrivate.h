/*
 * Copyright (C) 2014 Igalia S.L.
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

#ifndef WebKitNavigationActionPrivate_h
#define WebKitNavigationActionPrivate_h

#include "NavigationActionData.h"
#include "WebKitNavigationAction.h"
#include "WebKitPrivate.h"

struct _WebKitNavigationAction {
    _WebKitNavigationAction(WebKitURIRequest* uriRequest, const WebKit::NavigationActionData& navigationActionData)
        : type(toWebKitNavigationType(navigationActionData.navigationType))
        , mouseButton(toWebKitMouseButton(navigationActionData.mouseButton))
        , modifiers(toGdkModifiers(navigationActionData.modifiers))
        , isUserGesture(navigationActionData.isProcessingUserGesture)
        , request(uriRequest)
    {
    }

    _WebKitNavigationAction(WebKitNavigationAction* navigation)
        : type(navigation->type)
        , mouseButton(navigation->mouseButton)
        , modifiers(navigation->modifiers)
        , isUserGesture(navigation->isUserGesture)
        , request(navigation->request)
    {
    }

    WebKitNavigationType type;
    unsigned mouseButton;
    unsigned modifiers;
    bool isUserGesture : 1;
    GRefPtr<WebKitURIRequest> request;
};

WebKitNavigationAction* webkitNavigationActionCreate(WebKitURIRequest*, const WebKit::NavigationActionData&);

#endif // WebKitNavigationActionPrivate_h
