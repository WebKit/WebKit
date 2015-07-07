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

#include "config.h"
#include "WebKitUserContent.h"

#include "WebKitPrivate.h"
#include "WebKitUserContentPrivate.h"
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

/**
 * SECTION:WebKitUserContent
 * @short_description: Defines user content types which affect web pages.
 * @title: User content
 *
 * See also: #WebKitUserContentManager
 *
 * Since: 2.6
 */

static inline UserContentInjectedFrames toUserContentInjectedFrames(WebKitUserContentInjectedFrames injectedFrames)
{
    switch (injectedFrames) {
    case WEBKIT_USER_CONTENT_INJECT_TOP_FRAME:
        return InjectInTopFrameOnly;
    case WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES:
        return InjectInAllFrames;
    default:
        ASSERT_NOT_REACHED();
        return InjectInAllFrames;
    }
}

static inline UserStyleLevel toUserStyleLevel(WebKitUserStyleLevel styleLevel)
{
    switch (styleLevel) {
    case WEBKIT_USER_STYLE_LEVEL_USER:
        return UserStyleUserLevel;
    case WEBKIT_USER_STYLE_LEVEL_AUTHOR:
        return UserStyleAuthorLevel;
    default:
        ASSERT_NOT_REACHED();
        return UserStyleAuthorLevel;
    }
}

static inline Vector<String> toStringVector(const char* const* strv)
{
    if (!strv)
        return Vector<String>();

    Vector<String> result;
    for (auto str = strv; *str; ++str)
        result.append(std::move(String::fromUTF8(*str)));
    return result;
}

struct _WebKitUserStyleSheet {
    _WebKitUserStyleSheet(const gchar* source, WebKitUserContentInjectedFrames injectedFrames, WebKitUserStyleLevel level, const char* const* whitelist, const char* const* blacklist)
        : userStyleSheet(std::make_unique<UserStyleSheet>(
            String::fromUTF8(source), URL { },
            toStringVector(whitelist), toStringVector(blacklist),
            toUserContentInjectedFrames(injectedFrames),
            toUserStyleLevel(level)))
        , referenceCount(1)
    {
    }

    std::unique_ptr<UserStyleSheet> userStyleSheet;
    int referenceCount;
};

G_DEFINE_BOXED_TYPE(WebKitUserStyleSheet, webkit_user_style_sheet, webkit_user_style_sheet_ref, webkit_user_style_sheet_unref)

/**
 * webkit_user_style_sheet_ref:
 * @user_style_sheet: a #WebKitUserStyleSheet
 *
 * Atomically increments the reference count of @user_style_sheet by one.
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The passed #WebKitUserStyleSheet
 *
 * Since: 2.6
 */
WebKitUserStyleSheet* webkit_user_style_sheet_ref(WebKitUserStyleSheet* userStyleSheet)
{
    g_atomic_int_inc(&userStyleSheet->referenceCount);
    return userStyleSheet;
}

/**
 * webkit_user_style_sheet_unref:
 * @user_style_sheet: a #WebKitUserStyleSheet
 *
 * Atomically decrements the reference count of @user_style_sheet by one.
 * If the reference count drops to 0, all memory allocated by
 * #WebKitUserStyleSheet is released. This function is MT-safe and may be
 * called from any thread.
 *
 * Since: 2.6
 */
void webkit_user_style_sheet_unref(WebKitUserStyleSheet* userStyleSheet)
{
    if (g_atomic_int_dec_and_test(&userStyleSheet->referenceCount)) {
        userStyleSheet->~WebKitUserStyleSheet();
        g_slice_free(WebKitUserStyleSheet, userStyleSheet);
    }
}

/**
 * webkit_user_style_sheet_new:
 * @source: Source code of the user style sheet.
 * @injected_frames: A #WebKitUserContentInjectedFrames value
 * @level: A #WebKitUserStyleLevel
 * @whitelist: (array zero-terminated=1) (allow-none): A whitelist of URI patterns or %NULL
 * @blacklist: (array zero-terminated=1) (allow-none): A blacklist of URI patterns or %NULL
 *
 * Creates a new user style sheet. Style sheets can be applied to some URIs
 * only by passing non-null values for @whitelist or @blacklist. Passing a
 * %NULL whitelist implies that all URIs are on the whitelist. The style
 * sheet is applied if an URI matches the whitelist and not the blacklist.
 * URI patterns must be of the form `[protocol]://[host]/[path]`, where the
 * *host* and *path* components can contain the wildcard character (`*`) to
 * represent zero or more other characters.
 *
 * Returns: A new #WebKitUserStyleSheet
 *
 * Since: 2.6
 */
WebKitUserStyleSheet* webkit_user_style_sheet_new(const gchar* source, WebKitUserContentInjectedFrames injectedFrames, WebKitUserStyleLevel level, const char* const* whitelist, const char* const* blacklist)
{
    g_return_val_if_fail(source, nullptr);
    WebKitUserStyleSheet* userStyleSheet = g_slice_new(WebKitUserStyleSheet);
    new (userStyleSheet) WebKitUserStyleSheet(source, injectedFrames, level, whitelist, blacklist);
    return userStyleSheet;
}

const UserStyleSheet& webkitUserStyleSheetGetUserStyleSheet(WebKitUserStyleSheet* userStyleSheet)
{
    return *userStyleSheet->userStyleSheet;
}
