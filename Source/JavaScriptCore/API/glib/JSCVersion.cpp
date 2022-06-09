/*
 * Copyright (C) 2018 Igalia S.L.
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
#include "JSCVersion.h"

/**
 * SECTION: JSCVersion
 * @Short_description: Provides the JavaScriptCore version
 * @Title: JSCVersion
 *
 * Provides convenience functions returning JavaScriptCore's major, minor and
 * micro versions of the JavaScriptCore library your code is running
 * against. This is not necessarily the same as the
 * #JSC_MAJOR_VERSION, #JSC_MINOR_VERSION or
 * #JSC_MICRO_VERSION, which represent the version of the JavaScriptCore
 * headers included when compiling the code.
 *
 */

/**
 * jsc_get_major_version:
 *
 * Returns the major version number of the JavaScriptCore library.
 * (e.g. in JavaScriptCore version 1.8.3 this is 1.)
 *
 * This function is in the library, so it represents the JavaScriptCore library
 * your code is running against. Contrast with the #JSC_MAJOR_VERSION
 * macro, which represents the major version of the JavaScriptCore headers you
 * have included when compiling your code.
 *
 * Returns: the major version number of the JavaScriptCore library
 */
guint jsc_get_major_version(void)
{
    return JSC_MAJOR_VERSION;
}

/**
 * jsc_get_minor_version:
 *
 * Returns the minor version number of the JavaScriptCore library.
 * (e.g. in JavaScriptCore version 1.8.3 this is 8.)
 *
 * This function is in the library, so it represents the JavaScriptCore library
 * your code is running against. Contrast with the #JSC_MINOR_VERSION
 * macro, which represents the minor version of the JavaScriptCore headers you
 * have included when compiling your code.
 *
 * Returns: the minor version number of the JavaScriptCore library
 */
guint jsc_get_minor_version(void)
{
    return JSC_MINOR_VERSION;
}

/**
 * jsc_get_micro_version:
 *
 * Returns the micro version number of the JavaScriptCore library.
 * (e.g. in JavaScriptCore version 1.8.3 this is 3.)
 *
 * This function is in the library, so it represents the JavaScriptCore library
 * your code is running against. Contrast with the #JSC_MICRO_VERSION
 * macro, which represents the micro version of the JavaScriptCore headers you
 * have included when compiling your code.
 *
 * Returns: the micro version number of the JavaScriptCore library
 */
guint jsc_get_micro_version(void)
{
    return JSC_MICRO_VERSION;
}
