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

/**
 * @file    ewk_context.h
 * @brief   Describes the context API.
 *
 * @note ewk_context encapsulates all pages related to specific use of WebKit.
 * All pages in this context share the same visited link set,
 * local storage set, and preferences.
 */

#ifndef ewk_context_h
#define ewk_context_h

#include <Evas.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Creates a type name for @a _Ewk_Context. */
typedef struct _Ewk_Context Ewk_Context;

/**
 * Gets default Ewk_Context instance.
 *
 * @return Ewk_Context object.
 */
EAPI Ewk_Context *ewk_context_default_get();

#ifdef __cplusplus
}
#endif

#endif // ewk_context_h
