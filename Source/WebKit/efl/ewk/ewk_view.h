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

#ifndef ewk_view_h
#define ewk_view_h

#include "ewk_frame.h"
#include "ewk_history.h"
#include "ewk_window_features.h"

#include <Evas.h>
#include <cairo.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WebKit main smart object.
 *
 * This object is the high level access to WebKit-EFL browser
 * component. It is responsible for managing the main frame and other
 * critical resources.
 *
 * Every ewk_view has at least one frame, called "main frame" and
 * retrieved with ewk_view_frame_main_get(). Direct frame access is
 * often discouraged, it is recommended to use ewk_view functions
 * instead.
 *
 * The following signals (see evas_object_smart_callback_add()) are emitted:
 *
 *  - "ready", void: page is fully loaded.
 *  - "title,changed", const char*: title of the main frame changed.
 *  - "uri,changed", const char*: uri of the main frame changed.
 *  - "load,started", void: frame started loading.
 *  - "load,progress", double*: load progress changed (overall value
 *    from 0.0 to 1.0, connect to individual frames for fine grained).
 *  - "load,finished", const Ewk_Frame_Load_Error*: reports load
 *    finished and as argument @c NULL if successfully or pointer to
 *    structure defining the error.
 *  - "load,provisional", void: view started provisional load.
 *  - "load,error", const Ewk_Frame_Load_Error*: reports load failed
 *    and as argument a pointer to structure defining the error.
 *  - "frame,created", Evas_Object*: when frames are created, they are
 *    emitted in this signal.
 *  - "zoom,animated,end", void: requested animated zoom is finished.
 *  - "menubar,visible,set", Eina_Bool: set menubar visibility.
 *  - "menubar,visible,get", Eina_Bool *: expects a @c EINA_TRUE if menubar is
 *    visible; otherwise, @c EINA_FALSE.
 *  - "menubar,visible,set", Eina_Bool: set menubar visibility.
 *  - "menubar,visible,get", Eina_Bool *: expects a @c EINA_TRUE if menubar is
 *    visible; @c EINA_FALSE, otherwise.
 *  - "scrollbars,visible,set", Eina_Bool: set scrollbars visibility.
 *  - "scrollbars,visible,get", Eina_Bool *: expects a @c EINA_TRUE if scrollbars
 *    are visible; @c EINA_FALSE, otherwise.
 *  - "statusbar,visible,set", Eina_Bool: set statusbar visibility.
 *  - "statusbar,visible,get", Eina_Bool *: expects a @c EINA_TRUE if statusbar is
 *    visible; @c EINA_FALSE, otherwise.
 *  - "toolbar,visible,set", Eina_Bool: set toolbar visibility.
 *  - "toolbar,visible,get", Eina_Bool *: expects a @c EINA_TRUE if toolbar
 *    is visible; @c EINA_FALSE, otherwise.
 *  - "link,hover,in", const char *link[2]: reports mouse is over a link and as
 *    argument gives the url in link[0] and link's title in link[1].
 *  - "link,hover,out", const char *link[2]: reports mouse moved out from a link
 *    and as argument gives the url in link[0] and link's title in link[1].
 *  - "popup,create", Ewk_Menu: reports that a new menu was created.
 *  - "popup,willdeleted", Ewk_Menu: reports that a previously created menu is
 *    about to be deleted.
 *  - "download,request", Ewk_Download: reports a download is being requested
 *    and as arguments gives its details.
 *  - "icon,received", void: main frame received an icon.
 *  - "viewport,changed", void: Report that viewport has changed.
 *  - "inputmethods,changed" with a boolean indicating whether it's enabled or not.
 *  - "view,resized", void: view object's size has changed.
 */

typedef struct _Ewk_View_Smart_Data Ewk_View_Smart_Data;

/**
 * Ewk view's class, to be overridden by sub-classes.
 */
typedef struct _Ewk_View_Smart_Class Ewk_View_Smart_Class;
struct _Ewk_View_Smart_Class {
    Evas_Smart_Class sc; /**< all but 'data' is free to be changed. */
    unsigned long version;

    Evas_Object *(*window_create)(Ewk_View_Smart_Data *sd, Eina_Bool javascript, const Ewk_Window_Features *window_features); /**< creates a new window, requested by webkit */
    void (*window_close)(Ewk_View_Smart_Data *sd); /**< creates a new window, requested by webkit */
    // hooks to allow different backing stores
    Evas_Object *(*backing_store_add)(Ewk_View_Smart_Data *sd); /**< must be defined */
    Eina_Bool (*scrolls_process)(Ewk_View_Smart_Data *sd); /**< must be defined */
    Eina_Bool (*repaints_process)(Ewk_View_Smart_Data *sd); /**< must be defined */
    Eina_Bool (*contents_resize)(Ewk_View_Smart_Data *sd, int w, int h);
    Eina_Bool (*zoom_set)(Ewk_View_Smart_Data *sd, float zoom, Evas_Coord cx, Evas_Coord cy);
    Eina_Bool (*zoom_weak_set)(Ewk_View_Smart_Data *sd, float zoom, Evas_Coord cx, Evas_Coord cy);
    void (*zoom_weak_smooth_scale_set)(Ewk_View_Smart_Data *sd, Eina_Bool smooth_scale);
    void (*bg_color_set)(Ewk_View_Smart_Data *sd, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
    void (*flush)(Ewk_View_Smart_Data *sd);
    Eina_Bool (*pre_render_region)(Ewk_View_Smart_Data *sd, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, float zoom);
    Eina_Bool (*pre_render_relative_radius)(Ewk_View_Smart_Data *sd, unsigned int n, float zoom);
    void (*pre_render_cancel)(Ewk_View_Smart_Data *sd);
    Eina_Bool (*disable_render)(Ewk_View_Smart_Data *sd);
    Eina_Bool (*enable_render)(Ewk_View_Smart_Data *sd);

    // event handling:
    //  - returns true if handled
    //  - if overridden, have to call parent method if desired
    Eina_Bool (*focus_in)(Ewk_View_Smart_Data *sd);
    Eina_Bool (*focus_out)(Ewk_View_Smart_Data *sd);
    Eina_Bool (*mouse_wheel)(Ewk_View_Smart_Data *sd, const Evas_Event_Mouse_Wheel *ev);
    Eina_Bool (*mouse_down)(Ewk_View_Smart_Data *sd, const Evas_Event_Mouse_Down *ev);
    Eina_Bool (*mouse_up)(Ewk_View_Smart_Data *sd, const Evas_Event_Mouse_Up *ev);
    Eina_Bool (*mouse_move)(Ewk_View_Smart_Data *sd, const Evas_Event_Mouse_Move *ev);
    Eina_Bool (*key_down)(Ewk_View_Smart_Data *sd, const Evas_Event_Key_Down *ev);
    Eina_Bool (*key_up)(Ewk_View_Smart_Data *sd, const Evas_Event_Key_Up *ev);

    void (*add_console_message)(Ewk_View_Smart_Data *sd, const char *message, unsigned int lineNumber, const char *sourceID);
    void (*run_javascript_alert)(Ewk_View_Smart_Data *sd, Evas_Object *frame, const char *message);
    Eina_Bool (*run_javascript_confirm)(Ewk_View_Smart_Data *sd, Evas_Object *frame, const char *message);
    Eina_Bool (*run_javascript_prompt)(Ewk_View_Smart_Data *sd, Evas_Object *frame, const char *message, const char *defaultValue, char **value);
    Eina_Bool (*should_interrupt_javascript)(Ewk_View_Smart_Data *sd);
    uint64_t (*exceeded_database_quota)(Ewk_View_Smart_Data *sd, Evas_Object *frame, const char *databaseName, uint64_t current_size, uint64_t expected_size);

    Eina_Bool (*run_open_panel)(Ewk_View_Smart_Data *sd, Evas_Object *frame, Eina_Bool allows_multiple_files, const Eina_List *suggested_filenames, Eina_List **selected_filenames);

    Eina_Bool (*navigation_policy_decision)(Ewk_View_Smart_Data *sd, Ewk_Frame_Resource_Request *request);
};

#define EWK_VIEW_SMART_CLASS_VERSION 1UL /** the version you have to put into the version field in the Ewk_View_Smart_Class structure */

/**
 * Initializer for whole Ewk_View_Smart_Class structure.
 *
 * @param smart_class_init initializer to use for the "base" field
 * (Evas_Smart_Class).
 *
 * @see EWK_VIEW_SMART_CLASS_INIT_NULL
 * @see EWK_VIEW_SMART_CLASS_INIT_VERSION
 * @see EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION
 */
#define EWK_VIEW_SMART_CLASS_INIT(smart_class_init) {smart_class_init, EWK_VIEW_SMART_CLASS_VERSION, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

/**
 * Initializer to zero a whole Ewk_View_Smart_Class structure.
 *
 * @see EWK_VIEW_SMART_CLASS_INIT_VERSION
 * @see EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION
 * @see EWK_VIEW_SMART_CLASS_INIT
 */
#define EWK_VIEW_SMART_CLASS_INIT_NULL EWK_VIEW_SMART_CLASS_INIT(EVAS_SMART_CLASS_INIT_NULL)

/**
 * Initializer to zero a whole Ewk_View_Smart_Class structure and set version.
 *
 * Similar to EWK_VIEW_SMART_CLASS_INIT_NULL, but will set version field of
 * Evas_Smart_Class (base field) to latest EVAS_SMART_CLASS_VERSION
 *
 * @see EWK_VIEW_SMART_CLASS_INIT_NULL
 * @see EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION
 * @see EWK_VIEW_SMART_CLASS_INIT
 */
#define EWK_VIEW_SMART_CLASS_INIT_VERSION EWK_VIEW_SMART_CLASS_INIT(EVAS_SMART_CLASS_INIT_VERSION)

/**
 * Initializer to zero a whole Ewk_View_Smart_Class structure and set
 * name and version.
 *
 * Similar to EWK_VIEW_SMART_CLASS_INIT_NULL, but will set version field of
 * Evas_Smart_Class (base field) to latest EVAS_SMART_CLASS_VERSION and name
 * to the specific value.
 *
 * It will keep a reference to name field as a "const char *", that is,
 * name must be available while the structure is used (hint: static or global!)
 * and will not be modified.
 *
 * @see EWK_VIEW_SMART_CLASS_INIT_NULL
 * @see EWK_VIEW_SMART_CLASS_INIT_VERSION
 * @see EWK_VIEW_SMART_CLASS_INIT
 */
#define EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION(name) EWK_VIEW_SMART_CLASS_INIT(EVAS_SMART_CLASS_INIT_NAME_VERSION(name))

enum _Ewk_Imh {
    EWK_IMH_TELEPHONE = (1 << 0),
    EWK_IMH_NUMBER = (1 << 1),
    EWK_IMH_EMAIL = (1 << 2),
    EWK_IMH_URL = (1 << 3),
    EWK_IMH_PASSWORD = (1 << 4)
};
typedef enum _Ewk_Imh Ewk_Imh;

/**
 * @internal
 *
 * private data that is used internally by EFL WebKit and should never
 * be modified from outside.
 */
typedef struct _Ewk_View_Private_Data Ewk_View_Private_Data;

enum _Ewk_Menu_Item_Type {
    EWK_MENU_SEPARATOR,
    EWK_MENU_GROUP,
    EWK_MENU_OPTION
};
typedef enum _Ewk_Menu_Item_Type Ewk_Menu_Item_Type;


/**
 * Structure do contain data of each menu item
 */
typedef struct _Ewk_Menu_Item Ewk_Menu_Item;
struct _Ewk_Menu_Item {
    const char *text; /**< Item's text */
    Ewk_Menu_Item_Type type; /** Item's type */
};

/**
 * Structure to contain Popup menu data.
 */
typedef struct _Ewk_Menu Ewk_Menu;
struct _Ewk_Menu {
        Eina_List* items;
        int x;
        int y;
        int width;
        int height;
};

/**
 * Structure to contain Download data
 */
typedef struct _Ewk_Download Ewk_Download;
struct _Ewk_Download {
    const char* url;
    /* to be extended */
};

/**
 * Scroll request that should be processed by subclass implementations.
 */
typedef struct _Ewk_Scroll_Request Ewk_Scroll_Request;
struct _Ewk_Scroll_Request {
    Evas_Coord dx, dy;
    Evas_Coord x, y, w, h, x2, y2;
    Eina_Bool main_scroll;
};

/**
 * Structure to contain internal View data, it is to be considered
 * private by users, but may be extended or changed by sub-classes
 * (that's why it's in public header file).
 */
struct _Ewk_View_Smart_Data {
    Evas_Object_Smart_Clipped_Data base;
    const Ewk_View_Smart_Class *api; /**< reference to casted class instance */
    Evas_Object *self; /**< reference to owner object */
    Evas_Object *main_frame; /**< reference to main frame object */
    Evas_Object *backing_store; /**< reference to backing store */
    Evas_Object *events_rect; /**< rectangle that should receive mouse events */
    Ewk_View_Private_Data *_priv; /**< should never be accessed, c++ stuff */
    struct {
        Evas_Coord x, y, w, h; /**< last used viewport */
    } view;
    struct {
        struct {
            float start;
            float end;
            float current; /**< if > 0.0, then doing animated zoom. */
        } zoom;
    } animated_zoom;
    struct {
        unsigned char r, g, b, a;
    } bg_color;
    Eina_Bool zoom_weak_smooth_scale:1;
    struct { /**< what changed since last smart_calculate */
        Eina_Bool any:1;
        Eina_Bool size:1;
        Eina_Bool position:1;
        Eina_Bool frame_rect:1;
    } changed;
};

/**
 * Cache (pool) that contains unused tiles for ewk_view_tiled.
 *
 * This cache will maintain unused tiles and flush them when the total
 * memory exceeds the set amount when
 * ewk_tile_unused_cache_auto_flush() or explicitly set value when
 * ewk_tile_unused_cache_flush() is called.
 *
 * The tile may be shared among different ewk_view_tiled instances to
 * group maximum unused memory resident in the system.
 */
typedef struct _Ewk_Tile_Unused_Cache Ewk_Tile_Unused_Cache;
EAPI void   ewk_tile_unused_cache_max_set(Ewk_Tile_Unused_Cache *tuc, size_t max);
EAPI size_t ewk_tile_unused_cache_max_get(const Ewk_Tile_Unused_Cache *tuc);
EAPI size_t ewk_tile_unused_cache_used_get(const Ewk_Tile_Unused_Cache *tuc);
EAPI size_t ewk_tile_unused_cache_flush(Ewk_Tile_Unused_Cache *tuc, size_t bytes);
EAPI void   ewk_tile_unused_cache_auto_flush(Ewk_Tile_Unused_Cache *tuc);

EAPI Eina_Bool    ewk_view_base_smart_set(Ewk_View_Smart_Class *api);
EAPI Eina_Bool    ewk_view_single_smart_set(Ewk_View_Smart_Class *api);
EAPI Eina_Bool    ewk_view_tiled_smart_set(Ewk_View_Smart_Class *api);

EAPI Evas_Object *ewk_view_single_add(Evas *e);
EAPI Evas_Object *ewk_view_tiled_add(Evas *e);

EAPI Ewk_Tile_Unused_Cache *ewk_view_tiled_unused_cache_get(const Evas_Object *o);
EAPI void                   ewk_view_tiled_unused_cache_set(Evas_Object *o, Ewk_Tile_Unused_Cache *cache);

// FIXME: this function should be removed later, when we find the best flag to use.
EAPI void                   ewk_view_tiled_process_entire_queue_set(Evas_Object *o, Eina_Bool flag);

EAPI void         ewk_view_fixed_layout_size_set(Evas_Object *o, Evas_Coord w, Evas_Coord h);
EAPI void         ewk_view_fixed_layout_size_get(Evas_Object *o, Evas_Coord *w, Evas_Coord *h);

EAPI void         ewk_view_theme_set(Evas_Object *o, const char *path);
EAPI const char  *ewk_view_theme_get(Evas_Object *o);

EAPI Evas_Object *ewk_view_frame_main_get(const Evas_Object *o);
EAPI Evas_Object *ewk_view_frame_focused_get(const Evas_Object *o);

EAPI Eina_Bool    ewk_view_uri_set(Evas_Object *o, const char *uri);
EAPI const char  *ewk_view_uri_get(const Evas_Object *o);
EAPI const char  *ewk_view_title_get(const Evas_Object *o);

EAPI Eina_Bool    ewk_view_editable_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_editable_set(Evas_Object *o, Eina_Bool editable);

EAPI void         ewk_view_bg_color_set(Evas_Object *o, int r, int g, int b, int a);
EAPI void         ewk_view_bg_color_get(const Evas_Object *o, int *r, int *g, int *b, int *a);

EAPI char        *ewk_view_selection_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_select_none(Evas_Object *o);
EAPI Eina_Bool    ewk_view_select_all(Evas_Object *o);
EAPI Eina_Bool    ewk_view_select_paragraph(Evas_Object *o);
EAPI Eina_Bool    ewk_view_select_sentence(Evas_Object *o);
EAPI Eina_Bool    ewk_view_select_line(Evas_Object *o);
EAPI Eina_Bool    ewk_view_select_word(Evas_Object *o);

EAPI Eina_Bool    ewk_view_context_menu_forward_event(Evas_Object *o, const Evas_Event_Mouse_Down *ev);

EAPI void         ewk_view_popup_selected_set(Evas_Object *o, int index);
EAPI Eina_Bool    ewk_view_popup_destroy(Evas_Object *o);

EAPI Eina_Bool    ewk_view_text_search(const Evas_Object *o, const char *string, Eina_Bool case_sensitive, Eina_Bool forward, Eina_Bool wrap);

EAPI unsigned int ewk_view_text_matches_mark(Evas_Object *o, const char *string, Eina_Bool case_sensitive, Eina_Bool highlight, unsigned int limit);
EAPI Eina_Bool    ewk_view_text_matches_unmark_all(Evas_Object *o);
EAPI Eina_Bool    ewk_view_text_matches_highlight_set(Evas_Object *o, Eina_Bool highlight);
EAPI Eina_Bool    ewk_view_text_matches_highlight_get(const Evas_Object *o);

EAPI double       ewk_view_load_progress_get(const Evas_Object *o);

EAPI Eina_Bool    ewk_view_stop(Evas_Object *o);
EAPI Eina_Bool    ewk_view_reload(Evas_Object *o);
EAPI Eina_Bool    ewk_view_reload_full(Evas_Object *o);

EAPI Eina_Bool    ewk_view_back(Evas_Object *o);
EAPI Eina_Bool    ewk_view_forward(Evas_Object *o);
EAPI Eina_Bool    ewk_view_navigate(Evas_Object *o, int steps);

EAPI Eina_Bool    ewk_view_back_possible(Evas_Object *o);
EAPI Eina_Bool    ewk_view_forward_possible(Evas_Object *o);
EAPI Eina_Bool    ewk_view_navigate_possible(Evas_Object *o, int steps);

EAPI Eina_Bool    ewk_view_history_enable_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_history_enable_set(Evas_Object *o, Eina_Bool enable);
EAPI Ewk_History *ewk_view_history_get(const Evas_Object *o);

EAPI float        ewk_view_zoom_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_zoom_set(Evas_Object *o, float zoom, Evas_Coord cx, Evas_Coord cy);

EAPI Eina_Bool    ewk_view_zoom_weak_smooth_scale_get(const Evas_Object *o);
EAPI void         ewk_view_zoom_weak_smooth_scale_set(Evas_Object *o, Eina_Bool smooth_scale);

EAPI Eina_Bool    ewk_view_zoom_weak_set(Evas_Object *o, float zoom, Evas_Coord cx, Evas_Coord cy);
EAPI Eina_Bool    ewk_view_zoom_animated_mark_start(Evas_Object *o, float zoom);
EAPI Eina_Bool    ewk_view_zoom_animated_mark_end(Evas_Object *o, float zoom);
EAPI Eina_Bool    ewk_view_zoom_animated_mark_current(Evas_Object *o, float zoom);
EAPI Eina_Bool    ewk_view_zoom_animated_mark_stop(Evas_Object *o);

EAPI Eina_Bool    ewk_view_zoom_animated_set(Evas_Object *o, float zoom, float duration, Evas_Coord cx, Evas_Coord cy);
EAPI Eina_Bool    ewk_view_zoom_text_only_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_zoom_text_only_set(Evas_Object *o, Eina_Bool setting);

EAPI Eina_Bool    ewk_view_pre_render_region(Evas_Object *o, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, float zoom);
EAPI Eina_Bool    ewk_view_pre_render_relative_radius(Evas_Object *o, unsigned int n);
EAPI void         ewk_view_pre_render_cancel(Evas_Object *o);
EAPI Eina_Bool    ewk_view_enable_render(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_disable_render(const Evas_Object *o);

EAPI unsigned int ewk_view_imh_get(Evas_Object *o);

/* settings */
EAPI const char  *ewk_view_setting_user_agent_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_user_agent_set(Evas_Object *o, const char *user_agent);

EAPI Eina_Bool    ewk_view_setting_auto_load_images_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_auto_load_images_set(Evas_Object *o, Eina_Bool automatic);

EAPI Eina_Bool    ewk_view_setting_auto_shrink_images_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_auto_shrink_images_set(Evas_Object *o, Eina_Bool automatic);

EAPI Eina_Bool    ewk_view_setting_enable_auto_resize_window_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_enable_auto_resize_window_set(Evas_Object *o, Eina_Bool resizable);
EAPI Eina_Bool    ewk_view_setting_enable_scripts_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_enable_scripts_set(Evas_Object *o, Eina_Bool enable);

EAPI Eina_Bool    ewk_view_setting_enable_plugins_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_enable_plugins_set(Evas_Object *o, Eina_Bool enable);

EAPI Eina_Bool    ewk_view_setting_enable_frame_flattening_get(const Evas_Object* o);
EAPI Eina_Bool    ewk_view_setting_enable_frame_flattening_set(Evas_Object* o, Eina_Bool enable);

EAPI Eina_Bool    ewk_view_setting_scripts_window_open_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_scripts_window_open_set(Evas_Object *o, Eina_Bool allow);

EAPI Eina_Bool    ewk_view_setting_resizable_textareas_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_resizable_textareas_set(Evas_Object *o, Eina_Bool enable);

EAPI const char  *ewk_view_setting_user_stylesheet_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_user_stylesheet_set(Evas_Object *o, const char *uri);

EAPI Eina_Bool    ewk_view_setting_private_browsing_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_private_browsing_set(Evas_Object *o, Eina_Bool enable);
EAPI Eina_Bool    ewk_view_setting_offline_app_cache_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_offline_app_cache_set(Evas_Object *o, Eina_Bool enable);

EAPI Eina_Bool    ewk_view_setting_caret_browsing_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_caret_browsing_set(Evas_Object *o, Eina_Bool enable);

EAPI const char  *ewk_view_setting_encoding_custom_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_encoding_custom_set(Evas_Object *o, const char *encoding);
EAPI const char  *ewk_view_setting_encoding_default_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_encoding_default_set(Evas_Object *o, const char *encoding);

EAPI int          ewk_view_setting_font_minimum_size_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_font_minimum_size_set(Evas_Object *o, int size);
EAPI int          ewk_view_setting_font_minimum_logical_size_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_font_minimum_logical_size_set(Evas_Object *o, int size);
EAPI int          ewk_view_setting_font_default_size_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_font_default_size_set(Evas_Object *o, int size);
EAPI int          ewk_view_setting_font_monospace_size_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_font_monospace_size_set(Evas_Object *o, int size);

EAPI const char  *ewk_view_setting_font_standard_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_font_standard_set(Evas_Object *o, const char *family);

EAPI const char  *ewk_view_setting_font_cursive_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_font_cursive_set(Evas_Object *o, const char *family);

EAPI const char  *ewk_view_setting_font_monospace_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_font_monospace_set(Evas_Object *o, const char *family);

EAPI const char  *ewk_view_setting_font_fantasy_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_font_fantasy_set(Evas_Object *o, const char *family);

EAPI const char  *ewk_view_setting_font_serif_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_font_serif_set(Evas_Object *o, const char *family);

EAPI const char  *ewk_view_setting_font_sans_serif_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_font_sans_serif_set(Evas_Object *o, const char *family);

EAPI Eina_Bool    ewk_view_setting_spatial_navigation_get(Evas_Object* o);
EAPI Eina_Bool    ewk_view_setting_spatial_navigation_set(Evas_Object* o, Eina_Bool enable);

EAPI Eina_Bool    ewk_view_setting_local_storage_get(Evas_Object* o);
EAPI Eina_Bool    ewk_view_setting_local_storage_set(Evas_Object* o, Eina_Bool enable);
EAPI const char  *ewk_view_setting_local_storage_database_path_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_local_storage_database_path_set(Evas_Object *o, const char *path);

EAPI Eina_Bool    ewk_view_setting_page_cache_get(Evas_Object* o);
EAPI Eina_Bool    ewk_view_setting_page_cache_set(Evas_Object* o, Eina_Bool enable);

EAPI Eina_Bool    ewk_view_setting_encoding_detector_get(Evas_Object* o);
EAPI Eina_Bool    ewk_view_setting_encoding_detector_set(Evas_Object* o, Eina_Bool enable);

/* to be used by subclass implementations */
EAPI Ewk_View_Smart_Data *ewk_view_smart_data_get(const Evas_Object *o);

EAPI const Eina_Rectangle *ewk_view_repaints_get(const Ewk_View_Private_Data *priv, size_t *count);
EAPI const Ewk_Scroll_Request *ewk_view_scroll_requests_get(const Ewk_View_Private_Data *priv, size_t *count);

EAPI void ewk_view_repaint_add(Ewk_View_Private_Data *priv, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);

EAPI void ewk_view_layout_if_needed_recursive(Ewk_View_Private_Data *priv);

EAPI void ewk_view_scrolls_process(Ewk_View_Smart_Data *sd);

/**
 * Structure that keeps paint context.
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
typedef struct _Ewk_View_Paint_Context Ewk_View_Paint_Context;

EAPI Ewk_View_Paint_Context *ewk_view_paint_context_new(Ewk_View_Private_Data *priv, cairo_t *cr);
EAPI void ewk_view_paint_context_free(Ewk_View_Paint_Context *ctxt);

EAPI void ewk_view_paint_context_save(Ewk_View_Paint_Context *ctxt);
EAPI void ewk_view_paint_context_restore(Ewk_View_Paint_Context *ctxt);
EAPI void ewk_view_paint_context_clip(Ewk_View_Paint_Context *ctxt, const Eina_Rectangle *area);
EAPI void ewk_view_paint_context_paint(Ewk_View_Paint_Context *ctxt, const Eina_Rectangle *area);
EAPI void ewk_view_paint_context_paint_contents(Ewk_View_Paint_Context *ctxt, const Eina_Rectangle *area);
EAPI void ewk_view_paint_context_scale(Ewk_View_Paint_Context *ctxt, float scale_x, float scale_y);
EAPI void ewk_view_paint_context_translate(Ewk_View_Paint_Context *ctxt, float x, float y);

EAPI Eina_Bool ewk_view_paint(Ewk_View_Private_Data *priv, cairo_t *cr, const Eina_Rectangle *area);
EAPI Eina_Bool ewk_view_paint_contents(Ewk_View_Private_Data *priv, cairo_t *cr, const Eina_Rectangle *area);

EAPI void ewk_view_viewport_attributes_get(Evas_Object *o, float* w, float* h, float* init_scale, float* max_scale, float* min_scale, float* device_pixel_ratio , Eina_Bool* user_scalable);
EAPI Eina_Bool ewk_view_zoom_range_set(Evas_Object* o, float min_scale, float max_scale);
EAPI float ewk_view_zoom_range_min_get(Evas_Object* o);
EAPI float ewk_view_zoom_range_max_get(Evas_Object* o);
EAPI void ewk_view_user_scalable_set(Evas_Object* o, Eina_Bool user_scalable);
EAPI Eina_Bool ewk_view_user_scalable_get(Evas_Object* o);
EAPI float ewk_view_device_pixel_ratio_get(Evas_Object* o);

#ifdef __cplusplus
}
#endif
#endif // ewk_view_h
