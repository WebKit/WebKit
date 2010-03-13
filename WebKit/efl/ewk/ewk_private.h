/*
    Copyright (C) 2009-2010 ProFUSION embedded systems
    Copyright (C) 2009-2010 Samsung Electronics

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

#ifndef ewk_private_h
#define ewk_private_h

#include "BackForwardList.h"
#include "EWebKit.h"
#include "Frame.h"
#include "Page.h"
#include "Settings.h"
#include "Widget.h"
#include "ewk_logging.h"
#include "ewk_util.h"

#include <cairo.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

#ifdef __cplusplus
extern "C" {
#endif

// If defined, ewk will do type checking to ensure objects are of correct type
#define EWK_TYPE_CHECK 1

void             ewk_view_ready(Evas_Object *o);
void             ewk_view_title_set(Evas_Object *o, const char *title);
void             ewk_view_uri_changed(Evas_Object *o);
void             ewk_view_load_started(Evas_Object *o);
void             ewk_view_frame_main_load_started(Evas_Object *o);
void             ewk_view_load_finished(Evas_Object *o, const Ewk_Frame_Load_Error *error);
void             ewk_view_load_error(Evas_Object *o, const Ewk_Frame_Load_Error *error);
void             ewk_view_load_progress_changed(Evas_Object *o);

void             ewk_view_mouse_link_hover_in(Evas_Object *o, void *data);
void             ewk_view_mouse_link_hover_out(Evas_Object *o);

void             ewk_view_toolbars_visible_set(Evas_Object *o, Eina_Bool visible);
void             ewk_view_toolbars_visible_get(Evas_Object *o, Eina_Bool *visible);

void             ewk_view_statusbar_visible_set(Evas_Object *o, Eina_Bool visible);
void             ewk_view_statusbar_visible_get(Evas_Object *o, Eina_Bool *visible);
void             ewk_view_statusbar_text_set(Evas_Object *o, const char *text);

void             ewk_view_scrollbars_visible_set(Evas_Object *o, Eina_Bool visible);
void             ewk_view_scrollbars_visible_get(Evas_Object *o, Eina_Bool *visible);

void             ewk_view_menubar_visible_set(Evas_Object *o, Eina_Bool visible);
void             ewk_view_menubar_visible_get(Evas_Object *o, Eina_Bool *visible);

void             ewk_view_tooltip_text_set(Evas_Object *o, const char *text);

void             ewk_view_add_console_message(Evas_Object *o, const char *message, unsigned int lineNumber, const char *sourceID);

void             ewk_view_run_javascript_alert(Evas_Object *o, Evas_Object *frame, const char *message);
Eina_Bool        ewk_view_run_javascript_confirm(Evas_Object *o, Evas_Object *frame, const char *message);
Eina_Bool        ewk_view_run_javascript_prompt(Evas_Object *o, Evas_Object *frame, const char *message, const char *defaultValue, char **value);
Eina_Bool        ewk_view_should_interrupt_javascript(Evas_Object *o);
void             ewk_view_exceeded_database_quota(Evas_Object *o, Evas_Object *frame, const char *databaseName);

void             ewk_view_repaint(Evas_Object *o, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
void             ewk_view_scroll(Evas_Object *o, Evas_Coord dx, Evas_Coord dy, Evas_Coord sx, Evas_Coord sy, Evas_Coord sw, Evas_Coord sh, Evas_Coord cx, Evas_Coord cy, Evas_Coord cw, Evas_Coord ch, Eina_Bool main_frame);
WebCore::Page   *ewk_view_core_page_get(const Evas_Object *o);

WTF::PassRefPtr<WebCore::Frame> ewk_view_frame_create(Evas_Object *o, Evas_Object *frame, const WebCore::String& name, WebCore::HTMLFrameOwnerElement* ownerElement, const WebCore::KURL& url, const WebCore::String& referrer);

WTF::PassRefPtr<WebCore::Widget> ewk_view_plugin_create(Evas_Object* o, Evas_Object* frame, const WebCore::IntSize& pluginSize, WebCore::HTMLPlugInElement* element, const WebCore::KURL& url, const WTF::Vector<WebCore::String>& paramNames, const WTF::Vector<WebCore::String>& paramValues, const WebCore::String& mimeType, bool loadManually);

Ewk_History      *ewk_history_new(WebCore::BackForwardList *history);
void              ewk_history_free(Ewk_History *history);

Evas_Object      *ewk_frame_add(Evas *e);
Eina_Bool         ewk_frame_init(Evas_Object *o, Evas_Object *view, WebCore::Frame *frame);
Evas_Object      *ewk_frame_child_add(Evas_Object *o, WTF::PassRefPtr<WebCore::Frame> child, const WebCore::String& name, const WebCore::KURL& url, const WebCore::String& referrer);

WebCore::Frame   *ewk_frame_core_get(const Evas_Object *o);
void              ewk_frame_core_gone(Evas_Object *o);

void              ewk_frame_load_started(Evas_Object *o);
void              ewk_frame_load_finished(Evas_Object *o, const char *error_domain, int error_code, Eina_Bool is_cancellation, const char *error_description, const char *failing_url);
void              ewk_frame_load_error(Evas_Object *o, const char *error_domain, int error_code, Eina_Bool is_cancellation, const char *error_description, const char *failing_url);
void              ewk_frame_load_progress_changed(Evas_Object *o);

void              ewk_frame_contents_size_changed(Evas_Object *o, Evas_Coord w, Evas_Coord h);
void              ewk_frame_title_set(Evas_Object *o, const char *title);

void              ewk_frame_view_create_for_view(Evas_Object *o, Evas_Object *view);
Eina_Bool         ewk_frame_uri_changed(Evas_Object *o);
void              ewk_frame_force_layout(Evas_Object *o);

WTF::PassRefPtr<WebCore::Widget> ewk_frame_plugin_create(Evas_Object* o, const WebCore::IntSize& pluginSize, WebCore::HTMLPlugInElement* element, const WebCore::KURL& url, const WTF::Vector<WebCore::String>& paramNames, const WTF::Vector<WebCore::String>& paramValues, const WebCore::String& mimeType, bool loadManually);

#ifdef __cplusplus

}
#endif
#endif // ewk_private_h
