/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef WKAPICastEfl_h
#define WKAPICastEfl_h

#ifndef WKAPICast_h
#error "Please #include \"WKAPICast.h\" instead of this file directly."
#endif

#include <WebCore/TextDirection.h>
#include <WebKit2/WKPopupItem.h>

namespace WebKit {

class WebView;
class WebPopupItemEfl;
class WebPopupMenuListenerEfl;

WK_ADD_API_MAPPING(WKViewRef, WebView)
WK_ADD_API_MAPPING(WKPopupItemRef, WebPopupItemEfl)
WK_ADD_API_MAPPING(WKPopupMenuListenerRef, WebPopupMenuListenerEfl)

// Enum conversions.
inline WKPopupItemTextDirection toAPI(WebCore::TextDirection direction)
{
    WKPopupItemTextDirection wkDirection = kWKPopupItemTextDirectionLTR;

    switch (direction) {
    case WebCore::RTL:
        wkDirection = kWKPopupItemTextDirectionRTL;
        break;
    case WebCore::LTR:
        wkDirection = kWKPopupItemTextDirectionLTR;
        break;
    }

    return wkDirection;
}

}

#endif // WKAPICastEfl_h
