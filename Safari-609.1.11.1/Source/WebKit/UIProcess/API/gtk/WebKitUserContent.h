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

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitUserContent_h
#define WebKitUserContent_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

/**
 * WebKitUserContentInjectedFrames:
 * @WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES: Insert the user style
 *   sheet in all the frames loaded by the web view, including
 *   nested frames. This is the default.
 * @WEBKIT_USER_CONTENT_INJECT_TOP_FRAME: Insert the user style
 *   sheet *only* in the top-level frame loaded by the web view,
 *   and *not* in the nested frames.
 *
 * Specifies in which frames user style sheets are to be inserted in.
 *
 * Since: 2.6
 */
typedef enum {
    WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
    WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
} WebKitUserContentInjectedFrames;

/**
 * WebKitUserStyleLevel:
 * @WEBKIT_USER_STYLE_LEVEL_USER: The style sheet is an user style sheet,
 *   its contents always override other style sheets. This is the default.
 * @WEBKIT_USER_STYLE_LEVEL_AUTHOR: The style sheet will be treated as if
 *   it was provided by the loaded documents. That means other user style
 *   sheets may still override it.
 *
 * Specifies how to treat an user style sheet.
 *
 * Since: 2.6
 */
typedef enum {
    WEBKIT_USER_STYLE_LEVEL_USER,
    WEBKIT_USER_STYLE_LEVEL_AUTHOR,
} WebKitUserStyleLevel;

#define WEBKIT_TYPE_USER_STYLE_SHEET (webkit_user_style_sheet_get_type())

typedef struct _WebKitUserStyleSheet WebKitUserStyleSheet;

WEBKIT_API GType
webkit_user_style_sheet_get_type      (void);

WEBKIT_API WebKitUserStyleSheet *
webkit_user_style_sheet_ref           (WebKitUserStyleSheet           *user_style_sheet);

WEBKIT_API void
webkit_user_style_sheet_unref         (WebKitUserStyleSheet           *user_style_sheet);

WEBKIT_API WebKitUserStyleSheet *
webkit_user_style_sheet_new           (const gchar                    *source,
                                       WebKitUserContentInjectedFrames injected_frames,
                                       WebKitUserStyleLevel            level,
                                       const gchar* const             *whitelist,
                                       const gchar* const             *blacklist);

WEBKIT_API WebKitUserStyleSheet *
webkit_user_style_sheet_new_for_world (const gchar                    *source,
                                       WebKitUserContentInjectedFrames injected_frames,
                                       WebKitUserStyleLevel            level,
                                       const gchar                    *world_name,
                                       const gchar* const             *whitelist,
                                       const gchar* const             *blacklist);

/**
 * WebKitUserScriptInjectionTime:
 * @WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START: Insert the code of the user
 *   script at the beginning of loaded documents. This is the default.
 * @WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END: Insert the code of the user
 *   script at the end of the loaded documents.
 *
 * Specifies at which place of documents an user script will be inserted.
 *
 * Since: 2.6
 */
typedef enum {
    WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
    WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
} WebKitUserScriptInjectionTime;

#define WEBKIT_TYPE_USER_SCRIPT (webkit_user_script_get_type())

typedef struct _WebKitUserScript WebKitUserScript;

WEBKIT_API GType
webkit_user_script_get_type      (void);

WEBKIT_API WebKitUserScript *
webkit_user_script_ref           (WebKitUserScript               *user_script);

WEBKIT_API void
webkit_user_script_unref         (WebKitUserScript               *user_script);

WEBKIT_API WebKitUserScript *
webkit_user_script_new           (const gchar                    *source,
                                  WebKitUserContentInjectedFrames injected_frames,
                                  WebKitUserScriptInjectionTime   injection_time,
                                  const gchar* const             *whitelist,
                                  const gchar* const             *blacklist);

WEBKIT_API WebKitUserScript *
webkit_user_script_new_for_world (const gchar                    *source,
                                  WebKitUserContentInjectedFrames injected_frames,
                                  WebKitUserScriptInjectionTime   injection_time,
                                  const gchar                    *world_name,
                                  const gchar* const             *whitelist,
                                  const gchar* const             *blacklist);

#define WEBKIT_TYPE_USER_CONTENT_FILTER   (webkit_user_content_filter_get_type())

typedef struct _WebKitUserContentFilter WebKitUserContentFilter;

WEBKIT_API GType
webkit_user_content_filter_get_type       (void);

WEBKIT_API const char*
webkit_user_content_filter_get_identifier (WebKitUserContentFilter     *user_content_filter);

WEBKIT_API WebKitUserContentFilter *
webkit_user_content_filter_ref            (WebKitUserContentFilter     *user_content_filter);

WEBKIT_API void
webkit_user_content_filter_unref          (WebKitUserContentFilter     *user_content_filter);

G_END_DECLS

#endif
