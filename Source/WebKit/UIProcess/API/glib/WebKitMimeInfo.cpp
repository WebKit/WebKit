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

#include "config.h"
#include "WebKitMimeInfo.h"

struct _WebKitMimeInfo {
};

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
G_DEFINE_BOXED_TYPE(WebKitMimeInfo, webkit_mime_info, webkit_mime_info_ref, webkit_mime_info_unref)
ALLOW_DEPRECATED_DECLARATIONS_END

/**
 * webkit_mime_info_ref:
 * @info: a #WebKitMimeInfo
 *
 * Atomically increments the reference count of @info by one. This
 * function is MT-safe and may be called from any thread.
 *
 * Returns: The passed in #WebKitMimeInfo
 *
 * Deprecated: 2.32
 */
WebKitMimeInfo* webkit_mime_info_ref(WebKitMimeInfo*)
{
    return nullptr;
}

/**
 * webkit_mime_info_unref:
 * @info: a #WebKitMimeInfo
 *
 * Atomically decrements the reference count of @info by one. If the
 * reference count drops to 0, all memory allocated by the #WebKitMimeInfo is
 * released. This function is MT-safe and may be called from any
 * thread.
 *
 * Deprecated: 2.32
 */
void webkit_mime_info_unref(WebKitMimeInfo*)
{
}

/**
 * webkit_mime_info_get_mime_type:
 * @info: a #WebKitMimeInfo
 *
 * Returns: the MIME type of @info
 *
 * Deprecated: 2.32
 */
const char* webkit_mime_info_get_mime_type(WebKitMimeInfo*)
{
    return nullptr;
}

/**
 * webkit_mime_info_get_description:
 * @info: a #WebKitMimeInfo
 *
 * Returns: the description of the MIME type of @info
 *
 * Deprecated: 2.32
 */
const char* webkit_mime_info_get_description(WebKitMimeInfo*)
{
    return nullptr;
}

/**
 * webkit_mime_info_get_extensions:
 * @info: a #WebKitMimeInfo
 *
 * Get the list of file extensions associated to the
 * MIME type of @info
 *
 * Returns: (array zero-terminated=1) (transfer none): a
 *     %NULL-terminated array of strings
 *
 * Deprecated: 2.32
 */
const char* const* webkit_mime_info_get_extensions(WebKitMimeInfo*)
{
    return nullptr;
}
