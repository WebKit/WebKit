/*
    Copyright (C) 2011 ProFUSION embedded systems

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
 * @file    ewk_protocol_handler.h
 * @brief   Describes the custom protocol handler API.
 */

#ifndef ewk_protocol_handler_h
#define ewk_protocol_handler_h

#include <Eina.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register a protocol handler.
 *
 * @param protocols the protocols that will be handled.
 * @return @c EINA_TRUE if success, @c EINA_FALSE if not.
 */
EAPI Eina_Bool ewk_custom_protocol_handler_set(const char** protocols);

/**
 * Remove protocol handler.
 *
 * @return @c EINA_TRUE if success, @c EINA_FALSE if not.
 */
EAPI Eina_Bool ewk_custom_protocol_handler_all_unset();

#ifdef __cplusplus
}
#endif

#endif // ewk_protocol_handler_h

