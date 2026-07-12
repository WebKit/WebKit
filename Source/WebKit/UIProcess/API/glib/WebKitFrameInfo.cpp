/*
 * Copyright (C) 2026 Tau Gärtli
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
#include "WebKitFrameInfo.h"

#include "APIFrameInfo.h"
#include "WebKitFrameInfoPrivate.h"

/**
 * WebKitFrameInfo:
 *
 * Information about a frame on a webpage.
 *
 * Since: 2.42
 */
struct _WebKitFrameInfo {
    _WebKitFrameInfo(API::FrameInfo* info)
        : info(info)
    {
    }

    _WebKitFrameInfo(WebKitFrameInfo* frameInfo)
        : info(frameInfo->info)
    {
    }

    RefPtr<API::FrameInfo> info;
};

G_DEFINE_BOXED_TYPE(WebKitFrameInfo, webkit_frame_info, webkit_frame_info_copy, webkit_frame_info_free)

WebKitFrameInfo* webkitFrameInfoCreate(API::FrameInfo* frameInfo)
{
    WebKitFrameInfo* info = static_cast<WebKitFrameInfo*>(fastZeroedMalloc(sizeof(WebKitFrameInfo)));
    new (info) WebKitFrameInfo(frameInfo);
    return info;
}

/**
 * webkit_frame_info_copy:
 * @info: a #WebKitFrameInfo
 *
 * Make a copy of @info.
 *
 * Returns: (transfer full): A copy of passed in #WebKitFrameInfo
 *
 * Since: 2.54
 */
WebKitFrameInfo* webkit_frame_info_copy(WebKitFrameInfo* info)
{
    g_return_val_if_fail(info, nullptr);

    WebKitFrameInfo* copy = static_cast<WebKitFrameInfo*>(fastZeroedMalloc(sizeof(WebKitFrameInfo)));
    new (copy) WebKitFrameInfo(info);
    return copy;
}

/**
 * webkit_frame_info_free:
 * @info: a #WebKitFrameInfo
 *
 * Free the #WebKitFrameInfo
 *
 * Since: 2.54
 */
void webkit_frame_info_free(WebKitFrameInfo* info)
{
    g_return_if_fail(info);

    info->~WebKitFrameInfo();
    fastFree(info);
}

/**
 * webkit_frame_info_is_main_frame:
 * @info: a #WebKitFrameInfo
 *
 * Whether or not this frame is the web site's main frame or a subframe.
 *
 * Since: 2.54
 */
gboolean
webkit_frame_info_is_main_frame(WebKitFrameInfo* info)
{
    g_return_val_if_fail(info, FALSE);

    return info->info->isMainFrame();
}
