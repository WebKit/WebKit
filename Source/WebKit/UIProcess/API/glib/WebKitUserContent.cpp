/*
 * Copyright (C) 2014, 2018-2019 Igalia S.L.
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

#include "WebKitUserContentPrivate.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

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


API::UserContentWorld& webkitUserContentWorld(const char* worldName)
{
    static NeverDestroyed<HashMap<CString, RefPtr<API::UserContentWorld>>> map;
    return *map.get().ensure(worldName, [worldName = String::fromUTF8(worldName)] { return API::UserContentWorld::worldWithName(worldName); }).iterator->value;
}

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

static inline UserScriptInjectionTime toUserScriptInjectionTime(WebKitUserScriptInjectionTime injectionTime)
{
    switch (injectionTime) {
    case WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START:
        return InjectAtDocumentStart;
    case WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END:
        return InjectAtDocumentEnd;
    default:
        ASSERT_NOT_REACHED();
        return InjectAtDocumentStart;
    }
}

static inline Vector<String> toStringVector(const char* const* strv)
{
    if (!strv)
        return Vector<String>();

    Vector<String> result;
    for (auto str = strv; *str; ++str)
        result.append(String::fromUTF8(*str));
    return result;
}

struct _WebKitUserStyleSheet {
    _WebKitUserStyleSheet(const gchar* source, WebKitUserContentInjectedFrames injectedFrames, WebKitUserStyleLevel level, const char* const* whitelist, const char* const* blacklist, API::UserContentWorld& world)
        : userStyleSheet(adoptRef(new API::UserStyleSheet(UserStyleSheet {
            String::fromUTF8(source), URL { },
            toStringVector(whitelist), toStringVector(blacklist),
            toUserContentInjectedFrames(injectedFrames),
            toUserStyleLevel(level) }, world)))
        , referenceCount(1)
    {
    }

    RefPtr<API::UserStyleSheet> userStyleSheet;
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
        fastFree(userStyleSheet);
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
    WebKitUserStyleSheet* userStyleSheet = static_cast<WebKitUserStyleSheet*>(fastMalloc(sizeof(WebKitUserStyleSheet)));
    new (userStyleSheet) WebKitUserStyleSheet(source, injectedFrames, level, whitelist, blacklist, API::UserContentWorld::normalWorld());
    return userStyleSheet;
}

/**
 * webkit_user_style_sheet_new_for_world:
 * @source: Source code of the user style sheet.
 * @injected_frames: A #WebKitUserContentInjectedFrames value
 * @level: A #WebKitUserStyleLevel
 * @world_name: the name of a #WebKitScriptWorld
 * @whitelist: (array zero-terminated=1) (allow-none): A whitelist of URI patterns or %NULL
 * @blacklist: (array zero-terminated=1) (allow-none): A blacklist of URI patterns or %NULL
 *
 * Creates a new user style sheet for script world with name @world_name.
 * See webkit_user_style_sheet_new() for a full description.
 *
 * Returns: A new #WebKitUserStyleSheet
 *
 * Since: 2.22
 */
WebKitUserStyleSheet* webkit_user_style_sheet_new_for_world(const gchar* source, WebKitUserContentInjectedFrames injectedFrames, WebKitUserStyleLevel level, const char* worldName, const char* const* whitelist, const char* const* blacklist)
{
    g_return_val_if_fail(source, nullptr);
    g_return_val_if_fail(worldName, nullptr);

    WebKitUserStyleSheet* userStyleSheet = static_cast<WebKitUserStyleSheet*>(fastMalloc(sizeof(WebKitUserStyleSheet)));
    new (userStyleSheet) WebKitUserStyleSheet(source, injectedFrames, level, whitelist, blacklist, webkitUserContentWorld(worldName));
    return userStyleSheet;
}

API::UserStyleSheet& webkitUserStyleSheetGetUserStyleSheet(WebKitUserStyleSheet* userStyleSheet)
{
    return *userStyleSheet->userStyleSheet;
}

struct _WebKitUserScript {
    _WebKitUserScript(const gchar* source, WebKitUserContentInjectedFrames injectedFrames, WebKitUserScriptInjectionTime injectionTime, const gchar* const* whitelist, const gchar* const* blacklist, API::UserContentWorld& world)
        : userScript(adoptRef(new API::UserScript(UserScript {
            String::fromUTF8(source), URL { },
            toStringVector(whitelist), toStringVector(blacklist),
            toUserScriptInjectionTime(injectionTime),
            toUserContentInjectedFrames(injectedFrames) }, world)))
        , referenceCount(1)
    {
    }

    RefPtr<API::UserScript> userScript;
    int referenceCount;
};

G_DEFINE_BOXED_TYPE(WebKitUserScript, webkit_user_script, webkit_user_script_ref, webkit_user_script_unref)

/**
 * webkit_user_script_ref:
 * @user_script: a #WebKitUserScript
 *
 * Atomically increments the reference count of @user_script by one.
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The passed #WebKitUserScript
 *
 * Since: 2.6
 */
WebKitUserScript* webkit_user_script_ref(WebKitUserScript* userScript)
{
    g_atomic_int_inc(&userScript->referenceCount);
    return userScript;
}

/**
 * webkit_user_script_unref:
 * @user_script: a #WebKitUserScript
 *
 * Atomically decrements the reference count of @user_script by one.
 * If the reference count drops to 0, all memory allocated by
 * #WebKitUserScript is released. This function is MT-safe and may be called
 * from any thread.
 *
 * Since: 2.6
 */
void webkit_user_script_unref(WebKitUserScript* userScript)
{
    if (g_atomic_int_dec_and_test(&userScript->referenceCount)) {
        userScript->~WebKitUserScript();
        fastFree(userScript);
    }
}

/**
 * webkit_user_script_new:
 * @source: Source code of the user script.
 * @injected_frames: A #WebKitUserContentInjectedFrames value
 * @injection_time: A #WebKitUserScriptInjectionTime value
 * @whitelist: (array zero-terminated=1) (allow-none): A whitelist of URI patterns or %NULL
 * @blacklist: (array zero-terminated=1) (allow-none): A blacklist of URI patterns or %NULL
 *
 * Creates a new user script. Scripts can be applied to some URIs
 * only by passing non-null values for @whitelist or @blacklist. Passing a
 * %NULL whitelist implies that all URIs are on the whitelist. The script
 * is applied if an URI matches the whitelist and not the blacklist.
 * URI patterns must be of the form `[protocol]://[host]/[path]`, where the
 * *host* and *path* components can contain the wildcard character (`*`) to
 * represent zero or more other characters.
 *
 * Returns: A new #WebKitUserScript
 *
 * Since: 2.6
 */
WebKitUserScript* webkit_user_script_new(const gchar* source, WebKitUserContentInjectedFrames injectedFrames, WebKitUserScriptInjectionTime injectionTime, const gchar* const* whitelist, const gchar* const* blacklist)
{
    g_return_val_if_fail(source, nullptr);
    WebKitUserScript* userScript = static_cast<WebKitUserScript*>(fastMalloc(sizeof(WebKitUserScript)));
    new (userScript) WebKitUserScript(source, injectedFrames, injectionTime, whitelist, blacklist, API::UserContentWorld::normalWorld());
    return userScript;
}

/**
 * webkit_user_script_new_for_world:
 * @source: Source code of the user script.
 * @injected_frames: A #WebKitUserContentInjectedFrames value
 * @injection_time: A #WebKitUserScriptInjectionTime value
 * @world_name: the name of a #WebKitScriptWorld
 * @whitelist: (array zero-terminated=1) (allow-none): A whitelist of URI patterns or %NULL
 * @blacklist: (array zero-terminated=1) (allow-none): A blacklist of URI patterns or %NULL
 *
 * Creates a new user script for script world with name @world_name.
 * See webkit_user_script_new() for a full description.
 *
 * Returns: A new #WebKitUserScript
 *
 * Since: 2.22
 */
WebKitUserScript* webkit_user_script_new_for_world(const gchar* source, WebKitUserContentInjectedFrames injectedFrames, WebKitUserScriptInjectionTime injectionTime, const char* worldName, const gchar* const* whitelist, const gchar* const* blacklist)
{
    g_return_val_if_fail(source, nullptr);
    g_return_val_if_fail(worldName, nullptr);

    WebKitUserScript* userScript = static_cast<WebKitUserScript*>(fastMalloc(sizeof(WebKitUserScript)));
    new (userScript) WebKitUserScript(source, injectedFrames, injectionTime, whitelist, blacklist, webkitUserContentWorld(worldName));
    return userScript;
}

API::UserScript& webkitUserScriptGetUserScript(WebKitUserScript* userScript)
{
    return *userScript->userScript;
}


struct _WebKitUserContentFilter {
    _WebKitUserContentFilter(RefPtr<API::ContentRuleList>&& contentRuleList)
        : identifier(contentRuleList->name().utf8())
        , contentRuleList(WTFMove(contentRuleList))
        , referenceCount(1)
    {
    }

    CString identifier;
    RefPtr<API::ContentRuleList> contentRuleList;
    int referenceCount;
};

G_DEFINE_BOXED_TYPE(WebKitUserContentFilter, webkit_user_content_filter, webkit_user_content_filter_ref, webkit_user_content_filter_unref)

/**
 * webkit_user_content_filter_ref:
 * @user_content_filter: A #WebKitUserContentFilter
 *
 * Atomically increments the reference count of @user_content_filter by one.
 * This function is MT-safe and may be called from any thread.
 *
 * Since: 2.24
 */
WebKitUserContentFilter* webkit_user_content_filter_ref(WebKitUserContentFilter* userContentFilter)
{
    g_return_val_if_fail(userContentFilter, nullptr);
    g_atomic_int_inc(&userContentFilter->referenceCount);
    return userContentFilter;
}

/**
 * webkit_user_content_filter_unref:
 * @user_content_filter: A #WebKitUserContentFilter
 *
 * Atomically decrements the reference count of @user_content_filter by one.
 * If the reference count drops to 0, all the memory allocated by the
 * #WebKitUserContentFilter is released. This function is MT-safe and may
 * be called from any thread.
 *
 * Since: 2.24
 */
void webkit_user_content_filter_unref(WebKitUserContentFilter* userContentFilter)
{
    g_return_if_fail(userContentFilter);
    if (g_atomic_int_dec_and_test(&userContentFilter->referenceCount)) {
        userContentFilter->~WebKitUserContentFilter();
        fastFree(userContentFilter);
    }
}

/**
 * webkit_user_content_filter_get_identifier:
 * @user_content_filter: A #WebKitUserContentFilter
 *
 * Obtain the identifier previously used to save the @user_content_filter in the
 * #WebKitUserContentFilterStore.
 *
 * Returns: (transfer none): the identifier for the filter
 *
 * Since: 2.24
 */
const char* webkit_user_content_filter_get_identifier(WebKitUserContentFilter* userContentFilter)
{
    g_return_val_if_fail(userContentFilter, nullptr);
    return userContentFilter->identifier.data();
}

WebKitUserContentFilter* webkitUserContentFilterCreate(RefPtr<API::ContentRuleList>&& contentRuleList)
{
    WebKitUserContentFilter* userContentFilter = static_cast<WebKitUserContentFilter*>(fastMalloc(sizeof(WebKitUserContentFilter)));
    new (userContentFilter) WebKitUserContentFilter(WTFMove(contentRuleList));
    return userContentFilter;
}

API::ContentRuleList& webkitUserContentFilterGetContentRuleList(WebKitUserContentFilter* userContentFilter)
{
    ASSERT(userContentFilter);
    return *userContentFilter->contentRuleList;
}
