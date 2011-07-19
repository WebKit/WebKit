/*
    Copyright (C) 2011 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

/**
 * @file    ewk_network.h
 * @brief   Describes the network API.
 */

#ifndef ewk_network_h
#define ewk_network_h

#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

EAPI void             ewk_network_proxy_uri_set(const char* proxy);
EAPI const char*      ewk_network_proxy_uri_get(void);

EAPI void             ewk_network_state_notifier_online_set(Eina_Bool online);

#ifdef __cplusplus
}
#endif
#endif // ewk_network_h
