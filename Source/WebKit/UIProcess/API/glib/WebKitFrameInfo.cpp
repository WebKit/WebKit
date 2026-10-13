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
 * WebKitFrameInfo: (ref-func webkit_frame_info_ref) (unref-func webkit_frame_info_unref)
 *
 * Information about a frame on a webpage.
 *
 * Since: 2.56
 */
struct _WebKitFrameInfo {
    explicit _WebKitFrameInfo(API::FrameInfo* info)
        : info(info)
    {
    }

    explicit _WebKitFrameInfo(WebKitFrameInfo* frameInfo)
        : info(frameInfo->info)
    {
    }

    RefPtr<API::FrameInfo> info;
    int referenceCount { 1 };
};

G_DEFINE_BOXED_TYPE(WebKitFrameInfo, webkit_frame_info, webkit_frame_info_ref, webkit_frame_info_unref)

WebKitFrameInfo* webkitFrameInfoCreate(API::FrameInfo* frameInfo)
{
    WebKitFrameInfo* info = static_cast<WebKitFrameInfo*>(fastZeroedMalloc(sizeof(WebKitFrameInfo)));
    new (info) WebKitFrameInfo(frameInfo);
    return info;
}

/**
 * webkit_frame_info_ref:
 * @info: a #WebKitFrameInfo
 *
 * Atomically acquires a reference on the given @info.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The same @info with an additional reference.
 *
 * Since: 2.56
 */
WebKitFrameInfo* webkit_frame_info_ref(WebKitFrameInfo* info)
{
    g_return_val_if_fail(info, nullptr);
    g_atomic_int_inc(&info->referenceCount);
    return info;
}

/**
 * webkit_frame_info_unref:
 * @info: a #WebKitFrameInfo
 *
 * Atomically releases a reference on the given @info.
 *
 * If the reference was the last, the resources associated with
 * the @info are freed.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Since: 2.56
 */
void webkit_frame_info_unref(WebKitFrameInfo* info)
{
    g_return_if_fail(info);
    if (g_atomic_int_dec_and_test(&info->referenceCount)) {
        info->~WebKitFrameInfo();
        fastFree(info);
    }
}

/**
 * webkit_frame_info_is_main_frame:
 * @info: a #WebKitFrameInfo
 *
 * Whether or not this frame is the web site's main frame or a subframe.
 *
 * Since: 2.56
 */
gboolean
webkit_frame_info_is_main_frame(WebKitFrameInfo* info)
{
    g_return_val_if_fail(info, FALSE);

    return info->info->isMainFrame();
}

namespace WTF {
WTF_DEFINE_GREF_TRAITS(WebKitFrameInfo, webkit_frame_info_ref, webkit_frame_info_unref)
}
