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
 * @file    ewk_view.h
 * @brief   WebKit main smart object.
 *
 * This object allows the high level access to WebKit-EFL component.
 * It is responsible for managing the main frame and other
 * critical resources.
 *
 * Every ewk_view has at least one frame, called "main frame" and
 * retrieved with ewk_view_frame_main_get(). Direct frame access is
 * often discouraged, it is recommended to use ewk_view functions
 * instead.
 *
 * The following signals (see evas_object_smart_callback_add()) are emitted:
 *
 *  - "download,request", Ewk_Download: reports a download is being requested
 *  - "editorclient,contents,changed", void: reports to the view that editor
 *    client's contents were changed
 *  - "frame,created", Evas_Object*: a new frame is created.
 *  - "icon,received", void: main frame received an icon.
 *  - "inputmethod,changed", Eina_Bool: reports that input method was changed and
 *    it gives a boolean value whether it's enabled or not as an argument.
 *  - "link,hover,in", const char *link[2]: reports mouse is over a link.
 *    It gives the url in link[0] and link's title in link[1] as an argument.
 *  - "link,hover,out", void: reports mouse moved out from a link.
 *  - "load,error", const Ewk_Frame_Load_Error*: reports load failed
 *  - "load,finished", const Ewk_Frame_Load_Error*: reports load
 *    finished and it gives @c NULL on success or pointer to
 *    structure defining the error.
 *  - "load,newwindow,show", void: reports that a new window was created and can be shown.
 *    and it gives a pointer to structure defining the error as an argument.
 *  - "load,progress", double*: load progress is changed (overall value
 *    from 0.0 to 1.0, connect to individual frames for fine grained).
 *  - "load,provisional", void: view started provisional load.
 *  - "load,started", void: frame started loading the document.
 *  - "menubar,visible,get", Eina_Bool *: expects a @c EINA_TRUE if menubar is
 *    visible; @c EINA_FALSE, otherwise.
 *  - "menubar,visible,set", Eina_Bool: sets menubar visibility.
 *  - "ready", void: page is fully loaded.
 *  - "scrollbars,visible,get", Eina_Bool *: expects a @c EINA_TRUE if scrollbars
 *    are visible; @c EINA_FALSE, otherwise.
 *  - "scrollbars,visible,set", Eina_Bool: sets scrollbars visibility.
 *  - "statusbar,text,set", const char *: sets statusbar text.
 *  - "statusbar,visible,get", Eina_Bool *: expects a @c EINA_TRUE if statusbar is
 *    visible; @c EINA_FALSE, otherwise.
 *  - "statusbar,visible,set", Eina_Bool: sets statusbar visibility.
 *  - "title,changed", const char*: title of the main frame was changed.
 *  - "toolbars,visible,get", Eina_Bool *: expects a @c EINA_TRUE if toolbar
 *    is visible; @c EINA_FALSE, otherwise.
 *  - "toolbars,visible,set", Eina_Bool: sets toolbar visibility.
 *  - "popup,create", Ewk_Menu: reports that a new menu was created.
 *  - "popup,willdeleted", Ewk_Menu: reports that a previously created menu
 *    will be deleted.
 *  - "restore", Evas_Object *: reports that view should be restored to default conditions
 *    and it gives a frame that originated restore as an argument.
 *  - "tooltip,text,set", const char*: sets tooltip text and displays if it is currently hidden.
 *  - "uri,changed", const char*: uri of the main frame was changed.
 *  - "view,resized", void: view object's size was changed.
 *  - "viewport,changed", void: reports that viewport was changed.
 *  - "zoom,animated,end", void: requested animated zoom is finished.
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

/// Creates a type name for @a _Ewk_View_Smart_Data.
typedef struct _Ewk_View_Smart_Data Ewk_View_Smart_Data;

/// Creates a type name for @a _Ewk_View_Smart_Class.
typedef struct _Ewk_View_Smart_Class Ewk_View_Smart_Class;
/// Ewk view's class, to be overridden by sub-classes.
struct _Ewk_View_Smart_Class {
    Evas_Smart_Class sc; /**< All but 'data' is free to be changed. */
    unsigned long version;

    Evas_Object *(*window_create)(Ewk_View_Smart_Data *sd, Eina_Bool javascript, const Ewk_Window_Features *window_features); /**< creates a new window, requested by webkit */
    void (*window_close)(Ewk_View_Smart_Data *sd); /**< closes a window */
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

    Eina_Bool (*run_open_panel)(Ewk_View_Smart_Data *sd, Evas_Object *frame, Eina_Bool allows_multiple_files, const char *accept_types, Eina_List **selected_filenames);

    Eina_Bool (*navigation_policy_decision)(Ewk_View_Smart_Data *sd, Ewk_Frame_Resource_Request *request);
};

/**
 * The version you have to put into the version field
 * in the @a Ewk_View_Smart_Class structure.
 */
#define EWK_VIEW_SMART_CLASS_VERSION 1UL

/**
 * Initializes a whole @a Ewk_View_Smart_Class structure.
 *
 * @param smart_class_init initializer to use for the "base" field
 * @a Evas_Smart_Class
 *
 * @see EWK_VIEW_SMART_CLASS_INIT_NULL
 * @see EWK_VIEW_SMART_CLASS_INIT_VERSION
 * @see EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION
 */
#define EWK_VIEW_SMART_CLASS_INIT(smart_class_init) {smart_class_init, EWK_VIEW_SMART_CLASS_VERSION, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

/**
 * Initializes to zero a whole @a Ewk_View_Smart_Class structure.
 *
 * @see EWK_VIEW_SMART_CLASS_INIT_VERSION
 * @see EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION
 * @see EWK_VIEW_SMART_CLASS_INIT
 */
#define EWK_VIEW_SMART_CLASS_INIT_NULL EWK_VIEW_SMART_CLASS_INIT(EVAS_SMART_CLASS_INIT_NULL)

/**
 * Initializes to zero a whole @a Ewk_View_Smart_Class structure
 * and sets the version.
 *
 * Similar to @a EWK_VIEW_SMART_CLASS_INIT_NULL, but it sets the version field of
 * @a Evas_Smart_Class (base field) to latest @a EVAS_SMART_CLASS_VERSION.
 *
 * @see EWK_VIEW_SMART_CLASS_INIT_NULL
 * @see EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION
 * @see EWK_VIEW_SMART_CLASS_INIT
 */
#define EWK_VIEW_SMART_CLASS_INIT_VERSION EWK_VIEW_SMART_CLASS_INIT(EVAS_SMART_CLASS_INIT_VERSION)

/**
 * Initializes to zero a whole @a Ewk_View_Smart_Class structure
 * and sets the name and version.
 *
 * Similar to @a EWK_VIEW_SMART_CLASS_INIT_NULL, but it sets the version field of
 * @a Evas_Smart_Class (base field) to latest @a EVAS_SMART_CLASS_VERSION
 * and the name to the specific value.
 *
 * It will keep a reference to the name field as a "const char *", that is,
 * name must be available while the structure is used (hint: static or global!)
 * and it will not be modified.
 *
 * @see EWK_VIEW_SMART_CLASS_INIT_NULL
 * @see EWK_VIEW_SMART_CLASS_INIT_VERSION
 * @see EWK_VIEW_SMART_CLASS_INIT
 */
#define EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION(name) EWK_VIEW_SMART_CLASS_INIT(EVAS_SMART_CLASS_INIT_NAME_VERSION(name))

/// Defines the input method hints.
enum _Ewk_Imh {
    EWK_IMH_TELEPHONE = (1 << 0),
    EWK_IMH_NUMBER = (1 << 1),
    EWK_IMH_EMAIL = (1 << 2),
    EWK_IMH_URL = (1 << 3),
    EWK_IMH_PASSWORD = (1 << 4)
};
/// Creates a type name for @a _Ewk_Imh.
typedef enum _Ewk_Imh Ewk_Imh;

/// Creates a type name for @a _Ewk_View_Private_Data.
typedef struct _Ewk_View_Private_Data Ewk_View_Private_Data;

/// Defines the types of the items for the context menu.
enum _Ewk_Menu_Item_Type {
    EWK_MENU_SEPARATOR,
    EWK_MENU_GROUP,
    EWK_MENU_OPTION
};
/// Creates a type name for @a _Ewk_Menu_Item_Type.
typedef enum _Ewk_Menu_Item_Type Ewk_Menu_Item_Type;

/// Creates a type name for @a _Ewk_Menu_Item.
typedef struct _Ewk_Menu_Item Ewk_Menu_Item;
/// Contains data of each menu item.
struct _Ewk_Menu_Item {
    const char *text; /**< Text of the item. */
    Ewk_Menu_Item_Type type; /** Type of the item. */
};

/// Creates a type name for @a _Ewk_Menu.
typedef struct _Ewk_Menu Ewk_Menu;
/// Contains Popup menu data.
struct _Ewk_Menu {
        Eina_List *items; /**< List of items. */
        int x; /**< The horizontal position of Popup menu. */
        int y; /**< The vertical position of Popup menu. */
        int width; /**< Popup menu width. */
        int height; /**< Popup menu height. */
};

/// Creates a type name for @a _Ewk_Download.
typedef struct _Ewk_Download Ewk_Download;
/// Contains Download data.
struct _Ewk_Download {
    const char *url; /**< URL of resource. */
    /* to be extended */
};

/// Creates a type name for @a _Ewk_Scroll_Request.
typedef struct _Ewk_Scroll_Request Ewk_Scroll_Request;
/// Contains the scroll request that should be processed by subclass implementations.
struct _Ewk_Scroll_Request {
    Evas_Coord dx, dy;
    Evas_Coord x, y, w, h, x2, y2;
    Eina_Bool main_scroll;
};

/**
 * @brief Contains an internal View data.
 *
 * It is to be considered private by users, but may be extended or
 * changed by sub-classes (that's why it's in the public header file).
 */
struct _Ewk_View_Smart_Data {
    Evas_Object_Smart_Clipped_Data base;
    const Ewk_View_Smart_Class *api; /**< Reference to casted class instance. */
    Evas_Object *self; /**< Reference to owner object. */
    Evas_Object *main_frame; /**< Reference to main frame object. */
    Evas_Object *backing_store; /**< Reference to backing store. */
    Evas_Object *events_rect; /**< The rectangle that receives mouse events. */
    Ewk_View_Private_Data *_priv; /**< Should @b never be accessed, c++ stuff. */
    struct {
        Evas_Coord x, y, w, h;
    } view; /**< Contains the position and size of last used viewport. */
    struct {
        struct {
            float start;
            float end;
            float current; /**< if > 0.0, then doing animated zoom. */
        } zoom;
    } animated_zoom;
    struct {
        unsigned char r, g, b, a;
    } bg_color; /**< Keeps the background color. */
    Eina_Bool zoom_weak_smooth_scale:1;
    struct {
        Eina_Bool any:1;
        Eina_Bool size:1;
        Eina_Bool position:1;
        Eina_Bool frame_rect:1;
    } changed; /**< Keeps what changed since last smart_calculate. */
};

/// Defines the modes of view.
enum _Ewk_View_Mode {
    EWK_VIEW_MODE_WINDOWED,
    EWK_VIEW_MODE_FLOATING,
    EWK_VIEW_MODE_FULLSCREEN,
    EWK_VIEW_MODE_MAXIMIZED,
    EWK_VIEW_MODE_MINIMIZED
};
/// Creates a type name for @a _Ewk_View_Mode.
typedef enum _Ewk_View_Mode Ewk_View_Mode;

/**
 * @brief Creates a type name for @a _Ewk_Tile_Unused_Cache.
 *
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

/**
 * Change cache capacity, in bytes.
 *
 * This will not flush cache, use ewk_tile_unused_cache_flush() or
 * ewk_tile_unused_cache_auto_flush() to do so.
 */
EAPI void   ewk_tile_unused_cache_max_set(Ewk_Tile_Unused_Cache *tuc, size_t max);

/**
 * Retrieve maximum cache capacity, in bytes.
 */
EAPI size_t ewk_tile_unused_cache_max_get(const Ewk_Tile_Unused_Cache *tuc);

/**
 * Retrieve the used cache capacity, in bytes.
 */
EAPI size_t ewk_tile_unused_cache_used_get(const Ewk_Tile_Unused_Cache *tuc);

/**
 * Flush given amount of bytes from cache.
 *
 * After calling this function, near @a bytes are freed from cache. It
 * may be less if cache did not contain that amount of bytes (ie: an
 * empty cache has nothing to free!) or more if the cache just
 * contained objects that were larger than the requested amount (this
 * is usually the case).
 *
 * @param tuc cache of unused tiles.
 * @param bytes amount to free.
 *
 * @return amount really freed.
 *
 * @see ewk_tile_unused_cache_used_get()
 */
EAPI size_t ewk_tile_unused_cache_flush(Ewk_Tile_Unused_Cache *tuc, size_t bytes);

/**
 * Flush enough bytes to make cache usage lower than maximum.
 *
 * Just like ewk_tile_unused_cache_flush(), but this will make the cache
 * free enough tiles to respect maximum cache size as defined with
 * ewk_tile_unused_cache_max_set().
 *
 * This function is usually called when system becomes idle. This way
 * we keep memory low but do not impact performance when
 * creating/deleting tiles.
 */
EAPI void   ewk_tile_unused_cache_auto_flush(Ewk_Tile_Unused_Cache *tuc);

/**
 * Sets the smart class api without any backing store, enabling view
 * to be inherited.
 *
 * @param api class definition to be set, all members with the
 *        exception of Evas_Smart_Class->data may be overridden. Must
 *        @b not be @c 0.
 *
 * @note Evas_Smart_Class->data is used to implement type checking and
 *       is not supposed to be changed/overridden. If you need extra
 *       data for your smart class to work, just extend
 *       Ewk_View_Smart_Class instead.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE on failure (probably
 *         version mismatch).
 *
 * @see ewk_view_single_smart_set()
 * @see ewk_view_tiled_smart_set()
 */
EAPI Eina_Bool    ewk_view_base_smart_set(Ewk_View_Smart_Class *api);

/**
 * Sets the smart class api using single backing store, enabling view
 * to be inherited.
 *
 * @param api class definition to be set, all members with the
 *        exception of Evas_Smart_Class->data may be overridden. Must
 *        @b not be @c NULL.
 *
 * @note Evas_Smart_Class->data is used to implement type checking and
 *       is not supposed to be changed/overridden. If you need extra
 *       data for your smart class to work, just extend
 *       Ewk_View_Smart_Class instead.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE on failure (probably
 *         version mismatch).
 *
 * @see ewk_view_base_smart_set()
 */
EAPI Eina_Bool    ewk_view_single_smart_set(Ewk_View_Smart_Class *api);

/**
 * Sets the smart class api using tiled backing store, enabling view
 * to be inherited.
 *
 * @param api class definition to be set, all members with the
 *        exception of Evas_Smart_Class->data may be overridden. Must
 *        @b not be @c NULL.
 *
 * @note Evas_Smart_Class->data is used to implement type checking and
 *       is not supposed to be changed/overridden. If you need extra
 *       data for your smart class to work, just extend
 *       Ewk_View_Smart_Class instead.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE on failure (probably
 *         version mismatch).
 *
 * @see ewk_view_base_smart_set()
 */
EAPI Eina_Bool    ewk_view_tiled_smart_set(Ewk_View_Smart_Class *api);

/**
 * Creates a new EFL WebKit View object.
 *
 * View objects are the recommended way to deal with EFL WebKit as it
 * abstracts the complex pieces of the process.
 *
 * Each view is composed by a set of frames. The set has at least one
 * frame, called 'main_frame'. See ewk_view_frame_main_get() and
 * ewk_view_frame_focused_get().
 *
 * @param e canvas where to create the view object.
 *
 * @return view object or @c NULL if errors.
 *
 * @see ewk_view_uri_set()
 */
EAPI Evas_Object *ewk_view_single_add(Evas *e);

/**
 * Creates a new EFL WebKit View object using tiled backing store.
 *
 * View objects are the recommended way to deal with EFL WebKit as it
 * abstracts the complex pieces of the process.
 *
 * This object is almost the same as the one returned by the ewk_view_add()
 * function, but it uses the tiled backing store instead of the default
 * backing store.
 *
 * @param e canvas where to create the view object.
 *
 * @return view object or @c NULL if errors.
 *
 * @see ewk_view_uri_set()
 */
EAPI Evas_Object *ewk_view_tiled_add(Evas *e);

/**
 * Get the cache of unused tiles used by this view.
 *
 * @param o view object to get cache.
 * @return instance of "cache of unused tiles" or @c NULL on errors.
 */
EAPI Ewk_Tile_Unused_Cache *ewk_view_tiled_unused_cache_get(const Evas_Object *o);

/**
 * Set the cache of unused tiles used by this view.
 *
 * @param o view object to get cache.
 * @param cache instance of "cache of unused tiles". This can be used
 *        to share a single cache amongst different views. The tiles
 *        from one view will not be used by the other! This is just to
 *        limit the group with amount of unused memory.
 *        If @c NULL is provided, then a new cache is created.
 */
EAPI void                   ewk_view_tiled_unused_cache_set(Evas_Object *o, Ewk_Tile_Unused_Cache *cache);

// FIXME: this function should be removed later, when we find the best flag to use.
/**
 * Set the function with the same name of the tiled backing store.
 * @param o the tiled backing store object.
 * @param flag value of the tiled backing store flag to set.
 */
EAPI void                   ewk_view_tiled_process_entire_queue_set(Evas_Object *o, Eina_Bool flag);

/**
 * Set a fixed layout size to be used, dissociating it from viewport size.
 *
 * Setting a width different than zero enables fixed layout on that
 * size. It's automatically scaled based on zoom, but will not change
 * if viewport changes.
 *
 * Setting both @a w and @a h to zero will disable fixed layout.
 *
 * @param o view object to change fixed layout.
 * @param w fixed width to use. This size will be automatically scaled
 *        based on zoom level.
 * @param h fixed height to use. This size will be automatically scaled
 *        based on zoom level.
 */
EAPI void         ewk_view_fixed_layout_size_set(Evas_Object *o, Evas_Coord w, Evas_Coord h);

/**
 * Get fixed layout size in use.
 *
 * @param o view object to query fixed layout size.
 * @param w where to return width. Returns 0 on error or if no fixed
 *        layout in use.
 * @param h where to return height. Returns 0 on error or if no fixed
 *        layout in use.
 */
EAPI void         ewk_view_fixed_layout_size_get(Evas_Object *o, Evas_Coord *w, Evas_Coord *h);

/**
 * Set the theme path to be used by this view.
 *
 * This also sets the theme on the main frame. As frames inherit theme
 * from their parent, this will have all frames with unset theme to
 * use this one.
 *
 * @param o view object to change theme.
 * @param path theme path, may be @c 0 to reset to default.
 */
EAPI void         ewk_view_theme_set(Evas_Object *o, const char *path);

/**
 * Gets the theme set on this frame.
 *
 * This returns the value set by ewk_view_theme_set().
 *
 * @param o view object to get theme path.
 *
 * @return theme path, may be @c 0 if not set.
 */
EAPI const char  *ewk_view_theme_get(Evas_Object *o);

/**
 * Get the object that represents the main frame.
 *
 * @param o view object to get main frame.
 *
 * @return ewk_frame object or @c 0 if none yet.
 */
EAPI Evas_Object *ewk_view_frame_main_get(const Evas_Object *o);

/**
 * Get the currently focused frame object.
 *
 * @param o view object to get focused frame.
 *
 * @return ewk_frame object or @c 0 if none yet.
 */
EAPI Evas_Object *ewk_view_frame_focused_get(const Evas_Object *o);

/**
 * Ask main frame to load the given URI.
 *
 * @param o view object to load uri.
 * @param uri uniform resource identifier to load.
 *
 * @return @c EINA_TRUE on successful request, @c EINA_FALSE on failure.
 *         Note that it means the request was done, not that it was
 *         satisfied.
 */
EAPI Eina_Bool    ewk_view_uri_set(Evas_Object *o, const char *uri);

/**
 * Get the current uri loaded by main frame.
 *
 * @param o view object to get current uri.
 *
 * @return current uri reference or @c 0. It's internal, don't change.
 */
EAPI const char  *ewk_view_uri_get(const Evas_Object *o);

/**
 * Get the current title of main frame.
 *
 * @param o view object to get current title.
 *
 * @return current title reference or @c 0. It's internal, don't change.
 */
EAPI const char  *ewk_view_title_get(const Evas_Object *o);

/**
 * Gets if main frame is editable.
 *
 * @param o view object to get editable state.
 *
 * @return @c EINA_TRUE if editable, @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool    ewk_view_editable_get(const Evas_Object *o);

/**
 * Sets if main frame is editable.
 *
 * @param o view object to set editable state.
 * @param editable new state.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool    ewk_view_editable_set(Evas_Object *o, Eina_Bool editable);

/**
 * Set background color and transparency
 *
 * Just as in Evas, colors are pre-multiplied, so 50% red is
 * (128, 0, 0, 128) and not (255, 0, 0, 128)!
 *
 * @warning Watch out performance issues with transparency! Object
 *          will be handled as transparent image by evas even if the
 *          webpage specifies a background color. That mean you'll pay
 *          a price even if it's not really transparent, thus
 *          scrolling/panning and zooming will be likely slower than
 *          if transparency is off.
 *
 * @param o view object to change.
 * @param r red component.
 * @param g green component.
 * @param b blue component.
 * @param a transparency.
 */
EAPI void         ewk_view_bg_color_set(Evas_Object *o, int r, int g, int b, int a);

/**
 * Query if view object background color.
 *
 * Just as in Evas, colors are pre-multiplied, so 50% red is
 * (128, 0, 0, 128) and not (255, 0, 0, 128)!
 *
 * @param o view object to query.
 * @param r where to return red color component.
 * @param g where to return green color component.
 * @param b where to return blue color component.
 * @param a where to return alpha value.
 */
EAPI void         ewk_view_bg_color_get(const Evas_Object *o, int *r, int *g, int *b, int *a);

/**
 * Get the copy of the selection text.
 *
 * @param o view object to get selection text.
 *
 * @return newly allocated string or @c 0 if nothing is selected or failure.
 */
EAPI char        *ewk_view_selection_get(const Evas_Object *o);

/**
 * Forwards a request of new Context Menu to WebCore.
 *
 * @param o View.
 * @param ev Event data.
 *
 * @return @c EINA_TRUE if operation was executed, @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool    ewk_view_context_menu_forward_event(Evas_Object *o, const Evas_Event_Mouse_Down *ev);

enum _Ewk_Editor_Command {
    EWK_EDITOR_COMMAND_INSERT_IMAGE = 0,
    EWK_EDITOR_COMMAND_INSERT_TEXT,
    EWK_EDITOR_COMMAND_SELECT_NONE,
    EWK_EDITOR_COMMAND_SELECT_ALL,
    EWK_EDITOR_COMMAND_SELECT_PARAGRAPH,
    EWK_EDITOR_COMMAND_SELECT_SENTENCE,
    EWK_EDITOR_COMMAND_SELECT_LINE,
    EWK_EDITOR_COMMAND_SELECT_WORD
};
typedef enum _Ewk_Editor_Command Ewk_Editor_Command;

/**
 * Executes editor command.
 *
 * @param o view object to execute command.
 * @param command editor command to be executed.
 * @param value value to be passed into command.
 *
 * @return @c EINA_TRUE if operation was executed, @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool    ewk_view_execute_editor_command(Evas_Object *o, const Ewk_Editor_Command command, const char *value);

/**
 * Changes currently selected item.
 *
 * Changes the option selected in select widget. This is called by browser
 * whenever user has chosen a different item. Most likely after calling this, a
 * call to ewk_view_popup_destroy might be made in order to close the popup.
 *
 * @param o View.
 * @index Index of selected item.
 *
 */
EAPI void         ewk_view_popup_selected_set(Evas_Object *o, int index);

/**
 * Destroy a previously created menu.
 *
 * Before destroying, it informs client that menu's data is ready to be
 * destroyed by sending a "popup,willdelete" with a list of menu items. Then it
 * removes any reference to menu inside webkit. It's safe to call this
 * function either from inside webkit or from browser.
 *
 * @param o View.
 *
 * @returns EINA_TRUE in case menu was successfully destroyed or EINA_TRUE in
 * case there wasn't any menu to be destroyed.
 */
EAPI Eina_Bool    ewk_view_popup_destroy(Evas_Object *o);

/**
 * Search the given text string in document.
 *
 * @param o view object where to search text.
 * @param string reference string to search.
 * @param case_sensitive if search should be case sensitive or not.
 * @param forward if search is from cursor and on or backwards.
 * @param wrap if search should wrap at end.
 *
 * @return @c EINA_TRUE if found, @c EINA_FALSE if not or failure.
 */
EAPI Eina_Bool    ewk_view_text_search(const Evas_Object *o, const char *string, Eina_Bool case_sensitive, Eina_Bool forward, Eina_Bool wrap);

/**
 * Get if should highlight matches marked with ewk_view_text_matches_mark().
 *
 * @param o view object to query if matches are highlighted or not.
 *
 * @return @c EINA_TRUE if they are highlighted, @c EINA_FALSE otherwise.
 */
EAPI unsigned int ewk_view_text_matches_mark(Evas_Object *o, const char *string, Eina_Bool case_sensitive, Eina_Bool highlight, unsigned int limit);

/**
 * Reverses the effect of ewk_view_text_matches_mark()
 *
 * @param o view object where to search text.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE for failure.
 */
EAPI Eina_Bool    ewk_view_text_matches_unmark_all(Evas_Object *o);

/**
 * Set if should highlight matches marked with ewk_view_text_matches_mark().
 *
 * @param o view object where to set if matches are highlighted or not.
 * @param highlight if @c EINA_TRUE, matches will be highlighted.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE for failure.
 */
EAPI Eina_Bool    ewk_view_text_matches_highlight_set(Evas_Object *o, Eina_Bool highlight);

EAPI Eina_Bool    ewk_view_text_matches_highlight_get(const Evas_Object *o);

/**
 * Get current load progress estimate from 0.0 to 1.0.
 *
 * @param o view object to get current progress.
 *
 * @return progres value from 0.0 to 1.0 or -1.0 on error.
 */
EAPI double       ewk_view_load_progress_get(const Evas_Object *o);

/**
 * Ask main frame to stop loading.
 *
 * @param o view object to stop loading.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 *
 * @see ewk_frame_stop()
 */
EAPI Eina_Bool    ewk_view_stop(Evas_Object *o);

/**
 * Ask main frame to reload current document.
 *
 * @param o view object to reload.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 *
 * @see ewk_frame_reload()
 */
EAPI Eina_Bool    ewk_view_reload(Evas_Object *o);

/**
 * Ask main frame to fully reload current document, using no caches.
 *
 * @param o view object to reload.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 *
 * @see ewk_frame_reload_full()
 */
EAPI Eina_Bool    ewk_view_reload_full(Evas_Object *o);

/**
 * Ask main frame to navigate back in history.
 *
 * @param o view object to navigate back.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 *
 * @see ewk_frame_back()
 */
EAPI Eina_Bool    ewk_view_back(Evas_Object *o);

/**
 * Ask main frame to navigate forward in history.
 *
 * @param o view object to navigate forward.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 *
 * @see ewk_frame_forward()
 */
EAPI Eina_Bool    ewk_view_forward(Evas_Object *o);

/**
 * Navigate back or forward in history.
 *
 * @param o view object to navigate.
 * @param steps if positive navigates that amount forwards, if negative
 *        does backwards.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 *
 * @see ewk_frame_navigate()
 */
EAPI Eina_Bool    ewk_view_navigate(Evas_Object *o, int steps);

/**
 * Check if it is possible to navigate backwards one item in history.
 *
 * @param o view object to check if backward navigation is possible.
 *
 * @return @c EINA_TRUE if possible, @c EINA_FALSE otherwise.
 *
 * @see ewk_view_navigate_possible()
 */
EAPI Eina_Bool    ewk_view_back_possible(Evas_Object *o);

/**
 * Check if it is possible to navigate forwards one item in history.
 *
 * @param o view object to check if forward navigation is possible.
 *
 * @return @c EINA_TRUE if possible, @c EINA_FALSE otherwise.
 *
 * @see ewk_view_navigate_possible()
 */
EAPI Eina_Bool    ewk_view_forward_possible(Evas_Object *o);

/**
 * Check if it is possible to navigate given @a steps in history.
 *
 * @param o view object to navigate.
 * @param steps if positive navigates that amount forwards, if negative
 *        does backwards.
 *
 * @return @c EINA_TRUE if possible, @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool    ewk_view_navigate_possible(Evas_Object *o, int steps);

/**
 * Check if navigation history (back-forward lists) is enabled.
 *
 * @param o view object to check if navigation history is enabled.
 *
 * @return @c EINA_TRUE if view keeps history, @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool    ewk_view_history_enable_get(const Evas_Object *o);

/**
 * Sets if navigation history (back-forward lists) is enabled.
 *
 * @param o view object to set if navigation history is enabled.
 * @param enable @c EINA_TRUE if we want to enable navigation history;
 * @c EINA_FALSE otherwise.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool    ewk_view_history_enable_set(Evas_Object *o, Eina_Bool enable);

/**
 * Gets the history (back-forward list) associated with this view.
 *
 * @param o view object to get navigation history from.
 *
 * @return returns the history instance handle associated with this
 *         view or @c 0 on errors (including when history is not
 *         enabled with ewk_view_history_enable_set()). This instance
 *         is unique for this view and thus multiple calls to this
 *         function with the same view as parameter returns the same
 *         handle. This handle is alive while view is alive, thus one
 *         might want to listen for EVAS_CALLBACK_DEL on given view
 *         (@a o) to know when to stop using returned handle.
 *
 * @see ewk_view_history_enable_set()
 */
EAPI Ewk_History *ewk_view_history_get(const Evas_Object *o);

/**
 * Get the current zoom level of main frame.
 *
 * @param o view object to query zoom level.
 *
 * @return current zoom level in use or -1.0 on error.
 */
EAPI float        ewk_view_zoom_get(const Evas_Object *o);

/**
 * Set the current zoom level of main frame.
 *
 * @param o view object to set zoom level.
 * @param zoom new level to use.
 * @param cx x of center coordinate
 * @param cy y of center coordinate
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool    ewk_view_zoom_set(Evas_Object *o, float zoom, Evas_Coord cx, Evas_Coord cy);

EAPI Eina_Bool    ewk_view_zoom_weak_smooth_scale_get(const Evas_Object *o);
EAPI void         ewk_view_zoom_weak_smooth_scale_set(Evas_Object *o, Eina_Bool smooth_scale);

/**
 * Set the current zoom level of backing store, centered at given point.
 *
 * Unlike ewk_view_zoom_set(), this call do not ask WebKit to render
 * at new size, but scale what is already rendered, being much faster
 * but worse quality.
 *
 * Often one should use ewk_view_zoom_animated_set(), it will call the
 * same machinery internally.
 *
 * @note this will set variables used by ewk_view_zoom_animated_set()
 *       so sub-classes will not reset internal state on their
 *       "calculate" phase. To unset those and enable sub-classes to
 *       reset their internal state, call
 *       ewk_view_zoom_animated_mark_stop(). Namely, this call will
 *       set ewk_view_zoom_animated_mark_start() to actual webkit zoom
 *       level, ewk_view_zoom_animated_mark_end() and
 *       ewk_view_zoom_animated_mark_current() to given zoom level.
 *
 * @param o view object to set weak zoom level.
 * @param zoom level to scale backing store.
 * @param cx horizontal center offset, relative to object (w/2 is middle).
 * @param cy vertical center offset, relative to object (h/2 is middle).
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool    ewk_view_zoom_weak_set(Evas_Object *o, float zoom, Evas_Coord cx, Evas_Coord cy);

/**
 * Mark internal zoom animation state to given zoom.
 *
 * This does not modify any actual zoom in WebKit or backing store,
 * just set the Ewk_View_Smart_Data->animated_zoom.zoom.start so
 * sub-classes will know they should not reset their internal state.
 *
 * @param o view object to change value.
 * @param zoom new start value.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 *
 * @see ewk_view_zoom_animated_set()
 * @see ewk_view_zoom_weak_set()
 * @see ewk_view_zoom_animated_mark_stop()
 * @see ewk_view_zoom_animated_mark_end()
 * @see ewk_view_zoom_animated_mark_current()
 */
EAPI Eina_Bool    ewk_view_zoom_animated_mark_start(Evas_Object *o, float zoom);

/**
 * Mark internal zoom animation state to given zoom.
 *
 * This does not modify any actual zoom in WebKit or backing store,
 * just set the Ewk_View_Smart_Data->animated_zoom.zoom.end so
 * sub-classes will know they should not reset their internal state.
 *
 * @param o view object to change value.
 * @param zoom new end value.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 *
 * @see ewk_view_zoom_animated_set()
 * @see ewk_view_zoom_weak_set()
 * @see ewk_view_zoom_animated_mark_stop()
 * @see ewk_view_zoom_animated_mark_start()
 * @see ewk_view_zoom_animated_mark_current()
 */
EAPI Eina_Bool    ewk_view_zoom_animated_mark_end(Evas_Object *o, float zoom);

/**
 * Mark internal zoom animation state to given zoom.
 *
 * This does not modify any actual zoom in WebKit or backing store,
 * just set the Ewk_View_Smart_Data->animated_zoom.zoom.current so
 * sub-classes will know they should not reset their internal state.
 *
 * @param o view object to change value.
 * @param zoom new current value.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 *
 * @see ewk_view_zoom_animated_set()
 * @see ewk_view_zoom_weak_set()
 * @see ewk_view_zoom_animated_mark_stop()
 * @see ewk_view_zoom_animated_mark_start()
 * @see ewk_view_zoom_animated_mark_end()
 */
EAPI Eina_Bool    ewk_view_zoom_animated_mark_current(Evas_Object *o, float zoom);

/**
 * Unmark internal zoom animation state.
 *
 * This zero all start, end and current values.
 *
 * @param o view object to mark as animated is stopped.
 *
 * @see ewk_view_zoom_animated_mark_start()
 * @see ewk_view_zoom_animated_mark_end()
 * @see ewk_view_zoom_animated_mark_current()
 * @see ewk_view_zoom_weak_set()
 */
EAPI Eina_Bool    ewk_view_zoom_animated_mark_stop(Evas_Object *o);

/**
 * Set the current zoom level while animating.
 *
 * If the view was already animating to another zoom, it will start
 * from current point to the next provided zoom (@a zoom parameter)
 * and duration (@a duration parameter).
 *
 * This is the recommended way to do transitions from one level to
 * another. However, one may wish to do those from outside, in that
 * case use ewk_view_zoom_weak_set() and later control intermediate
 * states with ewk_view_zoom_animated_mark_current(),
 * ewk_view_zoom_animated_mark_end() and
 * ewk_view_zoom_animated_mark_stop().
 *
 * @param o view object to animate.
 * @param zoom final zoom level to use.
 * @param duration time in seconds the animation should take.
 * @param cx offset inside object that defines zoom center. 0 is left side.
 * @param cy offset inside object that defines zoom center. 0 is top side.
 * @return @c EINA_TRUE if animation will be started, @c EINA_FALSE if not
 *            because zoom is too small/big.
 */
EAPI Eina_Bool    ewk_view_zoom_animated_set(Evas_Object *o, float zoom, float duration, Evas_Coord cx, Evas_Coord cy);

/**
 * Query if zoom level just applies to text and not other elements.
 *
 * @param o view to query setting.
 *
 * @return @c EINA_TRUE if just text are scaled, @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool    ewk_view_zoom_text_only_get(const Evas_Object *o);

/**
 * Set if zoom level just applies to text and not other elements.
 *
 * @param o view to change setting.
 * @param setting @c EINA_TRUE if zoom should just be applied to text.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool    ewk_view_zoom_text_only_set(Evas_Object *o, Eina_Bool setting);

/**
 * Hint engine to pre-render region.
 *
 * Engines and backing store might be able to pre-render regions in
 * order to speed up zooming or scrolling to that region. Not all
 * engines might implement that and they will return @c EINA_FALSE
 * in that case.
 *
 * The given region is a hint. Engines might do bigger or smaller area
 * that covers that region. Pre-render might not be immediate, it may
 * be postponed to a thread, operated cooperatively in the main loop
 * and may be even ignored or cancelled afterwards.
 *
 * Multiple requests might be queued by engines. One can clear/forget
 * about them with ewk_view_pre_render_cancel().
 *
 * @param o view to ask pre-render of given region.
 * @param x absolute coordinate (0=left) to pre-render at zoom.
 * @param y absolute coordinate (0=top) to pre-render at zoom.
 * @param w width to pre-render starting from @a x at zoom.
 * @param h height to pre-render starting from @a y at zoom.
 * @param zoom desired zoom.
 *
 * @return @c EINA_TRUE if request was accepted, @c EINA_FALSE
 *         otherwise (errors, pre-render not supported, etc).
 *
 * @see ewk_view_pre_render_cancel()
 */
EAPI Eina_Bool    ewk_view_pre_render_region(Evas_Object *o, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, float zoom);

/**
 * Hint engine to pre-render region, given n extra cols/rows
 *
 * This is an alternative method to ewk_view_pre_render_region(). It does not
 * make sense in all engines and therefore it might not be implemented at all.
 *
 * It's only useful if engine divide the area being rendered in smaller tiles,
 * forming a grid. Then, browser could call this function to pre-render @param n
 * rows/cols involving the current viewport.
 *
 * @param o view to ask pre-render on.
 * @param n number of cols/rows that must be part of the region pre-rendered
 *
 * @see ewk_view_pre_render_region()
 */
EAPI Eina_Bool    ewk_view_pre_render_relative_radius(Evas_Object *o, unsigned int n);

/**
 * Cancel (clear) previous pre-render requests.
 *
 * @param o view to clear pre-render requests.
 */
EAPI void         ewk_view_pre_render_cancel(Evas_Object *o);

/**
  * Enable processing of update requests.
  *
  * @param o view to enable rendering.
  *
  * @return @c EINA_TRUE if render was enabled, @c EINA_FALSE
            otherwise (errors, rendering suspension not supported).
  */
EAPI Eina_Bool    ewk_view_enable_render(const Evas_Object *o);

/**
  * Disable processing of update requests.
  *
  * @param o view to disable rendering.
  *
  * @return @c EINA_TRUE if render was disabled, @c EINA_FALSE
            otherwise (errors, rendering suspension not supported).
  */
EAPI Eina_Bool    ewk_view_disable_render(const Evas_Object *o);

/**
 * Get input method hints
 *
 * @param o View.
 *
 * @return input method hints
 */
EAPI unsigned int ewk_view_imh_get(Evas_Object *o);

/* settings */
EAPI const char  *ewk_view_setting_user_agent_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_user_agent_set(Evas_Object *o, const char *user_agent);

EAPI Eina_Bool    ewk_view_setting_auto_load_images_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_auto_load_images_set(Evas_Object *o, Eina_Bool automatic);

EAPI Eina_Bool    ewk_view_setting_auto_shrink_images_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_auto_shrink_images_set(Evas_Object *o, Eina_Bool automatic);

/**
 * Gets if view can be resized automatically.
 *
 * @param o view to check status
 *
 * @return EINA_TRUE if view can be resized, EINA_FALSE
 *         otherwise (errors, cannot be resized).
 */
EAPI Eina_Bool    ewk_view_setting_enable_auto_resize_window_get(const Evas_Object *o);

/**
 * Sets if view can be resized automatically.
 *
 * @param o View.
 * @param resizable @c EINA_TRUE if we want to resize automatically;
 * @c EINA_FALSE otherwise. It defaults to @c EINA_TRUE
 *
 * @return EINA_TRUE if auto_resize_window status set, EINA_FALSE
 *         otherwise (errors).
 */
EAPI Eina_Bool    ewk_view_setting_enable_auto_resize_window_set(Evas_Object *o, Eina_Bool resizable);
EAPI Eina_Bool    ewk_view_setting_enable_scripts_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_enable_scripts_set(Evas_Object *o, Eina_Bool enable);

EAPI Eina_Bool    ewk_view_setting_enable_plugins_get(const Evas_Object *o);
EAPI Eina_Bool    ewk_view_setting_enable_plugins_set(Evas_Object *o, Eina_Bool enable);

/**
 * Get status of frame flattening.
 *
 * @param o view to check status
 *
 * @return EINA_TRUE if flattening is enabled, EINA_FALSE
 *         otherwise (errors, flattening disabled).
 */
EAPI Eina_Bool    ewk_view_setting_enable_frame_flattening_get(const Evas_Object* o);

/**
 * Set frame flattening.
 *
 * @param o view to set flattening
 *
 * @return EINA_TRUE if flattening status set, EINA_FALSE
 *         otherwise (errors).
 */
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

/**
 * Get current encoding of this View.
 *
 * @param o View.
 *
 * @return A pointer to an eina_strinshare containing the current custom
 * encoding for View object @param o, or @c 0 if it's not set.
 */
EAPI const char  *ewk_view_setting_encoding_custom_get(const Evas_Object *o);

/**
 * Set encoding of this View and reload page.
 *
 * @param o View.
 * @param encoding The new encoding or @c 0 to restore the default encoding.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 */
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

/**
 * Gets if the spatial naviagtion is enabled.
 *
 * @param o view object to get spatial navigation setting.
 * @return @c EINA_TRUE if spatial navigation is enabled, @c EINA_FALSE if not or on errors.
 */
EAPI Eina_Bool    ewk_view_setting_spatial_navigation_get(Evas_Object *o);

/**
 * Sets the spatial navigation.
 *
 * @param o view object to set spatial navigation setting.
 * @return @c EINA_TRUE on success and @c EINA_FALSE on failure
 */
EAPI Eina_Bool    ewk_view_setting_spatial_navigation_set(Evas_Object *o, Eina_Bool enable);

/**
 * Gets if the local storage is enabled.
 *
 * @param o view object to get if local storage is enabled.
 * @return @c EINA_TRUE if local storage is enabled, @c EINA_FALSE if not or on errors.
 */
EAPI Eina_Bool    ewk_view_setting_local_storage_get(Evas_Object *o);

/**
 * Sets the local storage of HTML5.
 *
 * @param o view object to set if local storage is enabled.
 * @return @c EINA_TRUE on success and @c EINA_FALSE on failure
 */
EAPI Eina_Bool    ewk_view_setting_local_storage_set(Evas_Object *o, Eina_Bool enable);

/**
 * Gets the local storage database path.
 *
 * @param o view object to get the local storage database path.
 * @return the local storage database path.
 */
EAPI const char  *ewk_view_setting_local_storage_database_path_get(const Evas_Object *o);

/**
 * Sets the local storage database path.
 *
 * @param o view object to set the local storage database path.
 * @return @c EINA_TRUE on success and @c EINA_FALSE on failure
 */
EAPI Eina_Bool    ewk_view_setting_local_storage_database_path_set(Evas_Object *o, const char *path);

/**
 * Gets if the page cache is enabled.
 *
 * @param o view object to set if page cache is enabled.
 * @return @c EINA_TRUE if page cache is enabled, @c EINA_FALSE if not.
 */
EAPI Eina_Bool    ewk_view_setting_page_cache_get(Evas_Object *o);

/**
 * Sets the page cache.
 *
 * @param o view object to set if page cache is enabled.
 * @return @c EINA_TRUE on success and @c EINA_FALSE on failure
 */
EAPI Eina_Bool    ewk_view_setting_page_cache_set(Evas_Object *o, Eina_Bool enable);

/**
 * Gets if the encoding detector is enabled.
 *
 * @param o view object to get if encoding detector is enabled.
 * @return @c EINA_TRUE if encoding detector is enabled, @c EINA_FALSE if not or on errors.
 */
EAPI Eina_Bool    ewk_view_setting_encoding_detector_get(Evas_Object *o);

/**
 * Sets the encoding detector.
 *
 * @param o view object to set if encoding detector is enabled.
 * @return @c EINA_TRUE on success and @c EINA_FALSE on failure
 */
EAPI Eina_Bool    ewk_view_setting_encoding_detector_set(Evas_Object *o, Eina_Bool enable);

/**
 * Returns whether developer extensions are enabled for the given view.
 *
 * Currently, this is used to know whether the Web Inspector is enabled for a
 * given view.
 *
 * @param o view object to check.
 *
 * @return @c EINA_TRUE if developer extensions are enabled, @c EINA_FALSE
 *         otherwise.
 */
EAPI Eina_Bool    ewk_view_setting_enable_developer_extras_get(Evas_Object *o);

/**
 * Enables/disables developer extensions for the given view.
 *
 * This currently controls whether the Web Inspector should be enabled.
 *
 * @param o The view whose setting will be changed.
 * @param enable @c EINA_TRUE to enable developer extras, @c EINA_FALSE to
 *               disable.
 *
 * @return @c EINA_TRUE on success, @EINA_FALSE on failure.
 */
EAPI Eina_Bool    ewk_view_setting_enable_developer_extras_set(Evas_Object *o, Eina_Bool enable);

/* to be used by subclass implementations */
/**
 * Similar to evas_object_smart_data_get(), but does type checking.
 *
 * @param o view object to query internal data.
 * @return internal data or @c 0 on errors (ie: incorrect type of @a o).
 */
EAPI Ewk_View_Smart_Data *ewk_view_smart_data_get(const Evas_Object *o);

EAPI void ewk_view_scrolls_process(Ewk_View_Smart_Data *sd);

/**
 * Structure that keeps paint context.
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
typedef struct _Ewk_View_Paint_Context Ewk_View_Paint_Context;

/**
 * Create a new paint context using the view as source and cairo as output.
 *
 * @param priv private handle pointer of the view to use as paint source.
 * @param cr cairo context to use as paint destination. A new
 *        reference is taken, so it's safe to call cairo_destroy()
 *        after this function returns.
 *
 * @return newly allocated instance or @c 0 on errors.
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
EAPI Ewk_View_Paint_Context *ewk_view_paint_context_new(Ewk_View_Private_Data *priv, cairo_t *cr);

/**
 * Destroy previously created paint context.
 *
 * @param ctxt paint context to destroy. Must @b not be @c 0.
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
EAPI void ewk_view_paint_context_free(Ewk_View_Paint_Context *ctxt);

/**
 * Save (push to stack) paint context status.
 *
 * @param ctxt paint context to save. Must @b not be @c 0.
 *
 * @see ewk_view_paint_context_restore()
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
EAPI void ewk_view_paint_context_save(Ewk_View_Paint_Context *ctxt);

/**
 * Restore (pop from stack) paint context status.
 *
 * @param ctxt paint context to restore. Must @b not be @c 0.
 *
 * @see ewk_view_paint_context_save()
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
EAPI void ewk_view_paint_context_restore(Ewk_View_Paint_Context *ctxt);

/**
 * Clip paint context drawings to given area.
 *
 * @param ctxt paint context to clip. Must @b not be @c 0.
 * @param area clip area to use.
 *
 * @see ewk_view_paint_context_save()
 * @see ewk_view_paint_context_restore()
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
EAPI void ewk_view_paint_context_clip(Ewk_View_Paint_Context *ctxt, const Eina_Rectangle *area);

/**
 * Paint using context using given area.
 *
 * @param ctxt paint context to paint. Must @b not be @c 0.
 * @param area paint area to use. Coordinates are relative to current viewport,
 *        thus "scrolled".
 *
 * @note one may use cairo functions on the cairo context to
 *       translate, scale or any modification that may fit his desires.
 *
 * @see ewk_view_paint_context_clip()
 * @see ewk_view_paint_context_paint_contents()
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
EAPI void ewk_view_paint_context_paint(Ewk_View_Paint_Context *ctxt, const Eina_Rectangle *area);

/**
 * Paint just contents using context using given area.
 *
 * Unlike ewk_view_paint_context_paint(), this function paint just
 * bare contents and ignores any scrolling, scrollbars and extras. It
 * will walk the rendering tree and paint contents inside the given
 * area to the cairo context specified in @a ctxt.
 *
 * @param ctxt paint context to paint. Must @b not be @c 0.
 * @param area paint area to use. Coordinates are absolute to page.
 *
 * @note one may use cairo functions on the cairo context to
 *       translate, scale or any modification that may fit his desires.
 *
 * @see ewk_view_paint_context_clip()
 * @see ewk_view_paint_context_paint()
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
EAPI void ewk_view_paint_context_paint_contents(Ewk_View_Paint_Context *ctxt, const Eina_Rectangle *area);

/**
 * Scale the contents by the given factors.
 *
 * This function applies a scaling transformation using Cairo.
 *
 * @param ctxt    paint context to paint. Must @b not be @c 0.
 * @param scale_x scale factor for the X dimension.
 * @param scale_y scale factor for the Y dimension.
 */
EAPI void ewk_view_paint_context_scale(Ewk_View_Paint_Context *ctxt, float scale_x, float scale_y);

/**
 * Performs a translation of the origin coordinates.
 *
 * This function moves the origin coordinates by @p x and @p y pixels.
 *
 * @param ctxt paint context to paint. Must @b not be @c 0.
 * @param x    amount of pixels to translate in the X dimension.
 * @param y    amount of pixels to translate in the Y dimension.
 */
EAPI void ewk_view_paint_context_translate(Ewk_View_Paint_Context *ctxt, float x, float y);

/**
 * Paint using given graphics context the given area.
 *
 * This uses viewport relative area and will also handle scrollbars
 * and other extra elements. See ewk_view_paint_contents() for the
 * alternative function.
 *
 * @param priv private handle pointer of view to use as paint source.
 * @param cr cairo context to use as paint destination. Its state will
 *        be saved before operation and restored afterwards.
 * @param area viewport relative geometry to paint.
 *
 * @return @c EINA_TRUE on success and @c EINA_FALSE on failure, like
 *         incorrect parameters.
 *
 * @note this is an easy to use version, but internal structures are
 *       always created, then graphics context is clipped, then
 *       painted, restored and destroyed. This might not be optimum,
 *       so using #Ewk_View_Paint_Context may be a better solutions
 *       for large number of operations.
 *
 * @see ewk_view_paint_contents()
 * @see ewk_view_paint_context_paint()
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
EAPI Eina_Bool ewk_view_paint(Ewk_View_Private_Data *priv, cairo_t *cr, const Eina_Rectangle *area);

/**
 * Paint just contents using given graphics context the given area.
 *
 * This uses absolute coordinates for area and will just handle
 * contents, no scrollbars or extras. See ewk_view_paint() for the
 * alternative solution.
 *
 * @param priv private handle pointer of view to use as paint source.
 * @param cr cairo context to use as paint destination. Its state will
 *        be saved before operation and restored afterwards.
 * @param area absolute geometry to paint.
 *
 * @return @c EINA_TRUE on success and @c EINA_FALSE on failure, like
 *         incorrect parameters.
 *
 * @note this is an easy to use version, but internal structures are
 *       always created, then graphics context is clipped, then
 *       painted, restored and destroyed. This might not be optimum,
 *       so using #Ewk_View_Paint_Context may be a better solutions
 *       for large number of operations.
 *
 * @see ewk_view_paint()
 * @see ewk_view_paint_context_paint_contents()
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
EAPI Eina_Bool ewk_view_paint_contents(Ewk_View_Private_Data *priv, cairo_t *cr, const Eina_Rectangle *area);

/**
 * Gets attributes of viewport meta tag.
 *
 * @param o view.
 * @param w width.
 * @param h height.
 * @param init_scale initial Scale value.
 * @param max_scale maximum Scale value.
 * @param min_scale minimum Scale value.
 * @param device_pixel_ratio value.
 * @param user_scalable user Scalable value.
 */
EAPI void ewk_view_viewport_attributes_get(Evas_Object *o, float *w, float *h, float *init_scale, float *max_scale, float *min_scale, float *device_pixel_ratio , Eina_Bool *user_scalable);

/**
 * Sets the zoom range.
 *
 * @param o view.
 * @param min_scale minimum value of zoom range.
 * @param max_scale maximum value of zoom range.
 *
 * @return @c EINA_TRUE if zoom range is changed, @c EINA_FALSE if not or failure.
 */
EAPI Eina_Bool ewk_view_zoom_range_set(Evas_Object *o, float min_scale, float max_scale);

/**
 * Gets the minimum value of zoom range.
 *
 * @param o view.
 *
 * @return minimum value of zoom range, or @c -1.0 on failure.
 */
EAPI float ewk_view_zoom_range_min_get(Evas_Object *o);

/**
 * Gets the maximum value of zoom range.
 *
 * @param o view.
 *
 * @return maximum value of zoom range, or @c -1.0 on failure.
 */
EAPI float ewk_view_zoom_range_max_get(Evas_Object *o);

/**
 * Sets if zoom is enabled.
 *
 * @param o view.
 * @param user_scalable boolean pointer in which to enable zoom. It defaults
 * to @c EINA_TRUE.
 *
 * @return @c EINA_TRUE on success, or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_view_user_scalable_set(Evas_Object *o, Eina_Bool user_scalable);

/**
 * Gets if zoom is enabled.
 *
 * @param o view.
 * @param user_scalable where to return the current user scalable value.
 *
 * @return @c EINA_TRUE if zoom is enabled, @c EINA_FALSE if not or on failure.
 */
EAPI Eina_Bool ewk_view_user_scalable_get(Evas_Object *o);

/**
 * Gets device pixel ratio value.
 *
 * @param o view.
 * @param user_scalable where to return the current user scalable value.
 *
 * @return device pixel ratio or @c -1.0 on failure.
 */
EAPI float ewk_view_device_pixel_ratio_get(Evas_Object *o);

/**
 * Sets view mode. The view-mode media feature describes the mode in which the
 * Web application is being shown as a running application.
 *
 * @param o view object to change view mode.
 * @param view_mode page view mode to be set
 *
 * @return @c EINA_TRUE if view mode is set as view_mode, @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool ewk_view_mode_set(Evas_Object *o, Ewk_View_Mode view_mode);

/**
 * Gets view mode. The view-mode media feature describes the mode in which the
 * Web application is being shown as a running application.
 *
 * @param o view object to query view mode.
 *
 * @return enum value of Ewk_View_Mode that indicates current view mode.
 */
EAPI Ewk_View_Mode ewk_view_mode_get(Evas_Object *o);
#ifdef __cplusplus
}
#endif
#endif // ewk_view_h
