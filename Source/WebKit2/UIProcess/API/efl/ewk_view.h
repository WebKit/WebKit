/*
   Copyright (C) 2011 Samsung Electronics
   Copyright (C) 2012 Intel Corporation. All rights reserved.

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
 * This object provides view related APIs of WebKit2 to EFL object.
 *
 * The following signals (see evas_object_smart_callback_add()) are emitted:
 *
 * - "close,window", void: window is closed.
 * - "create,window", Evas_Object**: a new window is created.
 * - "download,cancelled", Ewk_Download_Job*: reports that a download was effectively cancelled.
 * - "download,failed", Ewk_Download_Job_Error*: reports that a download failed with the given error.
 * - "download,finished", Ewk_Download_Job*: reports that a download completed successfully.
 * - "download,request", Ewk_Download_Job*: reports that a new download has been requested. The client should set the
 *   destination path by calling ewk_download_job_destination_set() or the download will fail.
 * - "form,submission,request", Ewk_Form_Submission_Request*: Reports that a form request is about to be submitted.
 *   The Ewk_Form_Submission_Request passed contains information about the text fields of the form. This
 *   is typically used to store login information that can be used later to pre-fill the form.
 *   The form will not be submitted until ewk_form_submission_request_submit() is called.
 *   It is possible to handle the form submission request asynchronously, by simply calling
 *   ewk_form_submission_request_ref() on the request and calling ewk_form_submission_request_submit()
 *   when done to continue with the form submission. If the last reference is removed on a
 *   #Ewk_Form_Submission_Request and the form has not been submitted yet,
 *   ewk_form_submission_request_submit() will be called automatically.
 * - "intent,request,new", Ewk_Intent*: reports new Web intent request.
 * - "intent,service,register", Ewk_Intent_Service*: reports new Web intent service registration.
 * - "load,error", const Ewk_Web_Error*: reports main frame load failed.
 * - "load,finished", void: reports load finished.
 * - "load,progress", double*: load progress has changed (value from 0.0 to 1.0).
 * - "load,provisional,failed", const Ewk_Web_Error*: view provisional load failed.
 * - "load,provisional,redirect", void: view received redirect for provisional load.
 * - "load,provisional,started", void: view started provisional load.
 * - "policy,decision,navigation", Ewk_Navigation_Policy_Decision*: a navigation policy decision should be taken.
 *   To make a policy decision asynchronously, simply increment the reference count of the
 *   #Ewk_Navigation_Policy_Decision object using ewk_navigation_policy_decision_ref().
 * - "policy,decision,new,window", Ewk_Navigation_Policy_Decision*: a new window policy decision should be taken.
 *   To make a policy decision asynchronously, simply increment the reference count of the
 *   #Ewk_Navigation_Policy_Decision object using ewk_navigation_policy_decision_ref().
 * - "resource,request,failed", const Ewk_Web_Resource_Load_Error*: a resource failed loading.
 * - "resource,request,finished", const Ewk_Web_Resource*: a resource finished loading.
 * - "resource,request,new", const Ewk_Web_Resource_Request*: a resource request was initiated.
 * - "resource,request,response", Ewk_Web_Resource_Load_Response*: a response to a resource request was received.
 * - "resource,request,sent", const Ewk_Web_Resource_Request*: a resource request was sent.
 * - "text,found", unsigned int*: the requested text was found and it gives the number of matches.
 * - "title,changed", const char*: title of the main frame was changed.
 * - "uri,changed", const char*: uri of the main frame was changed.
 */

#ifndef ewk_view_h
#define ewk_view_h

#include "ewk_back_forward_list.h"
#include "ewk_context.h"
#include "ewk_download_job.h"
#include "ewk_intent.h"
#include "ewk_url_request.h"
#include "ewk_url_response.h"
#include "ewk_web_error.h"
#include "ewk_web_resource.h"
#include <Evas.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _Ewk_View_Smart_Data Ewk_View_Smart_Data;
typedef struct _Ewk_View_Smart_Class Ewk_View_Smart_Class;

/// Ewk view's class, to be overridden by sub-classes.
struct _Ewk_View_Smart_Class {
    Evas_Smart_Class sc; /**< all but 'data' is free to be changed. */
    unsigned long version;

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
};

/**
 * The version you have to put into the version field
 * in the @a Ewk_View_Smart_Class structure.
 */
#define EWK_VIEW_SMART_CLASS_VERSION 1UL

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
#define EWK_VIEW_SMART_CLASS_INIT(smart_class_init) {smart_class_init, EWK_VIEW_SMART_CLASS_VERSION, 0, 0, 0, 0, 0, 0, 0, 0}

/**
 * Initializer to zero a whole Ewk_View_Smart_Class structure.
 *
 * @see EWK_VIEW_SMART_CLASS_INIT_VERSION
 * @see EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION
 * @see EWK_VIEW_SMART_CLASS_INIT
 */
#define EWK_VIEW_SMART_CLASS_INIT_NULL EWK_VIEW_SMART_CLASS_INIT(EVAS_SMART_CLASS_INIT_NULL)

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

typedef struct _Ewk_View_Private_Data Ewk_View_Private_Data;
/**
 * @brief Contains an internal View data.
 *
 * It is to be considered private by users, but may be extended or
 * changed by sub-classes (that's why it's in public header file).
 */
struct _Ewk_View_Smart_Data {
    Evas_Object_Smart_Clipped_Data base;
    const Ewk_View_Smart_Class* api; /**< reference to casted class instance */
    Evas_Object* self; /**< reference to owner object */
    Evas_Object* image; /**< reference to evas_object_image for drawing web contents */
    Ewk_View_Private_Data* priv; /**< should never be accessed, c++ stuff */
    struct {
        Evas_Coord x, y, w, h; /**< last used viewport */
    } view;
    struct { /**< what changed since last smart_calculate */
        Eina_Bool any:1;
        Eina_Bool size:1;
        Eina_Bool position:1;
    } changed;
};

/// Creates a type name for _Ewk_Web_Resource_Request.
typedef struct _Ewk_Web_Resource_Request Ewk_Web_Resource_Request;

/**
 * @brief Structure containing details about a resource request.
 */
struct _Ewk_Web_Resource_Request {
    Ewk_Web_Resource *resource; /**< resource being requested */
    Ewk_Url_Request *request; /**< URL request for the resource */
    Ewk_Url_Response *redirect_response; /**< Possible redirect response for the resource or @c NULL */
};

/// Creates a type name for _Ewk_Web_Resource_Load_Response.
typedef struct _Ewk_Web_Resource_Load_Response Ewk_Web_Resource_Load_Response;

/**
 * @brief Structure containing details about a response to a resource request.
 */
struct _Ewk_Web_Resource_Load_Response {
     Ewk_Web_Resource *resource; /**< resource requested */
     Ewk_Url_Response *response; /**< resource load response */
};

/// Creates a type name for _Ewk_Web_Resource_Load_Error.
typedef struct _Ewk_Web_Resource_Load_Error Ewk_Web_Resource_Load_Error;

/**
 * @brief Structure containing details about a resource load error.
 *
 * Details given about a resource load failure.
 */
struct _Ewk_Web_Resource_Load_Error {
    Ewk_Web_Resource *resource; /**< resource that failed loading */
    Ewk_Web_Error *error; /**< load error */
};

/// Creates a type name for _Ewk_Download_Job_Error.
typedef struct _Ewk_Download_Job_Error Ewk_Download_Job_Error;

/**
 * @brief Structure containing details about a download failure.
 */
struct _Ewk_Download_Job_Error {
    Ewk_Download_Job *download_job; /**< download that failed */
    Ewk_Web_Error *error; /**< download error */
};

/**
 * Enum values used to specify search options.
 * @brief   Provides option to find text
 * @info    Keep this in sync with WKFindOptions.h
 */
enum _Ewk_Find_Options {
    EWK_FIND_OPTIONS_NONE, /**< no search flags, this means a case sensitive, no wrap, forward only search. */
    EWK_FIND_OPTIONS_CASE_INSENSITIVE = 1 << 0, /**< case insensitive search. */
    EWK_FIND_OPTIONS_AT_WORD_STARTS = 1 << 1, /**< search text only at the beginning of the words. */
    EWK_FIND_OPTIONS_TREAT_MEDIAL_CAPITAL_AS_WORD_START = 1 << 2, /**< treat capital letters in the middle of words as word start. */
    EWK_FIND_OPTIONS_BACKWARDS = 1 << 3, /**< search backwards. */
    EWK_FIND_OPTIONS_WRAP_AROUND = 1 << 4, /**< if not present search will stop at the end of the document. */
    EWK_FIND_OPTIONS_SHOW_OVERLAY = 1 << 5, /**< show overlay */
    EWK_FIND_OPTIONS_SHOW_FIND_INDICATOR = 1 << 6, /**< show indicator */
    EWK_FIND_OPTIONS_SHOW_HIGHLIGHT = 1 << 7 /**< show highlight */
};
typedef enum _Ewk_Find_Options Ewk_Find_Options;

/**
 * Sets the smart class APIs, enabling view to be inherited.
 *
 * @param api class definition to set, all members with the
 *        exception of @a Evas_Smart_Class->data may be overridden, must
 *        @b not be @c NULL
 *
 * @note @a Evas_Smart_Class->data is used to implement type checking and
 *       is not supposed to be changed/overridden. If you need extra
 *       data for your smart class to work, just extend
 *       Ewk_View_Smart_Class instead.
 *       The Evas_Object which inherits the ewk_view should use
 *       ewk_view_smart_add() to create Evas_Object instead of
 *       evas_object_smart_add() because it performs additional initialization
 *       for the ewk_view.
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure (probably
 *         version mismatch)
 *
 * @see ewk_view_smart_add()
 */
EAPI Eina_Bool ewk_view_smart_class_set(Ewk_View_Smart_Class *api);

/**
 * Creates a new EFL WebKit view object with Evas_Smart and Ewk_Context.
 *
 * @note The Evas_Object which inherits the ewk_view should create its
 *       Evas_Object using this API instead of evas_object_smart_add()
 *       because the default initialization for ewk_view is done in this API.
 *
 * @param e canvas object where to create the view object
 * @param smart Evas_Smart object. Its type should be EWK_VIEW_TYPE_STR
 * @param context Ewk_Context object which is used for initializing
 *
 * @return view object on success or @c NULL on failure
 */
Evas_Object *ewk_view_smart_add(Evas *e, Evas_Smart *smart, Ewk_Context *context);

/**
 * Creates a new EFL WebKit view object.
 *
 * @param e canvas object where to create the view object
 *
 * @return view object on success or @c NULL on failure
 */
EAPI Evas_Object *ewk_view_add(Evas *e);

/**
 * Creates a new EFL WebKit view object based on specific Ewk_Context.
 *
 * @param e canvas object where to create the view object
 * @param context Ewk_Context object to declare process model
 *
 * @return view object on success or @c NULL on failure
 */
EAPI Evas_Object *ewk_view_add_with_context(Evas *e, Ewk_Context *context);

/**
 * Asks the object to load the given URI.
 *
 * @param o view object to load @a URI
 * @param uri uniform resource identifier to load
 *
 * @return @c EINA_TRUE is returned if @a o is valid, irrespective of load,
 *         or @c EINA_FALSE on failure
 */
EAPI Eina_Bool ewk_view_uri_set(Evas_Object *o, const char *uri);

/**
 * Returns the current URI string of view object.
 *
 * It returns an internal string and should not
 * be modified. The string is guaranteed to be stringshared.
 *
 * @param o view object to get current URI
 *
 * @return current URI on success or @c NULL on failure
 */
EAPI const char *ewk_view_uri_get(const Evas_Object *o);

/**
 * Asks the main frame to reload the current document.
 *
 * @param o view object to reload current document
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE otherwise
 *
 * @see ewk_view_reload_bypass_cache()
 */
EAPI Eina_Bool    ewk_view_reload(Evas_Object *o);

/**
 * Reloads the current page's document without cache.
 *
 * @param o view object to reload current document
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE otherwise
 */
EAPI Eina_Bool ewk_view_reload_bypass_cache(Evas_Object *o);

/**
 * Asks the main frame to stop loading.
 *
 * @param o view object to stop loading
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool    ewk_view_stop(Evas_Object *o);

/**
 * Delivers a Web intent to the view's main frame.
 *
 * @param o view object to deliver the intent to
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE otherwise.
 */
EAPI Eina_Bool    ewk_view_intent_deliver(Evas_Object *o, Ewk_Intent *intent);

/**
 * Asks the main frame to navigate back in the history.
 *
 * @param o view object to navigate back
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE otherwise
 *
 * @see ewk_frame_back()
 */
EAPI Eina_Bool    ewk_view_back(Evas_Object *o);

/**
 * Asks the main frame to navigate forward in the history.
 *
 * @param o view object to navigate forward
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE otherwise
 *
 * @see ewk_frame_forward()
 */
EAPI Eina_Bool    ewk_view_forward(Evas_Object *o);

/**
 * Queries if it is possible to navigate backwards one item in the history.
 *
 * @param o view object to query if backward navigation is possible
 *
 * @return @c EINA_TRUE if it is possible to navigate backwards in the history, @c EINA_FALSE otherwise
 */
EAPI Eina_Bool    ewk_view_back_possible(Evas_Object *o);

/**
 * Queries if it is possible to navigate forwards one item in the history.
 *
 * @param o view object to query if forward navigation is possible
 *
 * @return @c EINA_TRUE if it is possible to navigate forwards in the history, @c EINA_FALSE otherwise
 */
EAPI Eina_Bool    ewk_view_forward_possible(Evas_Object *o);

/**
 * Gets the back-forward list associated with this view.
 *
 * The returned instance is unique for this view and thus multiple calls
 * to this function with the same view as parameter returns the same
 * handle. This handle is alive while view is alive, thus one
 * might want to listen for EVAS_CALLBACK_DEL on given view
 * (@a o) to know when to stop using returned handle.
 *
 * @param o view object to get navigation back-forward list
 *
 * @return the back-forward list instance handle associated with this view
 */
EAPI Ewk_Back_Forward_List *ewk_view_back_forward_list_get(const Evas_Object *o);

/**
 * Gets the current title of the main frame.
 *
 * It returns an internal string and should not
 * be modified. The string is guaranteed to be stringshared.
 *
 * @param o view object to get current title
 *
 * @return current title on success or @c NULL on failure
 */
EAPI const char *ewk_view_title_get(const Evas_Object *o);

/**
 * Gets the current load progress of page.
 *
 * The progress estimation from 0.0 to 1.0.
 *
 * @param o view object to get the current progress
 *
 * @return the load progress of page, value from 0.0 to 1.0,
 *         or @c -1.0 on failure
 */
EAPI double ewk_view_load_progress_get(const Evas_Object *o);

/**
 * Loads the specified @a html string as the content of the view.
 *
 * External objects such as stylesheets or images referenced in the HTML
 * document are located relative to @a baseUrl.
 *
 * If an @a unreachableUrl is passed it is used as the url for the loaded
 * content. This is typically used to display error pages for a failed
 * load.
 *
 * @param o view object to load the HTML into
 * @param html HTML data to load
 * @param baseUrl Base URL used for relative paths to external objects (optional)
 * @param unreachableUrl URL that could not be reached (optional)
 *
 * @return @c EINA_TRUE if it the HTML was successfully loaded, @c EINA_FALSE otherwise
 */
EAPI Eina_Bool ewk_view_html_string_load(Evas_Object *o, const char *html, const char *baseUrl, const char *unreachableUrl);

/**
 * Scales the current page, centered at the given point.
 *
 * @param o view object to set the zoom level
 * @param scale_factor a new level to set
 * @param cx x of center coordinate
 * @param cy y of center coordinate
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE otherwise
 */
Eina_Bool ewk_view_scale_set(Evas_Object *o, double scaleFactor, int x, int y);

/**
 * Queries the current scale factor of the page.
 *
 * It returns previous scale factor after ewk_view_scale_set is called immediately
 * until scale factor of page is really changed.
 *
 * @param o view object to get the scale factor
 *
 * @return current scale factor in use on success or @c -1.0 on failure
 */
double ewk_view_scale_get(const Evas_Object *o);

/**
 * Queries the ratio between the CSS units and device pixels when the content is unscaled.
 *
 * When designing touch-friendly contents, knowing the approximated target size on a device
 * is important for contents providers in order to get the intented layout and element
 * sizes.
 *
 * As most first generation touch devices had a PPI of approximately 160, this became a
 * de-facto value, when used in conjunction with the viewport meta tag.
 *
 * Devices with a higher PPI learning towards 240 or 320, applies a pre-scaling on all
 * content, of either 1.5 or 2.0, not affecting the CSS scale or pinch zooming.
 *
 * This value can be set using this property and it is exposed to CSS media queries using
 * the -webkit-device-pixel-ratio query.
 *
 * For instance, if you want to load an image without having it upscaled on a web view
 * using a device pixel ratio of 2.0 it can be done by loading an image of say 100x100
 * pixels but showing it at half the size.
 *
 * @media (-webkit-min-device-pixel-ratio: 1.5) {
 *     .icon {
 *         width: 50px;
 *         height: 50px;
 *         url: "/images/icon@2x.png"; // This is actually a 100x100 image
 *     }
 * }
 *
 * If the above is used on a device with device pixel ratio of 1.5, it will be scaled
 * down but still provide a better looking image.
 *
 * @param o view object to get device pixel ratio
 *
 * @return the ratio between the CSS units and device pixels,
 *         or @c -1.0 on failure
 */
EAPI float ewk_view_device_pixel_ratio_get(const Evas_Object *o);

/**
 * Sets the ratio between the CSS units and device pixels when the content is unscaled.
 *
 * @param o view object to set device pixel ratio
 *
 * @return @c EINA_TRUE if the device pixel ratio was set, @c EINA_FALSE otherwise
 *
 * @see ewk_view_device_pixel_ratio_get()
 */
EAPI Eina_Bool ewk_view_device_pixel_ratio_set(Evas_Object *o, float ratio);

/**
 * Sets the theme path that will be used by this view.
 *
 * This also sets the theme on the main frame. As frames inherit theme
 * from their parent, this will have all frames with unset theme to
 * use this one.
 *
 * @param o view object to change theme
 * @param path theme path, may be @c NULL to reset to the default theme
 */
EAPI void ewk_view_theme_set(Evas_Object *o, const char *path);

/**
 * Gets the theme set on this view.
 *
 * This returns the value set by ewk_view_theme_set().
 *
 * @param o view object to get theme path
 *
 * @return the theme path, may be @c NULL if not set
 */
EAPI const char *ewk_view_theme_get(const Evas_Object *o);

/**
 * Gets the current custom character encoding name.
 *
 * @param o view object to get the current encoding
 *
 * @return @c eina_strinshare containing the current encoding, or
 *         @c NULL if it's not set
 */
EAPI const char  *ewk_view_setting_encoding_custom_get(const Evas_Object *o);

/**
 * Sets the custom character encoding and reloads the page.
 *
 * @param o view to set the encoding
 * @param encoding the new encoding to set or @c NULL to restore the default one
 *
 * @return @c EINA_TRUE on success @c EINA_FALSE otherwise
 */
EAPI Eina_Bool    ewk_view_setting_encoding_custom_set(Evas_Object *o, const char *encoding);

/**
* Searches the given string in the document.
*
* @param o view object to find text
* @param text text to find
* @param options options to find
* @param max count to find, unlimited if 0
*
* @return @c EINA_TRUE on success, @c EINA_FALSE on errors
*/
EAPI Eina_Bool ewk_view_text_find(Evas_Object *o, const char *text, Ewk_Find_Options options, unsigned int max_match_count);

/**
* Clears the highlight of searched text.
*
* @param o view object to find text
*
* @return @c EINA_TRUE on success, @c EINA_FALSE on errors
*/
EAPI Eina_Bool ewk_view_text_find_highlight_clear(Evas_Object *o);

#ifdef __cplusplus
}
#endif
#endif // ewk_view_h
