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

/**
 * @file    ewk_private.h
 * @brief   Private methods of WebKit-EFL.
 */

#ifndef ewk_private_h
#define ewk_private_h

#include "APICast.h"
#include "BackForwardListImpl.h"
#include "Frame.h"
#include "NP_jsobject.h"
#include "Page.h"
#include "Widget.h"
#include "ewk_history.h"
#include "ewk_js.h"
#include "ewk_view.h"
#include <Evas.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

#ifdef __cplusplus
extern "C" {
#endif

// If defined, ewk will do type checking to ensure objects are of correct type
#define EWK_TYPE_CHECK 1

#if ENABLE(NETSCAPE_PLUGIN_API)
#define EWK_JS_OBJECT_MAGIC 0x696969

typedef struct _Ewk_JS_Class Ewk_JS_Class;
struct _Ewk_JS_Object {
    JavaScriptObject base;
    const char* name;
    const Ewk_JS_Class* cls;
    Eina_Hash* properties;
    Evas_Object* view; // ewk_view: check if this object has already been added to another ewk_view
    Ewk_JS_Object_Type type;
    EINA_MAGIC;
};
#endif // ENABLE(NETSCAPE_PLUGIN_API)

// forward declarations
namespace WebCore {
struct PopupMenuClient;
struct ContextMenu;
struct ContextMenuItem;
}

struct Ewk_Window_Object_Cleared_Event {
    JSContextRef context;
    JSObjectRef windowObject;
    Evas_Object* frame;
};

namespace EWKPrivate {

WebCore::Frame *coreFrame(const Evas_Object *ewkFrame);
WebCore::Page *corePage(const Evas_Object *ewkView);
WebCore::HistoryItem *coreHistoryItem(const Ewk_History_Item *ewkHistoryItem);

Evas_Object* kitFrame(const WebCore::Frame* coreFrame);

} // namespace EWKPrivate

void ewk_view_ready(Evas_Object* o);
void ewk_view_input_method_state_set(Evas_Object* o, bool active);
void ewk_view_title_set(Evas_Object* o, const char* title);
void ewk_view_uri_changed(Evas_Object* o);
void ewk_view_load_document_finished(Evas_Object* o, Evas_Object* frame);
void ewk_view_load_started(Evas_Object* o);
void ewk_view_load_provisional(Evas_Object* o);
void ewk_view_frame_main_load_started(Evas_Object* o);
void ewk_view_frame_main_cleared(Evas_Object* o);
void ewk_view_frame_main_icon_received(Evas_Object* o);
void ewk_view_load_finished(Evas_Object* o, const Ewk_Frame_Load_Error* error);
void ewk_view_load_error(Evas_Object* o, const Ewk_Frame_Load_Error* error);
void ewk_view_load_progress_changed(Evas_Object* o);
void ewk_view_load_show(Evas_Object* o);
void ewk_view_restore_state(Evas_Object* o, Evas_Object* frame);
Evas_Object* ewk_view_window_create(Evas_Object* o, bool javascript, const WebCore::WindowFeatures* coreFeatures);
void ewk_view_window_close(Evas_Object* o);

void ewk_view_mouse_link_hover_in(Evas_Object* o, void* data);
void ewk_view_mouse_link_hover_out(Evas_Object* o);

void ewk_view_toolbars_visible_set(Evas_Object* o, bool visible);
void ewk_view_toolbars_visible_get(Evas_Object* o, bool* visible);

void ewk_view_statusbar_visible_set(Evas_Object* o, bool visible);
void ewk_view_statusbar_visible_get(Evas_Object* o, bool* visible);
void ewk_view_statusbar_text_set(Evas_Object* o, const char* text);

void ewk_view_scrollbars_visible_set(Evas_Object* o, bool visible);
void ewk_view_scrollbars_visible_get(Evas_Object* o, bool* visible);

void ewk_view_menubar_visible_set(Evas_Object* o, bool visible);
void ewk_view_menubar_visible_get(Evas_Object* o, bool* visible);

void ewk_view_tooltip_text_set(Evas_Object* o, const char* text);

void ewk_view_add_console_message(Evas_Object* o, const char* message, unsigned int lineNumber, const char* sourceID);

void ewk_view_run_javascript_alert(Evas_Object* o, Evas_Object* frame, const char* message);
bool ewk_view_run_javascript_confirm(Evas_Object* o, Evas_Object* frame, const char* message);
bool ewk_view_run_javascript_prompt(Evas_Object* o, Evas_Object* frame, const char* message, const char* defaultValue, char** value);
bool ewk_view_should_interrupt_javascript(Evas_Object* o);
uint64_t ewk_view_exceeded_database_quota(Evas_Object* o, Evas_Object* frame, const char* databaseName, uint64_t current_size, uint64_t expected_size);

bool ewk_view_run_open_panel(Evas_Object* ewkView, Evas_Object* frame, bool allowsMultipleFiles, const Vector<String>& acceptMIMETypes, Eina_List** selectedFilenames);

void ewk_view_repaint(Evas_Object* o, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);
void ewk_view_scroll(Evas_Object* o, Evas_Coord dx, Evas_Coord dy, Evas_Coord sx, Evas_Coord sy, Evas_Coord sw, Evas_Coord sh, Evas_Coord cx, Evas_Coord cy, Evas_Coord cw, Evas_Coord ch, bool main_frame);
WebCore::Page* ewk_view_core_page_get(const Evas_Object* o);

WTF::PassRefPtr<WebCore::Frame> ewk_view_frame_create(Evas_Object* o, Evas_Object* frame, const WTF::String& name, WebCore::HTMLFrameOwnerElement* ownerElement, const WebCore::KURL& url, const WTF::String& referrer);

WTF::PassRefPtr<WebCore::Widget> ewk_view_plugin_create(Evas_Object* o, Evas_Object* frame, const WebCore::IntSize& pluginSize, WebCore::HTMLPlugInElement* element, const WebCore::KURL& url, const WTF::Vector<WTF::String>& paramNames, const WTF::Vector<WTF::String>& paramValues, const WTF::String& mimeType, bool loadManually);

void ewk_view_popup_new(Evas_Object* o, WebCore::PopupMenuClient* client, int selected, const WebCore::IntRect& rect);
void ewk_view_viewport_attributes_set(Evas_Object* o, const WebCore::ViewportArguments& arguments);

void ewk_view_download_request(Evas_Object* o, Ewk_Download* download);

void ewk_view_editor_client_contents_changed(Evas_Object* o);
void ewk_view_editor_client_selection_changed(Evas_Object* o);

bool ewk_view_focus_can_cycle(Evas_Object *o, Ewk_Focus_Direction direction);

#if ENABLE(NETSCAPE_PLUGIN_API)
void ewk_view_js_window_object_clear(Evas_Object* o, Evas_Object* frame);
#endif

int ewk_util_dpi_get();

#if ENABLE(TOUCH_EVENTS)
void ewk_view_need_touch_events_set(Evas_Object*, bool needed);
bool ewk_view_need_touch_events_get(const Evas_Object*);
#endif

Ewk_History_Item *ewk_history_item_new_from_core(WebCore::HistoryItem *core);
Ewk_History* ewk_history_new(WebCore::BackForwardListImpl* history);
void ewk_history_free(Ewk_History* history);

#if ENABLE(CONTEXT_MENUS)
Ewk_Context_Menu* ewk_context_menu_new(Evas_Object* view, WebCore::ContextMenuController* controller);
bool ewk_context_menu_free(Ewk_Context_Menu* o);
void ewk_context_menu_item_append(Ewk_Context_Menu* o, WebCore::ContextMenuItem& core);
Ewk_Context_Menu* ewk_context_menu_customize(Ewk_Context_Menu* o);
void ewk_context_menu_show(Ewk_Context_Menu* o);
#endif

const Eina_Rectangle* ewk_view_repaints_get(const Ewk_View_Private_Data* priv, size_t* count);
const Ewk_Scroll_Request* ewk_view_scroll_requests_get(const Ewk_View_Private_Data* priv, size_t* count);

void ewk_view_repaint_add(Ewk_View_Private_Data* priv, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);

void ewk_view_layout_if_needed_recursive(Ewk_View_Private_Data* priv);

Ewk_Window_Features* ewk_window_features_new_from_core(const WebCore::WindowFeatures* core);

Evas_Object* ewk_frame_add(Evas* e);
bool ewk_frame_init(Evas_Object* o, Evas_Object* view, WebCore::Frame* frame);
bool ewk_frame_child_add(Evas_Object* o, WTF::PassRefPtr<WebCore::Frame> child, const WTF::String& name, const WebCore::KURL& url, const WTF::String& referrer);
void ewk_frame_view_set(Evas_Object* o, Evas_Object* newParent);

void ewk_frame_core_gone(Evas_Object* o);

void ewk_frame_load_started(Evas_Object* o);
void ewk_frame_load_provisional(Evas_Object* o);
void ewk_frame_load_firstlayout_finished(Evas_Object* o);
void ewk_frame_load_firstlayout_nonempty_finished(Evas_Object* o);
void ewk_frame_load_document_finished(Evas_Object* o);
void ewk_frame_load_finished(Evas_Object* o, const char* error_domain, int error_code, bool is_cancellation, const char* error_description, const char* failing_url);
void ewk_frame_load_error(Evas_Object* o, const char* error_domain, int error_code, bool is_cancellation, const char* error_description, const char* failing_url);
void ewk_frame_load_progress_changed(Evas_Object* o);

void ewk_frame_request_will_send(Evas_Object* o, Ewk_Frame_Resource_Request* request);
void ewk_frame_request_assign_identifier(Evas_Object* o, const Ewk_Frame_Resource_Request* request);
void ewk_frame_view_state_save(Evas_Object* o, WebCore::HistoryItem* item);

void ewk_frame_did_perform_first_navigation(Evas_Object* o);

void ewk_frame_contents_size_changed(Evas_Object* o, Evas_Coord w, Evas_Coord h);
void ewk_frame_title_set(Evas_Object* o, const char* title);

void ewk_frame_view_create_for_view(Evas_Object* o, Evas_Object* view);
bool ewk_frame_uri_changed(Evas_Object* o);
void ewk_frame_force_layout(Evas_Object* o);

WTF::PassRefPtr<WebCore::Widget> ewk_frame_plugin_create(Evas_Object* o, const WebCore::IntSize& pluginSize, WebCore::HTMLPlugInElement* element, const WebCore::KURL& url, const WTF::Vector<WTF::String>& paramNames, const WTF::Vector<WTF::String>& paramValues, const WTF::String& mimeType, bool loadManually);

bool ewk_view_navigation_policy_decision(Evas_Object* o, Ewk_Frame_Resource_Request* request);

void ewk_view_contents_size_changed(Evas_Object* o, Evas_Coord w, Evas_Coord h);

WebCore::FloatRect ewk_view_page_rect_get(const Evas_Object* o);

const char* ewk_settings_default_user_agent_get();

void ewk_frame_editor_client_contents_changed(Evas_Object* o);
void ewk_frame_editor_client_selection_changed(Evas_Object* o);

#ifdef __cplusplus

}
#endif
#endif // ewk_private_h
