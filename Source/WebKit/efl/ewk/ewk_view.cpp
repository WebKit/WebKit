/*
    Copyright (C) 2009-2010 ProFUSION embedded systems
    Copyright (C) 2009-2012 Samsung Electronics
    Copyright (C) 2012 Intel Corporation
    Copyright (C) 2013 Apple Inc. All rights reserved.

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

#define __STDC_FORMAT_MACROS
#include "config.h"
#include "ewk_view.h"

#include "AcceleratedCompositingContextEfl.h"
#include "BackForwardController.h"
#include "BackForwardList.h"
#include "Bridge.h"
#include "Chrome.h"
#include "ChromeClientEfl.h"
#include "ContextMenuClientEfl.h"
#include "ContextMenuController.h"
#include "DocumentLoader.h"
#include "DragClientEfl.h"
#include "DumpRenderTreeSupportEfl.h"
#include "Editor.h"
#include "EditorClientEfl.h"
#include "EflScreenUtilities.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "FrameLoaderClientEfl.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "InspectorClientEfl.h"
#include "InspectorController.h"
#include "IntSize.h"
#include "JSDOMBinding.h"
#include "JSDOMWindow.h"
#include "JSLock.h"
#include "MainFrame.h"
#include "NetworkStorageSession.h"
#include "Operations.h"
#include "PageGroup.h"
#include "PlatformMouseEvent.h"
#include "PopupMenuClient.h"
#include "ProgressTracker.h"
#include "ProgressTrackerClientEfl.h"
#include "RefPtrCairo.h"
#include "RenderThemeEfl.h"
#include "ResourceHandle.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptController.h"
#include "SessionID.h"
#include "Settings.h"
#include "SoupNetworkSession.h"
#include "TiledBackingStore.h"
#include "c_instance.h"
#include "ewk_contextmenu_private.h"
#include "ewk_frame.h"
#include "ewk_frame_private.h"
#include "ewk_history_private.h"
#include "ewk_js_private.h"
#include "ewk_paint_context_private.h"
#include "ewk_private.h"
#include "ewk_settings_private.h"
#include "ewk_view_private.h"
#include "ewk_window_features_private.h"
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Edje.h>
#include <Eina.h>
#include <Evas.h>
#include <eina_safety_checks.h>
#include <inttypes.h>
#include <limits>
#include <math.h>
#include <sys/time.h>

#if ENABLE(DEVICE_ORIENTATION)
#include "DeviceMotionClientEfl.h"
#include "DeviceOrientationClientEfl.h"
#endif

#if ENABLE(GEOLOCATION)
#include "GeolocationClientMock.h"
#include "GeolocationController.h"
#endif

#if ENABLE(VIBRATION)
#include "VibrationClientEfl.h"
#endif

#if ENABLE(BATTERY_STATUS)
#include "BatteryClientEfl.h"
#endif

#if ENABLE(NETWORK_INFO)
#include "NetworkInfoClientEfl.h"
#endif

#if ENABLE(INPUT_TYPE_COLOR)
#include "ColorChooserClient.h"
#endif

#if ENABLE(NAVIGATOR_CONTENT_UTILS)
#include "NavigatorContentUtilsClientEfl.h"
#endif

static const float zoomMinimum = 0.05;
static const float zoomMaximum = 4.0;

static const char ewkViewTypeString[] = "EWK_View";

static const size_t ewkViewRepaintsSizeInitial = 32;
static const size_t ewkViewRepaintsSizeStep = 8;
static const size_t ewkViewRepaintsSizeMaximumFree = 64;

static const Evas_Smart_Cb_Description _ewk_view_callback_names[] = {
    { "colorchooser,create", "(yyyy)" },
    { "colorchooser,willdelete", "" },
    { "colorchooser,color,changed", "(yyyy)" },
    { "download,request", "p" },
    { "editorclient,contents,changed", "" },
    { "editorclient,selection,changed", "" },
    { "frame,created", "p" },
    { "icon,received", "" },
    { "inputmethod,changed", "b" },
    { "js,windowobject,clear", "" },
    { "link,hover,in", "p" },
    { "link,hover,out", "" },
    { "load,document,finished", "p" },
    { "load,error", "p" },
    { "load,finished", "p" },
    { "load,newwindow,show", "" },
    { "load,progress", "d" },
    { "load,provisional", "" },
    { "load,started", "" },
    { "menubar,visible,get", "b" },
    { "menubar,visible,set", "b" },
    { "popup,created", "p" },
    { "popup,willdelete", "p" },
    { "ready", "" },
    { "scrollbars,visible,get", "b" },
    { "scrollbars,visible,set", "b" },
    { "statusbar,text,set", "s" },
    { "statusbar,visible,get", "b" },
    { "statusbar,visible,set", "b" },
    { "title,changed", "s" },
    { "toolbars,visible,get", "b" },
    { "toolbars,visible,set", "b" },
    { "tooltip,text,set", "s" },
    { "tooltip,text,unset", "s" },
    { "uri,changed", "s" },
    { "view,resized", "" },
    { "zoom,animated,end", "" },
    { 0, 0 }
};

struct EditorCommand {
    Ewk_Editor_Command ewkEditorCommand;
    const char* editorCommandString;
};

/**
 * @brief A table grouping Ewk_Editor_Command enums with corresponding command
 * strings used by WebCore::EditorCommand, keeping both in sync.
 *
 * @internal
 */
static const EditorCommand editorCommands[] = {
    { EWK_EDITOR_COMMAND_UNDO, "Undo" },
    { EWK_EDITOR_COMMAND_REDO, "Redo" },
    { EWK_EDITOR_COMMAND_TOGGLE_BOLD, "ToggleBold" },
    { EWK_EDITOR_COMMAND_TOGGLE_ITALIC, "ToggleItalic" },
    { EWK_EDITOR_COMMAND_TOGGLE_UNDERLINE, "ToggleUnderline" },
    { EWK_EDITOR_COMMAND_TOGGLE_STRIKETHROUGH, "Strikethrough" },
    { EWK_EDITOR_COMMAND_TOGGLE_SUBSCRIPT, "SubScript" },
    { EWK_EDITOR_COMMAND_TOGGLE_SUPERSCRIPT, "SuperScript" },
    { EWK_EDITOR_COMMAND_INDENT, "Indent" },
    { EWK_EDITOR_COMMAND_OUTDENT, "Outdent" },
    { EWK_EDITOR_COMMAND_INSERT_ORDEREDLIST, "InsertOrderedList" },
    { EWK_EDITOR_COMMAND_INSERT_UNORDEREDLIST, "InsertUnorderedList" },
    { EWK_EDITOR_COMMAND_INSERT_IMAGE, "InsertImage" },
    { EWK_EDITOR_COMMAND_INSERT_TEXT, "InsertText" },
    { EWK_EDITOR_COMMAND_INSERT_HTML, "InsertHTML" },
    { EWK_EDITOR_COMMAND_INSERT_PARAGRAPH, "InsertParagraph" },
    { EWK_EDITOR_COMMAND_INSERT_PARAGRAPH_SEPARATOR, "InsertNewLine" },
    { EWK_EDITOR_COMMAND_INSERT_LINE_SEPARATOR, "InsertLineBreak" },
    { EWK_EDITOR_COMMAND_BACK_COLOR, "BackColor" },
    { EWK_EDITOR_COMMAND_FORE_COLOR, "ForeColor" },
    { EWK_EDITOR_COMMAND_HILITE_COLOR, "HiliteColor" },
    { EWK_EDITOR_COMMAND_FONT_SIZE, "FontSize" },
    { EWK_EDITOR_COMMAND_ALIGN_CENTER, "AlignCenter" },
    { EWK_EDITOR_COMMAND_ALIGN_JUSTIFIED, "AlignJustified" },
    { EWK_EDITOR_COMMAND_ALIGN_LEFT, "AlignLeft" },
    { EWK_EDITOR_COMMAND_ALIGN_RIGHT, "AlignRight" },
    { EWK_EDITOR_COMMAND_MOVE_TO_NEXT_CHAR, "MoveForward" },
    { EWK_EDITOR_COMMAND_MOVE_TO_PREVIOUS_CHAR, "MoveBackward" },
    { EWK_EDITOR_COMMAND_MOVE_TO_NEXT_WORD, "MoveWordForward" },
    { EWK_EDITOR_COMMAND_MOVE_TO_PREVIOUS_WORD, "MoveWordBackward" },
    { EWK_EDITOR_COMMAND_MOVE_TO_NEXT_LINE, "MoveDown" },
    { EWK_EDITOR_COMMAND_MOVE_TO_PREVIOUS_LINE, "MoveUp" },
    { EWK_EDITOR_COMMAND_MOVE_TO_BEGINNING_OF_LINE, "MoveToBeginningOfLine" },
    { EWK_EDITOR_COMMAND_MOVE_TO_END_OF_LINE, "MoveToEndOfLine" },
    { EWK_EDITOR_COMMAND_MOVE_TO_BEGINNING_OF_PARAGRAPH, "MoveToBeginningOfParagraph" },
    { EWK_EDITOR_COMMAND_MOVE_TO_END_OF_PARAGRAPH, "MoveToEndOfParagraph" },
    { EWK_EDITOR_COMMAND_MOVE_TO_BEGINNING_OF_DOCUMENT, "MoveToBeginningOfDocument" },
    { EWK_EDITOR_COMMAND_MOVE_TO_END_OF_DOCUMENT, "MoveToEndOfDocument" },
    { EWK_EDITOR_COMMAND_SELECT_NONE, "SelectNone" },
    { EWK_EDITOR_COMMAND_SELECT_ALL, "SelectAll" },
    { EWK_EDITOR_COMMAND_SELECT_PARAGRAPH, "SelectParagraph" },
    { EWK_EDITOR_COMMAND_SELECT_SENTENCE, "SelectSentence" },
    { EWK_EDITOR_COMMAND_SELECT_LINE, "SelectLine" },
    { EWK_EDITOR_COMMAND_SELECT_WORD, "SelectWord" },
    { EWK_EDITOR_COMMAND_SELECT_NEXT_CHAR, "MoveForwardAndModifySelection" },
    { EWK_EDITOR_COMMAND_SELECT_PREVIOUS_CHAR, "MoveBackwardAndModifySelection" },
    { EWK_EDITOR_COMMAND_SELECT_NEXT_WORD, "MoveWordForwardAndModifySelection" },
    { EWK_EDITOR_COMMAND_SELECT_PREVIOUS_WORD, "MoveWordBackwardAndModifySelection" },
    { EWK_EDITOR_COMMAND_SELECT_NEXT_LINE, "MoveDownAndModifySelection" },
    { EWK_EDITOR_COMMAND_SELECT_PREVIOUS_LINE, "MoveUpAndModifySelection" },
    { EWK_EDITOR_COMMAND_SELECT_START_OF_LINE, "MoveToBeginningOfLineAndModifySelection" },
    { EWK_EDITOR_COMMAND_SELECT_END_OF_LINE, "MoveToEndOfLineAndModifySelection" },
    { EWK_EDITOR_COMMAND_SELECT_START_OF_PARAGRAPH, "MoveToBeginningOfParagraphAndModifySelection" },
    { EWK_EDITOR_COMMAND_SELECT_END_OF_PARAGRAPH, "MoveToEndOfParagraphAndModifySelection" },
    { EWK_EDITOR_COMMAND_SELECT_START_OF_DOCUMENT, "MoveToBeginningOfDocumentAndModifySelection" },
    { EWK_EDITOR_COMMAND_SELECT_END_OF_DOCUMENT, "MoveToEndOfDocumentAndModifySelection" },
    { EWK_EDITOR_COMMAND_DELETE_WORD_BACKWARD, "DeleteWordBackward" },
    { EWK_EDITOR_COMMAND_DELETE_WORD_FORWARD, "DeleteWordForward" },
    { EWK_EDITOR_COMMAND_NONE, 0 } // EWK_EDITOR_COMMAND_NONE must be the last element.
};

/**
 * @brief Private data that is used internally by EFL WebKit
 * and should never be modified from outside.
 *
 * @internal
 */
struct _Ewk_View_Private_Data {
    OwnPtr<WebCore::Page> page;
    WebCore::ViewportArguments viewportArguments;
    Ewk_History* history;
    std::unique_ptr<WebCore::AcceleratedCompositingContext> acceleratedCompositingContext;
#if ENABLE(INPUT_TYPE_COLOR)
    WebCore::ColorChooserClient* colorChooserClient;
#endif
#if ENABLE(NAVIGATOR_CONTENT_UTILS) || ENABLE(CUSTOM_SCHEME_HANDLER)
    std::unique_ptr<WebCore::NavigatorContentUtilsClientEfl> navigatorContentUtilsClient;
#endif
    struct {
        Ewk_Menu menu;
        WebCore::PopupMenuClient* menuClient;
    } popup;
    struct {
        Eina_Rectangle* array;
        size_t count;
        size_t allocated;
    } repaints;
    unsigned int imh; /**< input method hints */
    struct {
        bool viewCleared : 1;
        bool needTouchEvents : 1;
        bool hasDisplayedMixedContent : 1;
        bool hasRunMixedContent : 1;
    } flags;
    struct {
        const char* userAgent;
        const char* userStylesheet;
        const char* encodingDefault;
        const char* encodingCustom;
        const char* theme;
        const char* localStorageDatabasePath;
        int fontMinimumSize;
        int fontMinimumLogicalSize;
        int fontDefaultSize;
        int fontMonospaceSize;
        const char* fontStandard;
        const char* fontCursive;
        const char* fontMonospace;
        const char* fontFantasy;
        const char* fontSerif;
        const char* fontSansSerif;
        bool autoLoadImages : 1;
        bool autoShrinkImages : 1;
        bool enableAutoResizeWindow : 1;
        bool enableDeveloperExtras : 1;
        bool enableScripts : 1;
        bool enablePlugins : 1;
        bool enableFrameFlattening : 1;
#if ENABLE(FULLSCREEN_API)
        bool enableFullscreen : 1;
#endif
        bool encodingDetector : 1;
        bool hyperlinkAuditingEnabled : 1;
        bool scriptsCanOpenWindows : 1;
        bool scriptsCanCloseWindows : 1;
#if ENABLE(VIDEO_TRACK)
        bool shouldDisplayCaptions : 1;
        bool shouldDisplaySubtitles : 1;
        bool shouldDisplayTextDescriptions: 1;
#endif
        bool scriptsCanAccessClipboard : 1;
        bool resizableTextareas : 1;
        bool privateBrowsing : 1;
        bool caretBrowsing : 1;
        bool spatialNavigation : 1;
        bool localStorage : 1;
        bool offlineAppCache : 1;
        bool pageCache : 1;
        bool enableXSSAuditor : 1;
        bool webGLEnabled : 1;
        bool tabsToLinks : 1;
        struct {
            float minScale;
            float maxScale;
            Eina_Bool userScalable : 1;
        } zoomRange;
        double domTimerInterval;
        bool allowUniversalAccessFromFileURLs : 1;
        bool allowFileAccessFromFileURLs : 1;
    } settings;
    struct {
        struct {
            double start;
            double end;
            double duration;
        } time;
        struct {
            float start;
            float end;
            float range;
        } zoom;
        struct {
            Evas_Coord x, y;
        } center;
        Ecore_Animator* animator;
    } animatedZoom;
    const char* cursorGroup;
    Evas_Object* cursorObject;
#if ENABLE(INSPECTOR)
    Evas_Object* inspectorView;
#endif
#ifdef HAVE_ECORE_X
    bool isUsingEcoreX;
#endif
#if ENABLE(CONTEXT_MENUS)
    Ewk_Context_Menu* contextMenu;
#endif
};

#ifndef EWK_TYPE_CHECK
#define EWK_VIEW_TYPE_CHECK(ewkView, ...) do { } while (0)
#else
#define EWK_VIEW_TYPE_CHECK(ewkView, ...) \
    do { \
        const char* _tmp_otype = evas_object_type_get(ewkView); \
        const Evas_Smart* _tmp_s = evas_object_smart_smart_get(ewkView); \
        if (EINA_UNLIKELY(!_tmp_s)) { \
            EINA_LOG_CRIT \
                ("%p (%s) is not a smart object!", ewkView, \
                _tmp_otype ? _tmp_otype : "(null)"); \
            return __VA_ARGS__; \
        } \
        const Evas_Smart_Class* _tmp_sc = evas_smart_class_get(_tmp_s); \
        if (EINA_UNLIKELY(!_tmp_sc)) { \
            EINA_LOG_CRIT \
                ("%p (%s) is not a smart object!", ewkView, \
                _tmp_otype ? _tmp_otype : "(null)"); \
            return __VA_ARGS__; \
        } \
        if (EINA_UNLIKELY(_tmp_sc->data != ewkViewTypeString)) { \
            EINA_LOG_CRIT \
                ("%p (%s) is not of an ewk_view (need %p, got %p)!", \
                ewkView, _tmp_otype ? _tmp_otype : "(null)", \
                ewkViewTypeString, _tmp_sc->data); \
            return __VA_ARGS__; \
        } \
    } while (0)
#endif

#define EWK_VIEW_SD_GET(ewkView, pointer) \
    Ewk_View_Smart_Data* pointer = static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(ewkView))

#define EWK_VIEW_SD_GET_OR_RETURN(ewkView, pointer, ...) \
    EWK_VIEW_TYPE_CHECK(ewkView, __VA_ARGS__); \
    EWK_VIEW_SD_GET(ewkView, pointer); \
    if (!pointer) { \
        CRITICAL("no smart data for object %p (%s)", \
                 ewkView, evas_object_type_get(ewkView)); \
        return __VA_ARGS__; \
    }

#define EWK_VIEW_PRIV_GET(smartData, pointer) \
    Ewk_View_Private_Data* pointer = smartData->_priv

#define EWK_VIEW_PRIV_GET_OR_RETURN(smartData, pointer, ...) \
    EWK_VIEW_PRIV_GET(smartData, pointer); \
    if (!pointer) { \
        CRITICAL("no private data for object %p (%s)", \
                 smartData->self, evas_object_type_get(smartData->self)); \
        return __VA_ARGS__; \
    }

static void _ewk_view_smart_changed(Ewk_View_Smart_Data* smartData)
{
    if (smartData->changed.any)
        return;
    smartData->changed.any = true;
    evas_object_smart_changed(smartData->self);
}

static Eina_Bool _ewk_view_repaints_resize(Ewk_View_Private_Data* priv, size_t size)
{
    void* tmp = realloc(priv->repaints.array, size * sizeof(Eina_Rectangle));
    if (!tmp) {
        CRITICAL("could not realloc repaints array to %zu elements.", size);
        return false;
    }
    priv->repaints.allocated = size;
    priv->repaints.array = static_cast<Eina_Rectangle*>(tmp);
    return true;
}

static void _ewk_view_repaint_add(Ewk_View_Private_Data* priv, Evas_Coord x, Evas_Coord y, Evas_Coord width, Evas_Coord height)
{
    size_t newSize = 0;

    if (priv->repaints.allocated == priv->repaints.count)
        newSize = priv->repaints.allocated + ewkViewRepaintsSizeStep;
    else if (!priv->repaints.count && priv->repaints.allocated > ewkViewRepaintsSizeInitial)
        newSize = ewkViewRepaintsSizeInitial;

    if (newSize) {
        if (!_ewk_view_repaints_resize(priv, newSize))
            return;
    }

    Eina_Rectangle* rect = priv->repaints.array + priv->repaints.count;
    priv->repaints.count++;

    rect->x = x;
    rect->y = y;
    rect->w = width;
    rect->h = height;

    DBG("add repaint %d, %d+%dx%d", x, y, width, height);
}

static void _ewk_view_repaints_flush(Ewk_View_Private_Data* priv)
{
    priv->repaints.count = 0;
    if (priv->repaints.allocated <= ewkViewRepaintsSizeMaximumFree)
        return;
    _ewk_view_repaints_resize(priv, ewkViewRepaintsSizeMaximumFree);
}

// Default Event Handling //////////////////////////////////////////////
static Eina_Bool _ewk_view_smart_focus_in(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_PRIV_GET(smartData, priv);
    WebCore::FocusController* focusController = &priv->page->focusController();
    DBG("ewkView=%p, focusController=%p", smartData->self, focusController);

    focusController->setActive(true);
    focusController->setFocused(true);
    return true;
}

static Eina_Bool _ewk_view_smart_focus_out(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_PRIV_GET(smartData, priv);
    WebCore::FocusController* focusController = &priv->page->focusController();
    DBG("ewkView=%p, fc=%p", smartData->self, focusController);

    focusController->setActive(false);
    focusController->setFocused(false);
    return true;
}

static Eina_Bool _ewk_view_smart_mouse_wheel(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Wheel* wheelEvent)
{
    return ewk_frame_feed_mouse_wheel(smartData->main_frame, wheelEvent);
}

static Eina_Bool _ewk_view_smart_mouse_down(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Down* downEvent)
{
    return ewk_frame_feed_mouse_down(smartData->main_frame, downEvent);
}

static Eina_Bool _ewk_view_smart_mouse_up(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Up* upEvent)
{
    return ewk_frame_feed_mouse_up(smartData->main_frame, upEvent);
}

static Eina_Bool _ewk_view_smart_mouse_move(Ewk_View_Smart_Data* smartData, const Evas_Event_Mouse_Move* moveEvent)
{
    return ewk_frame_feed_mouse_move(smartData->main_frame, moveEvent);
}

static Eina_Bool _ewk_view_smart_key_down(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Down* downEvent)
{
    Evas_Object* frame = ewk_view_frame_focused_get(smartData->self);

    if (!frame)
        frame = smartData->main_frame;

    return ewk_frame_feed_key_down(frame, downEvent);
}

static Eina_Bool _ewk_view_smart_key_up(Ewk_View_Smart_Data* smartData, const Evas_Event_Key_Up* upEvent)
{
    Evas_Object* frame = ewk_view_frame_focused_get(smartData->self);

    if (!frame)
        frame = smartData->main_frame;

    return ewk_frame_feed_key_up(frame, upEvent);
}

static void _ewk_view_smart_add_console_message(Ewk_View_Smart_Data*, const char* message, unsigned int lineNumber, const char* sourceID)
{
    INFO("console message: %s @%d: %s\n", sourceID, lineNumber, message);
}

static void _ewk_view_smart_run_javascript_alert(Ewk_View_Smart_Data*, Evas_Object* /*frame*/, const char* message)
{
    INFO("javascript alert: %s\n", message);
}

static Eina_Bool _ewk_view_smart_run_javascript_confirm(Ewk_View_Smart_Data*, Evas_Object* /*frame*/, const char* message)
{
    INFO("javascript confirm: %s", message);
    INFO("javascript confirm (HARD CODED)? YES");
    return true;
}

static Eina_Bool _ewk_view_smart_run_before_unload_confirm(Ewk_View_Smart_Data*, Evas_Object* /*frame*/, const char* message)
{
    INFO("before unload confirm: %s", message);
    return true;
}

static Eina_Bool _ewk_view_smart_should_interrupt_javascript(Ewk_View_Smart_Data*)
{
    INFO("should interrupt javascript?\n"
        "\t(HARD CODED) NO");
    return false;
}

static Eina_Bool _ewk_view_smart_run_javascript_prompt(Ewk_View_Smart_Data*, Evas_Object* /*frame*/, const char* message, const char* defaultValue, const char** value)
{
    *value = eina_stringshare_add("test");
    Eina_Bool result = true;
    INFO("javascript prompt:\n"
        "\t      message: %s\n"
        "\tdefault value: %s\n"
        "\tgiving answer: %s\n"
        "\t       button: %s", message, defaultValue, *value, result ? "ok" : "cancel");

    return result;
}

// Event Handling //////////////////////////////////////////////////////
static void _ewk_view_on_focus_in(void* data, Evas*, Evas_Object*, void*)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->focus_in);
    smartData->api->focus_in(smartData);
}

static void _ewk_view_on_focus_out(void* data, Evas*, Evas_Object*, void*)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->focus_out);
    smartData->api->focus_out(smartData);
}

static void _ewk_view_on_mouse_wheel(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Wheel* wheelEvent = static_cast<Evas_Event_Mouse_Wheel*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_wheel);
    smartData->api->mouse_wheel(smartData, wheelEvent);
}

static void _ewk_view_on_mouse_down(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Down* downEvent = static_cast<Evas_Event_Mouse_Down*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_down);
    smartData->api->mouse_down(smartData, downEvent);
}

static void _ewk_view_on_mouse_up(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Up* upEvent = static_cast<Evas_Event_Mouse_Up*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_up);
    smartData->api->mouse_up(smartData, upEvent);
}

static void _ewk_view_on_mouse_move(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Mouse_Move* moveEvent = static_cast<Evas_Event_Mouse_Move*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->mouse_move);
    smartData->api->mouse_move(smartData, moveEvent);
}

static void _ewk_view_on_key_down(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Key_Down* downEvent = static_cast<Evas_Event_Key_Down*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->key_down);
    smartData->api->key_down(smartData, downEvent);
}

static void _ewk_view_on_key_up(void* data, Evas*, Evas_Object*, void* eventInfo)
{
    Evas_Event_Key_Up* upEvent = static_cast<Evas_Event_Key_Up*>(eventInfo);
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->key_up);
    smartData->api->key_up(smartData, upEvent);
}

static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

static Ewk_View_Private_Data* _ewk_view_priv_new(Ewk_View_Smart_Data* smartData)
{
    Ewk_View_Private_Data* priv = new Ewk_View_Private_Data;
    memset(priv, 0, sizeof(Ewk_View_Private_Data));
    AtomicString string;
    WebCore::URL url;

    WebCore::Page::PageClients pageClients;
    pageClients.chromeClient = new WebCore::ChromeClientEfl(smartData->self);
#if ENABLE(CONTEXT_MENUS)
    pageClients.contextMenuClient = new WebCore::ContextMenuClientEfl;
#endif
    pageClients.editorClient = new WebCore::EditorClientEfl(smartData->self);
    pageClients.dragClient = new WebCore::DragClientEfl;
#if ENABLE(INSPECTOR)
    pageClients.inspectorClient = new WebCore::InspectorClientEfl(smartData->self);
#endif
    pageClients.loaderClientForMainFrame = new WebCore::FrameLoaderClientEfl(smartData->self);
    pageClients.progressTrackerClient = new WebCore::ProgressTrackerClientEfl(smartData->self);
    priv->page = adoptPtr(new WebCore::Page(pageClients));

#if ENABLE(DEVICE_ORIENTATION)
    WebCore::provideDeviceMotionTo(priv->page.get(), new WebCore::DeviceMotionClientEfl);
    WebCore::provideDeviceOrientationTo(priv->page.get(), new WebCore::DeviceOrientationClientEfl);
#endif

#if ENABLE(NETWORK_INFO)
    WebCore::provideNetworkInfoTo(priv->page.get(), new WebCore::NetworkInfoClientEfl);
#endif

#if ENABLE(VIBRATION)
    WebCore::provideVibrationTo(priv->page.get(), new WebCore::VibrationClientEfl(smartData->self));
#endif

#if ENABLE(BATTERY_STATUS)
    WebCore::provideBatteryTo(priv->page.get(), new BatteryClientEfl(smartData->self));
#endif

#if ENABLE(NAVIGATOR_CONTENT_UTILS)
    priv->navigatorContentUtilsClient = std::make_unique<WebCore::NavigatorContentUtilsClientEfl>(smartData->self);
    WebCore::provideNavigatorContentUtilsTo(priv->page.get(), priv->navigatorContentUtilsClient.get());
#endif

#if ENABLE(GEOLOCATION)
    if (DumpRenderTreeSupportEfl::dumpRenderTreeModeEnabled()) {
        WebCore::GeolocationClientMock* mock = new WebCore::GeolocationClientMock;
        WebCore::provideGeolocationTo(priv->page.get(), mock);
        mock->setController(WebCore::GeolocationController::from(priv->page.get()));
    }
#endif

    WebCore::Settings& pageSettings = priv->page->settings();

    WebCore::LayoutMilestones layoutMilestones = WebCore::DidFirstLayout | WebCore::DidFirstVisuallyNonEmptyLayout;
    priv->page->addLayoutMilestones(layoutMilestones);

    // FIXME: Noone is supposed to do this manually.
    priv->viewportArguments.width = WebCore::ViewportArguments::ValueAuto;
    priv->viewportArguments.height = WebCore::ViewportArguments::ValueAuto;
    priv->viewportArguments.zoom = WebCore::ViewportArguments::ValueAuto;
    priv->viewportArguments.minZoom = WebCore::ViewportArguments::ValueAuto;
    priv->viewportArguments.maxZoom = WebCore::ViewportArguments::ValueAuto;
    priv->viewportArguments.userZoom = true;

    pageSettings.setLoadsImagesAutomatically(true);
    pageSettings.setDefaultTextEncodingName("iso-8859-1");
    pageSettings.setDefaultFixedFontSize(12);
    pageSettings.setDefaultFontSize(16);
    pageSettings.setSerifFontFamily("serif");
    pageSettings.setFixedFontFamily("monotype");
    pageSettings.setSansSerifFontFamily("sans");
    pageSettings.setStandardFontFamily("sans");
    pageSettings.setHyperlinkAuditingEnabled(false);
    WebCore::RuntimeEnabledFeatures::sharedFeatures().setCSSRegionsEnabled(true);
    pageSettings.setScriptEnabled(true);
    pageSettings.setPluginsEnabled(true);
    pageSettings.setLocalStorageEnabled(true);
    pageSettings.setOfflineWebApplicationCacheEnabled(true);
    pageSettings.setUsesPageCache(true);
    pageSettings.setUsesEncodingDetector(false);
#if ENABLE(WEB_AUDIO)
    pageSettings.setWebAudioEnabled(false);
#endif
    pageSettings.setWebGLEnabled(true);
    pageSettings.setXSSAuditorEnabled(true);
#if ENABLE(FULLSCREEN_API)
    pageSettings.setFullScreenEnabled(true);
#endif
    pageSettings.setInteractiveFormValidationEnabled(true);
    pageSettings.setAcceleratedCompositingEnabled(true);
    pageSettings.setForceCompositingMode(true);
    char* debugVisualsEnvironment = getenv("WEBKIT_SHOW_COMPOSITING_DEBUG_VISUALS");
    bool showDebugVisuals = debugVisualsEnvironment && !strcmp(debugVisualsEnvironment, "1");
    pageSettings.setShowDebugBorders(showDebugVisuals);
    pageSettings.setShowRepaintCounter(showDebugVisuals);

    url = pageSettings.userStyleSheetLocation();
    priv->settings.userStylesheet = eina_stringshare_add(url.string().utf8().data());

    priv->settings.encodingDefault = eina_stringshare_add(pageSettings.defaultTextEncodingName().utf8().data());
    priv->settings.encodingCustom = 0;

    string = pageSettings.localStorageDatabasePath();
    priv->settings.localStorageDatabasePath = eina_stringshare_add(string.string().utf8().data());

    priv->settings.fontMinimumSize = pageSettings.minimumFontSize();
    priv->settings.fontMinimumLogicalSize = pageSettings.minimumLogicalFontSize();
    priv->settings.fontDefaultSize = pageSettings.defaultFontSize();
    priv->settings.fontMonospaceSize = pageSettings.defaultFixedFontSize();

    string = pageSettings.standardFontFamily();
    priv->settings.fontStandard = eina_stringshare_add(string.string().utf8().data());
    string = pageSettings.cursiveFontFamily();
    priv->settings.fontCursive = eina_stringshare_add(string.string().utf8().data());
    string = pageSettings.fixedFontFamily();
    priv->settings.fontMonospace = eina_stringshare_add(string.string().utf8().data());
    string = pageSettings.fantasyFontFamily();
    priv->settings.fontFantasy = eina_stringshare_add(string.string().utf8().data());
    string = pageSettings.serifFontFamily();
    priv->settings.fontSerif = eina_stringshare_add(string.string().utf8().data());
    string = pageSettings.sansSerifFontFamily();
    priv->settings.fontSansSerif = eina_stringshare_add(string.string().utf8().data());

    priv->settings.autoLoadImages = pageSettings.loadsImagesAutomatically();
    priv->settings.autoShrinkImages = pageSettings.shrinksStandaloneImagesToFit();
    priv->settings.enableAutoResizeWindow = true;
    priv->settings.enableDeveloperExtras = pageSettings.developerExtrasEnabled();
    priv->settings.enableScripts = pageSettings.isScriptEnabled();
    priv->settings.enablePlugins = pageSettings.arePluginsEnabled();
    priv->settings.enableFrameFlattening = pageSettings.frameFlatteningEnabled();
#if ENABLE(FULLSCREEN_API)
    priv->settings.enableFullscreen = pageSettings.fullScreenEnabled();
#endif
    priv->settings.enableXSSAuditor = pageSettings.xssAuditorEnabled();
    priv->settings.hyperlinkAuditingEnabled = pageSettings.hyperlinkAuditingEnabled();
    priv->settings.scriptsCanOpenWindows = pageSettings.javaScriptCanOpenWindowsAutomatically();
    priv->settings.scriptsCanCloseWindows = pageSettings.allowScriptsToCloseWindows();
#if ENABLE(VIDEO_TRACK)
    priv->settings.shouldDisplayCaptions = pageSettings.shouldDisplayCaptions();
    priv->settings.shouldDisplaySubtitles = pageSettings.shouldDisplaySubtitles();
    priv->settings.shouldDisplayTextDescriptions = pageSettings.shouldDisplayTextDescriptions();
#endif
    priv->settings.scriptsCanAccessClipboard = pageSettings.javaScriptCanAccessClipboard() && pageSettings.DOMPasteAllowed();
    priv->settings.resizableTextareas = pageSettings.textAreasAreResizable();
    priv->settings.privateBrowsing = priv->page->usesEphemeralSession();
    priv->settings.caretBrowsing = pageSettings.caretBrowsingEnabled();
    priv->settings.spatialNavigation = pageSettings.spatialNavigationEnabled();
    priv->settings.localStorage = pageSettings.localStorageEnabled();
    priv->settings.offlineAppCache = true; // XXX no function to read setting; this keeps the original setting
    priv->settings.pageCache = pageSettings.usesPageCache();
    priv->settings.encodingDetector = pageSettings.usesEncodingDetector();
    priv->settings.webGLEnabled = pageSettings.webGLEnabled();
    priv->settings.tabsToLinks = true;

    priv->settings.userAgent = ewk_settings_default_user_agent_get();

    // Since there's no scale separated from zooming in webkit-efl, this functionality of
    // viewport meta tag is implemented using zoom. When scale zoom is supported by webkit-efl,
    // this functionality will be modified by the scale zoom patch.
    priv->settings.zoomRange.minScale = zoomMinimum;
    priv->settings.zoomRange.maxScale = zoomMaximum;
    priv->settings.zoomRange.userScalable = true;

    priv->settings.domTimerInterval = pageSettings.defaultMinDOMTimerInterval();

    priv->settings.allowUniversalAccessFromFileURLs = pageSettings.allowUniversalAccessFromFileURLs();
    priv->settings.allowFileAccessFromFileURLs = pageSettings.allowFileAccessFromFileURLs();

    priv->history = ewk_history_new(static_cast<WebCore::BackForwardList*>(priv->page->backForward().client()));

#ifdef HAVE_ECORE_X
    priv->isUsingEcoreX = WebCore::isUsingEcoreX(smartData->base.evas);
#endif

#if ENABLE(CONTEXT_MENUS)
    priv->contextMenu = 0;
#endif

    return priv;
}

static void _ewk_view_priv_del(Ewk_View_Private_Data* priv)
{
    if (!priv)
        return;

    /* do not delete priv->main_frame */

    free(priv->repaints.array);

    eina_stringshare_del(priv->settings.userAgent);
    eina_stringshare_del(priv->settings.userStylesheet);
    eina_stringshare_del(priv->settings.encodingDefault);
    eina_stringshare_del(priv->settings.encodingCustom);
    eina_stringshare_del(priv->settings.fontStandard);
    eina_stringshare_del(priv->settings.fontCursive);
    eina_stringshare_del(priv->settings.fontMonospace);
    eina_stringshare_del(priv->settings.fontFantasy);
    eina_stringshare_del(priv->settings.fontSerif);
    eina_stringshare_del(priv->settings.fontSansSerif);
    eina_stringshare_del(priv->settings.localStorageDatabasePath);

    if (priv->animatedZoom.animator)
        ecore_animator_del(priv->animatedZoom.animator);

    ewk_history_free(priv->history);

    if (priv->cursorObject)
        evas_object_del(priv->cursorObject);

#if ENABLE(CONTEXT_MENUS)
    if (priv->contextMenu)
        ewk_context_menu_free(priv->contextMenu);
#endif

    priv->acceleratedCompositingContext = nullptr;

    delete priv;
}

static void _ewk_view_accelerated_compositing_cb(void* data, Evas_Object*)
{
    static_cast<WebCore::AcceleratedCompositingContext*>(data)->flushAndRenderLayers();
}

static void _ewk_view_smart_add(Evas_Object* ewkView)
{
    const Evas_Smart* smart = evas_object_smart_smart_get(ewkView);
    const Evas_Smart_Class* smartClass = evas_smart_class_get(smart);
    const Ewk_View_Smart_Class* api = reinterpret_cast<const Ewk_View_Smart_Class*>(smartClass);
    EWK_VIEW_SD_GET(ewkView, smartData);

    if (!smartData) {
        smartData = static_cast<Ewk_View_Smart_Data*>(calloc(1, sizeof(Ewk_View_Smart_Data)));
        if (!smartData) {
            CRITICAL("could not allocate Ewk_View_Smart_Data");
            return;
        }
        evas_object_smart_data_set(ewkView, smartData);
    }

    smartData->bg_color.r = 255;
    smartData->bg_color.g = 255;
    smartData->bg_color.b = 255;
    smartData->bg_color.a = 255;

    smartData->self = ewkView;
    smartData->api = api;

    _parent_sc.add(ewkView);

    smartData->_priv = _ewk_view_priv_new(smartData);
    if (!smartData->_priv)
        return;

    EWK_VIEW_PRIV_GET(smartData, priv);

    smartData->image = evas_object_image_add(smartData->base.evas);
    if (EINA_UNLIKELY(!smartData->image)) {
        ERR("Could not create backing store object.");
        return;
    }

    evas_object_image_alpha_set(smartData->image, false);
    evas_object_image_filled_set(smartData->image, true);
    evas_object_image_smooth_scale_set(smartData->image, smartData->zoom_weak_smooth_scale);
    evas_object_pass_events_set(smartData->image, true);
    evas_object_smart_member_add(smartData->image, ewkView);
    evas_object_show(smartData->image);

    priv->acceleratedCompositingContext = std::make_unique<WebCore::AcceleratedCompositingContext>(ewkView, smartData->image);

    // Set the pixel get callback.
    evas_object_image_pixels_get_callback_set(smartData->image, _ewk_view_accelerated_compositing_cb, priv->acceleratedCompositingContext.get());

    smartData->events_rect = evas_object_rectangle_add(smartData->base.evas);
    evas_object_color_set(smartData->events_rect, 0, 0, 0, 0);
    evas_object_smart_member_add(smartData->events_rect, ewkView);
    evas_object_show(smartData->events_rect);

    smartData->main_frame = ewk_frame_add(smartData->base.evas);
    if (!smartData->main_frame) {
        ERR("Could not create main frame object.");
        return;
    }

    if (!ewk_frame_init(smartData->main_frame, ewkView, adoptPtr(static_cast<WebCore::FrameLoaderClientEfl*>(&priv->page->mainFrame().loader().client())))) {
        ERR("Could not initialize main frme object.");
        evas_object_del(smartData->main_frame);
        smartData->main_frame = 0;

        return;
    }
    EWKPrivate::setCoreFrame(smartData->main_frame, &priv->page->mainFrame());
    priv->page->mainFrame().tree().setName(String());
    priv->page->mainFrame().init();

    evas_object_name_set(smartData->main_frame, "EWK_Frame:main");
    evas_object_smart_member_add(smartData->main_frame, ewkView);
    evas_object_show(smartData->main_frame);

#define CONNECT(s, c) evas_object_event_callback_add(ewkView, s, c, smartData)
    CONNECT(EVAS_CALLBACK_FOCUS_IN, _ewk_view_on_focus_in);
    CONNECT(EVAS_CALLBACK_FOCUS_OUT, _ewk_view_on_focus_out);
    CONNECT(EVAS_CALLBACK_MOUSE_WHEEL, _ewk_view_on_mouse_wheel);
    CONNECT(EVAS_CALLBACK_MOUSE_DOWN, _ewk_view_on_mouse_down);
    CONNECT(EVAS_CALLBACK_MOUSE_UP, _ewk_view_on_mouse_up);
    CONNECT(EVAS_CALLBACK_MOUSE_MOVE, _ewk_view_on_mouse_move);
    CONNECT(EVAS_CALLBACK_KEY_DOWN, _ewk_view_on_key_down);
    CONNECT(EVAS_CALLBACK_KEY_UP, _ewk_view_on_key_up);
#undef CONNECT
}

static void _ewk_view_smart_del(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET(ewkView, smartData);
    Ewk_View_Private_Data* priv = smartData ? smartData->_priv : 0;

    ewk_view_stop(ewkView);
    _parent_sc.del(ewkView);
    _ewk_view_priv_del(priv);
}

static void _ewk_view_smart_resize(Evas_Object* ewkView, Evas_Coord width, Evas_Coord height)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    // these should be queued and processed in calculate as well!
    evas_object_resize(smartData->image, width, height);
    evas_object_image_size_set(smartData->image, width, height);
    evas_object_image_fill_set(smartData->image, 0, 0, width, height);

    priv->acceleratedCompositingContext->resize(WebCore::IntSize(width, height));

    smartData->changed.size = true;
    _ewk_view_smart_changed(smartData);
}

static void _ewk_view_smart_move(Evas_Object* ewkView, Evas_Coord /*x*/, Evas_Coord /*y*/)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    smartData->changed.position = true;
    _ewk_view_smart_changed(smartData);
}

static inline void _ewk_view_screen_move(uint32_t* image, size_t destinationX, size_t destinationY, size_t sourceX, size_t sourceY, size_t copyWidth, size_t copyHeight, size_t imageWidth)
{
    uint32_t* sourceBegin = image + (imageWidth * sourceY) + sourceX;
    uint32_t* destinationBegin = image + (imageWidth * destinationY) + destinationX;

    size_t copyLength = copyWidth * 4;

    int moveLineUpDown;
    int row;

    if (sourceY >= destinationY) {
        row = 0;
        moveLineUpDown = 1;
    } else {
        row = copyHeight - 1;
        moveLineUpDown = -1;
    }

    uint32_t* source, * destination;
    if ((destinationX > sourceX && destinationX < sourceX + copyWidth)
        || (destinationX < sourceX && destinationX + copyWidth > sourceX)) {
        for (size_t i = 0; i < copyHeight; ++i, row += moveLineUpDown) {
            source = sourceBegin + (imageWidth * row);
            destination = destinationBegin + (imageWidth * row);
            memmove(destination, source, copyLength);
        }
    } else {
        for (size_t i = 0; i < copyHeight; ++i, row += moveLineUpDown) {
            source = sourceBegin + (imageWidth * row);
            destination = destinationBegin + (imageWidth * row);
            memcpy(destination, source, copyLength);
        }
    }
}

static void _ewk_view_smart_calculate(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);
    Evas_Coord x, y, width, height;

    smartData->changed.any = false;

    if (!smartData->main_frame)
        return;

    evas_object_geometry_get(ewkView, &x, &y, &width, &height);

    DBG("ewkView=%p geo=[%d, %d + %dx%d], changed: size=%huu",
        ewkView, x, y, width, height, smartData->changed.size);

    if (smartData->changed.size && ((width != smartData->view.w) || (height != smartData->view.h))) {
        WebCore::FrameView* view = priv->page->mainFrame().view();
        if (view) {
            view->resize(width, height);
            view->forceLayout();
            view->adjustViewSize();
        }
        evas_object_resize(smartData->main_frame, width, height);
        evas_object_resize(smartData->events_rect, width, height);
        smartData->changed.frame_rect = true;
        smartData->view.w = width;
        smartData->view.h = height;

        _ewk_view_repaint_add(priv, 0, 0, width, height);

        // This callback is a good place e.g. to change fixed layout size (ewk_view_fixed_layout_size_set).
        evas_object_smart_callback_call(ewkView, "view,resized", 0);
    }
    smartData->changed.size = false;

    if (smartData->changed.position && ((x != smartData->view.x) || (y != smartData->view.y))) {
        evas_object_move(smartData->main_frame, x, y);
        evas_object_move(smartData->image, x, y);
        evas_object_move(smartData->events_rect, x, y);
        smartData->changed.frame_rect = true;
        smartData->view.x = x;
        smartData->view.y = y;
    }
    smartData->changed.position = false;

    if (smartData->changed.frame_rect) {
        priv->page->mainFrame().view()->frameRectsChanged();
        smartData->changed.frame_rect = false;
    }
}

static void _ewk_view_smart_show(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    if (evas_object_clipees_get(smartData->base.clipper))
        evas_object_show(smartData->base.clipper);
    evas_object_show(smartData->image);
}

static void _ewk_view_smart_hide(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    evas_object_hide(smartData->base.clipper);
    evas_object_hide(smartData->image);
}

static Eina_Bool _ewk_view_smart_contents_resize(Ewk_View_Smart_Data*, int /*width*/, int /*height*/)
{
    return true;
}

static Eina_Bool _ewk_view_smart_zoom_set(Ewk_View_Smart_Data* smartData, float zoom, Evas_Coord centerX, Evas_Coord centerY)
{
    double px, py;
    Evas_Coord x, y, width, height;
    Eina_Bool result;

    ewk_frame_scroll_size_get(smartData->main_frame, &width, &height);
    ewk_frame_scroll_pos_get(smartData->main_frame, &x, &y);

    if (width + smartData->view.w > 0)
        px = static_cast<double>(x + centerX) / (width + smartData->view.w);
    else
        px = 0.0;

    if (height + smartData->view.h > 0)
        py = static_cast<double>(y + centerY) / (height + smartData->view.h);
    else
        py = 0.0;

    result = ewk_frame_page_zoom_set(smartData->main_frame, zoom);

    ewk_frame_scroll_size_get(smartData->main_frame, &width, &height);
    x = (width + smartData->view.w) * px - centerX;
    y = (height + smartData->view.h) * py - centerY;
    ewk_frame_scroll_set(smartData->main_frame, x, y);
    return result;
}

static void _ewk_view_smart_flush(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    _ewk_view_repaints_flush(priv);
}

static void _ewk_view_zoom_animated_mark_stop(Ewk_View_Smart_Data* smartData)
{
    smartData->animated_zoom.zoom.start = 0.0;
    smartData->animated_zoom.zoom.end = 0.0;
    smartData->animated_zoom.zoom.current = 0.0;
}

static void _ewk_view_zoom_animated_finish(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_PRIV_GET(smartData, priv);
    ecore_animator_del(priv->animatedZoom.animator);
    priv->animatedZoom.animator = 0;
    _ewk_view_zoom_animated_mark_stop(smartData);
    evas_object_smart_callback_call(smartData->self, "zoom,animated,end", 0);
}

static float _ewk_view_zoom_animated_current(Ewk_View_Private_Data* priv)
{
    double now = ecore_loop_time_get();
    double delta = now - priv->animatedZoom.time.start;

    if (delta > priv->animatedZoom.time.duration)
        delta = priv->animatedZoom.time.duration;
    if (delta < 0.0) // time went back, clock adjusted?
        delta = 0.0;

    delta /= priv->animatedZoom.time.duration;

    return ((priv->animatedZoom.zoom.range * delta)
            + priv->animatedZoom.zoom.start);
}

static Eina_Bool _ewk_view_smart_zoom_weak_set(Ewk_View_Smart_Data* smartData, float zoom, Evas_Coord centerX, Evas_Coord centerY)
{
    float scale = zoom / smartData->animated_zoom.zoom.start;
    Evas_Coord w = smartData->view.w * scale;
    Evas_Coord h = smartData->view.h * scale;
    Evas_Coord deltaX, deltaY, contentWidth, contentHeight;
    Evas_Object* clip = evas_object_clip_get(smartData->image);

    ewk_frame_contents_size_get(smartData->main_frame, &contentWidth, &contentHeight);
    if (smartData->view.w > 0 && smartData->view.h > 0) {
        deltaX = (w * (smartData->view.w - centerX)) / smartData->view.w;
        deltaY = (h * (smartData->view.h - centerY)) / smartData->view.h;
    } else {
        deltaX = 0;
        deltaY = 0;
    }

    evas_object_image_fill_set(smartData->image, centerX + deltaX, centerY + deltaY, w, h);

    if (smartData->view.w > 0 && smartData->view.h > 0) {
        deltaX = ((smartData->view.w - w) * centerX) / smartData->view.w;
        deltaY = ((smartData->view.h - h) * centerY) / smartData->view.h;
    } else {
        deltaX = 0;
        deltaY = 0;
    }
    evas_object_move(clip, smartData->view.x + deltaX, smartData->view.y + deltaY);

    if (contentWidth < smartData->view.w)
        w = contentWidth * scale;
    if (contentHeight < smartData->view.h)
        h = contentHeight * scale;
    evas_object_resize(clip, w, h);

    return true;
}

static void _ewk_view_smart_zoom_weak_smooth_scale_set(Ewk_View_Smart_Data* smartData, Eina_Bool smooth_scale)
{
    evas_object_image_smooth_scale_set(smartData->image, smooth_scale);
}

static Eina_Bool _ewk_view_zoom_animator_cb(void* data)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);
    Evas_Coord centerX, centerY;
    EWK_VIEW_PRIV_GET(smartData, priv);
    double now = ecore_loop_time_get();

    centerX = priv->animatedZoom.center.x;
    centerY = priv->animatedZoom.center.y;

    // TODO: progressively center (cx, cy) -> (view.x + view.h/2, view.y + view.h/2)
    if (centerX >= smartData->view.w)
        centerX = smartData->view.w - 1;
    if (centerY >= smartData->view.h)
        centerY = smartData->view.h - 1;

    if ((now >= priv->animatedZoom.time.end)
        || (now < priv->animatedZoom.time.start)) {
        _ewk_view_zoom_animated_finish(smartData);
        ewk_view_zoom_set(smartData->self, priv->animatedZoom.zoom.end, centerX, centerY);
        smartData->api->sc.calculate(smartData->self);
        return false;
    }

    smartData->animated_zoom.zoom.current = _ewk_view_zoom_animated_current(priv);
    smartData->api->zoom_weak_set(smartData, smartData->animated_zoom.zoom.current, centerX, centerY);
    return true;
}

static void _ewk_view_zoom_animation_start(Ewk_View_Smart_Data* smartData)
{
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    if (priv->animatedZoom.animator)
        return;
    priv->animatedZoom.animator = ecore_animator_add
                                       (_ewk_view_zoom_animator_cb, smartData);
}

static WebCore::ViewportAttributes _ewk_view_viewport_attributes_compute(Ewk_View_Private_Data* priv)
{
    int desktopWidth = 980;
    WebCore::IntRect availableRect = enclosingIntRect(priv->page->chrome().client().pageRect());
    WebCore::IntRect deviceRect = enclosingIntRect(priv->page->chrome().client().windowRect());

    WebCore::ViewportAttributes attributes = WebCore::computeViewportAttributes(priv->viewportArguments, desktopWidth, deviceRect.width(), deviceRect.height(), priv->page->deviceScaleFactor(), availableRect.size());
    WebCore::restrictMinimumScaleFactorToViewportSize(attributes, availableRect.size(), priv->page->deviceScaleFactor());
    WebCore::restrictScaleFactorToInitialScaleIfNotUserScalable(attributes);

    return attributes;
}

static Eina_Bool _ewk_view_smart_disable_render(Ewk_View_Smart_Data* smartData)
{
    WARN("not supported by engine. smartData=%p", smartData);
    return false;
}

static Eina_Bool _ewk_view_smart_enable_render(Ewk_View_Smart_Data* smartData)
{
    WARN("not supported by engine. smartData=%p", smartData);
    return false;
}

static const char* _ewk_view_editor_command_string_get(Ewk_View_Private_Data*, Ewk_Editor_Command ewkCommand)
{
    static EflUniquePtr<Eina_Hash> editorCommandHash;

    if (!editorCommandHash) {
        editorCommandHash = EflUniquePtr<Eina_Hash>(eina_hash_int32_new(0));
        for (int i = 0; editorCommands[i].ewkEditorCommand != EWK_EDITOR_COMMAND_NONE; i++)
            eina_hash_add(editorCommandHash.get(), &editorCommands[i].ewkEditorCommand, editorCommands[i].editorCommandString);
    }
    return reinterpret_cast<const char*>(eina_hash_find(editorCommandHash.get(), &ewkCommand));
}

Eina_Bool ewk_view_smart_set(Ewk_View_Smart_Class* api)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(api, false);

    if (api->version != EWK_VIEW_SMART_CLASS_VERSION) {
        EINA_LOG_CRIT
            ("Ewk_View_Smart_Class %p is version %lu while %lu was expected.",
            api, api->version, EWK_VIEW_SMART_CLASS_VERSION);
        return false;
    }

    if (EINA_UNLIKELY(!_parent_sc.add))
        evas_object_smart_clipped_smart_set(&_parent_sc);

    evas_object_smart_clipped_smart_set(&api->sc);
    api->sc.add = _ewk_view_smart_add;
    api->sc.del = _ewk_view_smart_del;
    api->sc.resize = _ewk_view_smart_resize;
    api->sc.move = _ewk_view_smart_move;
    api->sc.calculate = _ewk_view_smart_calculate;
    api->sc.show = _ewk_view_smart_show;
    api->sc.hide = _ewk_view_smart_hide;
    api->sc.data = ewkViewTypeString; /* used by type checking */
    api->sc.callbacks = _ewk_view_callback_names;

    api->contents_resize = _ewk_view_smart_contents_resize;
    api->zoom_set = _ewk_view_smart_zoom_set;
    api->zoom_weak_set = _ewk_view_smart_zoom_weak_set;
    api->zoom_weak_smooth_scale_set = _ewk_view_smart_zoom_weak_smooth_scale_set;
    api->flush = _ewk_view_smart_flush;
    api->disable_render = _ewk_view_smart_disable_render;
    api->enable_render = _ewk_view_smart_enable_render;

    api->focus_in = _ewk_view_smart_focus_in;
    api->focus_out = _ewk_view_smart_focus_out;
    api->mouse_wheel = _ewk_view_smart_mouse_wheel;
    api->mouse_down = _ewk_view_smart_mouse_down;
    api->mouse_up = _ewk_view_smart_mouse_up;
    api->mouse_move = _ewk_view_smart_mouse_move;
    api->key_down = _ewk_view_smart_key_down;
    api->key_up = _ewk_view_smart_key_up;

    api->add_console_message = _ewk_view_smart_add_console_message;
    api->run_javascript_alert = _ewk_view_smart_run_javascript_alert;
    api->run_javascript_confirm = _ewk_view_smart_run_javascript_confirm;
    api->run_before_unload_confirm = _ewk_view_smart_run_before_unload_confirm;
    api->run_javascript_prompt = _ewk_view_smart_run_javascript_prompt;
    api->should_interrupt_javascript = _ewk_view_smart_should_interrupt_javascript;

    return true;
}

static inline Evas_Smart* _ewk_view_smart_class_new()
{
    static Ewk_View_Smart_Class api = EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION(ewkViewTypeString);
    static Evas_Smart* smart = 0;

    if (EINA_UNLIKELY(!smart)) {
        ewk_view_smart_set(&api);
        smart = evas_smart_class_new(&api.sc);
    }

    return smart;
}

Evas_Object* ewk_view_add(Evas* canvas)
{
    return evas_object_smart_add(canvas, _ewk_view_smart_class_new());
}

void ewk_view_fixed_layout_size_set(Evas_Object* ewkView, Evas_Coord width, Evas_Coord height)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    WebCore::FrameView* view = priv->page->mainFrame().view();
    if (!view)
        return;

    WebCore::IntSize layoutSize(width, height);
    if (layoutSize.width() <= 0 && layoutSize.height() <= 0) {
        if (!view->useFixedLayout())
            return;
        view->setUseFixedLayout(false);
    } else {
        WebCore::IntSize fixedLayoutSize = view->fixedLayoutSize();
        if (fixedLayoutSize == layoutSize)
            return;
        view->setFixedLayoutSize(layoutSize);
        view->setUseFixedLayout(true);
    }

    view->setNeedsLayout();
}

void ewk_view_fixed_layout_size_get(const Evas_Object* ewkView, Evas_Coord* width, Evas_Coord* height)
{
    if (width)
        *width = 0;
    if (height)
        *height = 0;
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    WebCore::FrameView* view = priv->page->mainFrame().view();
    if (view->useFixedLayout()) {
        WebCore::IntSize size = view->fixedLayoutSize();
        if (width)
            *width = size.width();
        if (height)
            *height = size.height();
    }
}

void ewk_view_theme_set(Evas_Object* ewkView, const char* path)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    if (!eina_stringshare_replace(&priv->settings.theme, path))
        return;

    WebCore::RenderThemeEfl& theme = static_cast<WebCore::RenderThemeEfl&>(priv->page->theme());
    theme.setThemePath(path);
}

const char* ewk_view_theme_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.theme;
}

Evas_Object* ewk_view_frame_main_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    return smartData->main_frame;
}

Evas_Object* ewk_view_frame_focused_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    WebCore::Frame* core = priv->page->focusController().focusedFrame();
    if (!core)
        return 0;

    WebCore::FrameLoaderClientEfl& client = static_cast<WebCore::FrameLoaderClientEfl&>(core->loader().client());
    return client.webFrame();
}

Eina_Bool ewk_view_uri_set(Evas_Object* ewkView, const char* uri)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_uri_set(smartData->main_frame, uri);
}

const char* ewk_view_uri_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    return ewk_frame_uri_get(smartData->main_frame);
}

const Ewk_Text_With_Direction* ewk_view_title_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    return ewk_frame_title_get(smartData->main_frame);
}

Eina_Bool ewk_view_editable_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_editable_get(smartData->main_frame);
}

void ewk_view_bg_color_set(Evas_Object* ewkView, int red, int green, int blue, int alpha)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);

    if (alpha < 0) {
        WARN("Alpha less than zero (%d).", alpha);
        alpha = 0;
    } else if (alpha > 255) {
        WARN("Alpha is larger than 255 (%d).", alpha);
        alpha = 255;
    }

#define CHECK_PREMUL_COLOR(color, alpha)                                        \
    if (color < 0) {                                                        \
        WARN("Color component " #color " is less than zero (%d).", color);         \
        color = 0;                                                          \
    } else if (color > alpha) {                                                 \
        WARN("Color component " #color " is greater than alpha (%d, alpha=%d).", \
            color, alpha);                                                      \
        color = alpha;                                                          \
    }
    CHECK_PREMUL_COLOR(red, alpha);
    CHECK_PREMUL_COLOR(green, alpha);
    CHECK_PREMUL_COLOR(blue, alpha);
#undef CHECK_PREMUL_COLOR

    smartData->bg_color.r = red;
    smartData->bg_color.g = green;
    smartData->bg_color.b = blue;
    smartData->bg_color.a = alpha;

    evas_object_image_alpha_set(smartData->image, alpha < 255);

    WebCore::FrameView* view = smartData->_priv->page->mainFrame().view();
    if (view) {
        WebCore::Color color;

        if (!alpha)
            color = WebCore::Color(0, 0, 0, 0);
        else if (alpha == 255)
            color = WebCore::Color(red, green, blue, alpha);
        else
            color = WebCore::Color(red * 255 / alpha, green * 255 / alpha, blue * 255 / alpha, alpha);

        view->updateBackgroundRecursively(color, !alpha);
    }
}

void ewk_view_bg_color_get(const Evas_Object* ewkView, int* red, int* green, int* blue, int* alpha)
{
    if (red)
        *red = 0;
    if (green)
        *green = 0;
    if (blue)
        *blue = 0;
    if (alpha)
        *alpha = 0;
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    if (red)
        *red = smartData->bg_color.r;
    if (green)
        *green = smartData->bg_color.g;
    if (blue)
        *blue = smartData->bg_color.b;
    if (alpha)
        *alpha = smartData->bg_color.a;
}

Eina_Bool ewk_view_text_search(const Evas_Object* ewkView, const char* string, Eina_Bool caseSensitive, Eina_Bool forward, Eina_Bool wrap)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(string, false);

    WebCore::FindOptions options = (caseSensitive ? 0 : WebCore::CaseInsensitive) | (forward ? 0 : WebCore::Backwards) | (wrap ? WebCore::WrapAround : 0);
    return priv->page->findString(String::fromUTF8(string), options);
}

/**
 * Mark matches the given text string in document.
 *
 * @param ewkView view object where to search text.
 * @param string reference string to match.
 * @param caseSensitive if match should be case sensitive or not.
 * @param heightighlight if matches should be highlighted.
 * @param limit maximum amount of matches, or zero to unlimited.
 *
 * @return number of matches.
 */
unsigned int ewk_view_text_matches_mark(Evas_Object* ewkView, const char* string, Eina_Bool caseSensitive, Eina_Bool highlight, unsigned int limit)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(string, 0);
    WTF::TextCaseSensitivity sensitive;

    if (caseSensitive)
        sensitive = WTF::TextCaseSensitive;
    else
        sensitive = WTF::TextCaseInsensitive;

    return priv->page->markAllMatchesForText(String::fromUTF8(string), sensitive, highlight, limit);
}

Eina_Bool ewk_view_text_matches_unmark_all(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    priv->page->unmarkAllTextMatches();
    return true;
}

Eina_Bool ewk_view_text_matches_highlight_set(Evas_Object* ewkView, Eina_Bool highlight)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_text_matches_highlight_set(smartData->main_frame, highlight);
}

Eina_Bool ewk_view_text_matches_highlight_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_text_matches_highlight_get(smartData->main_frame);
}

Eina_Bool ewk_view_editable_set(Evas_Object* ewkView, Eina_Bool editable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_editable_set(smartData->main_frame, editable);
}

const char* ewk_view_selection_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    CString selectedString = priv->page->focusController().focusedOrMainFrame().editor().selectedText().utf8();
    if (selectedString.isNull())
        return 0;
    return eina_stringshare_add(selectedString.data());
}

Eina_Bool ewk_view_editor_command_execute(const Evas_Object* ewkView, const Ewk_Editor_Command command, const char* value)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    const char* commandString = _ewk_view_editor_command_string_get(priv, command);
    if (!commandString)
        return false;

    return priv->page->focusController().focusedOrMainFrame().editor().command(commandString).execute(WTF::String::fromUTF8(value));
}

Eina_Bool ewk_view_context_menu_forward_event(Evas_Object* ewkView, const Evas_Event_Mouse_Down* downEvent)
{
#if ENABLE(CONTEXT_MENUS)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    Eina_Bool mouse_press_handled = false;

    priv->page->contextMenuController().clearContextMenu();
    if (priv->contextMenu)
        ewk_context_menu_free(priv->contextMenu);

    WebCore::Frame& mainFrame = priv->page->mainFrame();
    Evas_Coord x, y;
    evas_object_geometry_get(smartData->self, &x, &y, 0, 0);

    WebCore::PlatformMouseEvent event(downEvent, WebCore::IntPoint(x, y));

    if (mainFrame.view()) {
        mouse_press_handled =
            mainFrame.eventHandler().handleMousePressEvent(event);
    }

    if (!mainFrame.eventHandler().sendContextMenuEvent(event))
        return false;

    WebCore::ContextMenu* coreMenu =
        priv->page->contextMenuController().contextMenu();
    if (!coreMenu) {
        // WebCore decided not to create a context menu, return true if event
        // was handled by handleMouseReleaseEvent
        return mouse_press_handled;
    }

    priv->contextMenu = ewk_context_menu_new(ewkView, &priv->page->contextMenuController(), coreMenu);
    if (!priv->contextMenu)
        return false;

    ewk_context_menu_show(priv->contextMenu);

    return true;
#else
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(downEvent);
    return false;
#endif
}

double ewk_view_load_progress_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);
    return priv->page->progress().estimatedProgress();
}

Eina_Bool ewk_view_stop(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_stop(smartData->main_frame);
}

Eina_Bool ewk_view_reload(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_reload(smartData->main_frame);
}

Eina_Bool ewk_view_reload_full(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_reload_full(smartData->main_frame);
}

Eina_Bool ewk_view_back(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_back(smartData->main_frame);
}

Eina_Bool ewk_view_forward(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_forward(smartData->main_frame);
}

Eina_Bool ewk_view_navigate(Evas_Object* ewkView, int steps)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_navigate(smartData->main_frame, steps);
}

Eina_Bool ewk_view_back_possible(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_back_possible(smartData->main_frame);
}

Eina_Bool ewk_view_forward_possible(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_forward_possible(smartData->main_frame);
}

Eina_Bool ewk_view_navigate_possible(Evas_Object* ewkView, int steps)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_navigate_possible(smartData->main_frame, steps);
}

Eina_Bool ewk_view_history_enable_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return static_cast<WebCore::BackForwardList*>(priv->page->backForward().client())->enabled();
}

Eina_Bool ewk_view_history_enable_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    static_cast<WebCore::BackForwardList*>(priv->page->backForward().client())->setEnabled(enable);
    return true;
}

Ewk_History* ewk_view_history_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    if (!static_cast<WebCore::BackForwardList*>(priv->page->backForward().client())->enabled()) {
        ERR("asked history, but it's disabled! Returning 0!");
        return 0;
    }
    return priv->history;
}

Eina_Bool ewk_view_visited_link_add(Evas_Object* ewkView, const char* visitedUrl)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    EINA_SAFETY_ON_NULL_RETURN_VAL(priv->page, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv->page->groupPtr(), false);

    WebCore::URL kurl(WebCore::URL(), WTF::String::fromUTF8(visitedUrl));
    priv->page->groupPtr()->addVisitedLink(kurl);
    return true;
}

float ewk_view_zoom_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    return ewk_frame_page_zoom_get(smartData->main_frame);
}

Eina_Bool ewk_view_zoom_set(Evas_Object* ewkView, float zoom, Evas_Coord centerX, Evas_Coord centerY)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET(smartData, priv);

    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api->zoom_set, false);

    if (!priv->settings.zoomRange.userScalable) {
        WARN("userScalable is false");
        return false;
    }

    if (zoom < priv->settings.zoomRange.minScale) {
        WARN("zoom level is < %f : %f", priv->settings.zoomRange.minScale, zoom);
        return false;
    }
    if (zoom > priv->settings.zoomRange.maxScale) {
        WARN("zoom level is > %f : %f", priv->settings.zoomRange.maxScale, zoom);
        return false;
    }

    _ewk_view_zoom_animated_mark_stop(smartData);
    return smartData->api->zoom_set(smartData, zoom, centerX, centerY);
}

float ewk_view_page_zoom_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    return ewk_frame_page_zoom_get(smartData->main_frame);
}

Eina_Bool ewk_view_page_zoom_set(Evas_Object* ewkView, float pageZoomFactor)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_page_zoom_set(smartData->main_frame, pageZoomFactor);
}

float ewk_view_scale_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);
    return priv->page->pageScaleFactor();
}

Eina_Bool ewk_view_scale_set(Evas_Object* ewkView, float scaleFactor, Evas_Coord scrollX, Evas_Coord scrollY)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    float currentScaleFactor = ewk_view_scale_get(ewkView);
    if (currentScaleFactor == -1)
        return false;

    priv->page->setPageScaleFactor(scaleFactor, WebCore::IntPoint(scrollX, scrollY));
    return true;
}

float ewk_view_text_zoom_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    return ewk_frame_text_zoom_get(smartData->main_frame);
}

Eina_Bool ewk_view_text_zoom_set(Evas_Object* ewkView, float textZoomFactor)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return ewk_frame_text_zoom_set(smartData->main_frame, textZoomFactor);
}

Eina_Bool ewk_view_zoom_weak_smooth_scale_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    return smartData->zoom_weak_smooth_scale;
}

void ewk_view_zoom_weak_smooth_scale_set(Evas_Object* ewkView, Eina_Bool smoothScale)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    smoothScale = !!smoothScale;
    if (smartData->zoom_weak_smooth_scale == smoothScale)
        return;
    smartData->zoom_weak_smooth_scale = smoothScale;
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->zoom_weak_smooth_scale_set);
    smartData->api->zoom_weak_smooth_scale_set(smartData, smoothScale);
}

Eina_Bool ewk_view_zoom_weak_set(Evas_Object* ewkView, float zoom, Evas_Coord centerX, Evas_Coord centerY)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET(smartData, priv);

    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api->zoom_weak_set, false);

    if (!priv->settings.zoomRange.userScalable) {
        WARN("userScalable is false");
        return false;
    }

    if (zoom < priv->settings.zoomRange.minScale) {
        WARN("zoom level is < %f : %f", priv->settings.zoomRange.minScale, zoom);
        return false;
    }
    if (zoom > priv->settings.zoomRange.maxScale) {
        WARN("zoom level is > %f : %f", priv->settings.zoomRange.maxScale, zoom);
        return false;
    }

    smartData->animated_zoom.zoom.start = ewk_frame_page_zoom_get(smartData->main_frame);
    smartData->animated_zoom.zoom.end = zoom;
    smartData->animated_zoom.zoom.current = zoom;
    return smartData->api->zoom_weak_set(smartData, zoom, centerX, centerY);
}

Eina_Bool ewk_view_zoom_animated_mark_start(Evas_Object* ewkView, float zoom)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    smartData->animated_zoom.zoom.start = zoom;
    return true;
}

Eina_Bool ewk_view_zoom_animated_mark_end(Evas_Object* ewkView, float zoom)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    smartData->animated_zoom.zoom.end = zoom;
    return true;
}

Eina_Bool ewk_view_zoom_animated_mark_current(Evas_Object* ewkView, float zoom)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    smartData->animated_zoom.zoom.current = zoom;
    return true;
}

Eina_Bool ewk_view_zoom_animated_mark_stop(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    _ewk_view_zoom_animated_mark_stop(smartData);
    return true;
}

Eina_Bool ewk_view_zoom_animated_set(Evas_Object* ewkView, float zoom, float duration, Evas_Coord centerX, Evas_Coord centerY)
{
    double now;
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api->zoom_weak_set, false);

    if (!priv->settings.zoomRange.userScalable) {
        WARN("userScalable is false");
        return false;
    }

    if (zoom < priv->settings.zoomRange.minScale) {
        WARN("zoom level is < %f : %f", priv->settings.zoomRange.minScale, zoom);
        return false;
    }
    if (zoom > priv->settings.zoomRange.maxScale) {
        WARN("zoom level is > %f : %f", priv->settings.zoomRange.maxScale, zoom);
        return false;
    }

    if (priv->animatedZoom.animator)
        priv->animatedZoom.zoom.start = _ewk_view_zoom_animated_current(priv);
    else {
        priv->animatedZoom.zoom.start = ewk_frame_page_zoom_get(smartData->main_frame);
        _ewk_view_zoom_animation_start(smartData);
    }

    if (centerX < 0)
        centerX = 0;
    if (centerY < 0)
        centerY = 0;

    now = ecore_loop_time_get();
    priv->animatedZoom.time.start = now;
    priv->animatedZoom.time.end = now + duration;
    priv->animatedZoom.time.duration = duration;
    priv->animatedZoom.zoom.end = zoom;
    priv->animatedZoom.zoom.range = (priv->animatedZoom.zoom.end - priv->animatedZoom.zoom.start);
    priv->animatedZoom.center.x = centerX;
    priv->animatedZoom.center.y = centerY;
    smartData->animated_zoom.zoom.current = priv->animatedZoom.zoom.start;
    smartData->animated_zoom.zoom.start = priv->animatedZoom.zoom.start;
    smartData->animated_zoom.zoom.end = priv->animatedZoom.zoom.end;

    return true;
}

unsigned int ewk_view_imh_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->imh;
}

Eina_Bool ewk_view_enable_render(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api->enable_render, false);
    return smartData->api->enable_render(smartData);
}

Eina_Bool ewk_view_disable_render(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api->disable_render, false);
    return smartData->api->disable_render(smartData);
}

const char* ewk_view_setting_user_agent_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.userAgent;
}

Eina_Bool ewk_view_setting_user_agent_set(Evas_Object* ewkView, const char* userAgent)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (eina_stringshare_replace(&priv->settings.userAgent, userAgent)) {
        WebCore::FrameLoaderClientEfl& client = static_cast<WebCore::FrameLoaderClientEfl&>(priv->page->mainFrame().loader().client());
        client.setCustomUserAgent(String::fromUTF8(userAgent));
    }
    return true;
}

const char* ewk_view_setting_user_stylesheet_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.userStylesheet;
}

Eina_Bool ewk_view_setting_user_stylesheet_set(Evas_Object* ewkView, const char* uri)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (eina_stringshare_replace(&priv->settings.userStylesheet, uri)) {
        WebCore::URL kurl(WebCore::URL(), String::fromUTF8(uri));
        priv->page->settings().setUserStyleSheetLocation(kurl);
    }
    return true;
}

Eina_Bool ewk_view_setting_auto_load_images_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.autoLoadImages;
}

Eina_Bool ewk_view_setting_auto_load_images_set(Evas_Object* ewkView, Eina_Bool automatic)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    automatic = !!automatic;
    if (priv->settings.autoLoadImages != automatic) {
        priv->page->settings().setLoadsImagesAutomatically(automatic);
        priv->settings.autoLoadImages = automatic;
    }
    return true;
}

Eina_Bool ewk_view_setting_auto_shrink_images_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.autoShrinkImages;
}

Eina_Bool ewk_view_setting_auto_shrink_images_set(Evas_Object* ewkView, Eina_Bool automatic)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    automatic = !!automatic;
    if (priv->settings.autoShrinkImages != automatic) {
        priv->page->settings().setShrinksStandaloneImagesToFit(automatic);
        priv->settings.autoShrinkImages = automatic;
    }
    return true;
}

Eina_Bool ewk_view_setting_enable_auto_resize_window_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.enableAutoResizeWindow;
}

Eina_Bool ewk_view_setting_enable_auto_resize_window_set(Evas_Object* ewkView, Eina_Bool resizable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    priv->settings.enableAutoResizeWindow = resizable;
    return true;
}

Eina_Bool ewk_view_setting_enable_scripts_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.enableScripts;
}

Eina_Bool ewk_view_setting_enable_scripts_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.enableScripts != enable) {
        priv->page->settings().setScriptEnabled(enable);
        priv->settings.enableScripts = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_enable_plugins_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.enablePlugins;
}

Eina_Bool ewk_view_setting_enable_plugins_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.enablePlugins != enable) {
        priv->page->settings().setPluginsEnabled(enable);
        priv->settings.enablePlugins = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_enable_frame_flattening_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.enableFrameFlattening;
}

Eina_Bool ewk_view_setting_enable_frame_flattening_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.enableFrameFlattening != enable) {
        priv->page->settings().setFrameFlatteningEnabled(enable);
        priv->settings.enableFrameFlattening = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_scripts_can_open_windows_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.scriptsCanOpenWindows;
}

Eina_Bool ewk_view_setting_scripts_can_open_windows_set(Evas_Object* ewkView, Eina_Bool allow)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    allow = !!allow;
    if (priv->settings.scriptsCanOpenWindows != allow) {
        priv->page->settings().setJavaScriptCanOpenWindowsAutomatically(allow);
        priv->settings.scriptsCanOpenWindows = allow;
    }
    return true;
}

Eina_Bool ewk_view_setting_scripts_can_close_windows_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.scriptsCanCloseWindows;
}

Eina_Bool ewk_view_setting_scripts_can_close_windows_set(Evas_Object* ewkView, Eina_Bool allow)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    allow = !!allow;
    if (priv->settings.scriptsCanCloseWindows != allow) {
        priv->page->settings().setAllowScriptsToCloseWindows(allow);
        priv->settings.scriptsCanCloseWindows = allow;
    }
    return true;
}

Eina_Bool ewk_view_setting_scripts_can_access_clipboard_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.scriptsCanAccessClipboard;
}

Eina_Bool ewk_view_setting_scripts_can_access_clipboard_set(Evas_Object* ewkView, Eina_Bool allow)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    allow = !!allow;
    if (priv->settings.scriptsCanAccessClipboard != allow) {
        priv->page->settings().setJavaScriptCanAccessClipboard(allow);
        priv->page->settings().setDOMPasteAllowed(allow);
        priv->settings.scriptsCanAccessClipboard = allow;
    }
    return true;
}

Eina_Bool ewk_view_setting_resizable_textareas_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.resizableTextareas;
}

Eina_Bool ewk_view_setting_resizable_textareas_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.resizableTextareas != enable) {
        priv->page->settings().setTextAreasAreResizable(enable);
        priv->settings.resizableTextareas = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_private_browsing_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.privateBrowsing;
}

Eina_Bool ewk_view_setting_private_browsing_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.privateBrowsing != enable) {
        priv->page->enableLegacyPrivateBrowsing(enable);
        priv->settings.privateBrowsing = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_application_cache_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.offlineAppCache;
}

Eina_Bool ewk_view_setting_application_cache_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.offlineAppCache != enable) {
        priv->page->settings().setOfflineWebApplicationCacheEnabled(enable);
        priv->settings.offlineAppCache = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_caret_browsing_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.caretBrowsing;
}

Eina_Bool ewk_view_setting_caret_browsing_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.caretBrowsing != enable) {
        priv->page->settings().setCaretBrowsingEnabled(enable);
        priv->settings.caretBrowsing = enable;
    }
    return true;
}

const char* ewk_view_setting_encoding_custom_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    Evas_Object* main_frame = ewk_view_frame_main_get(ewkView);
    WebCore::Frame* core_frame = EWKPrivate::coreFrame(main_frame);

    String overrideEncoding = core_frame->loader().documentLoader()->overrideEncoding();

    if (overrideEncoding.isEmpty())
        return 0;

    eina_stringshare_replace(&priv->settings.encodingCustom, overrideEncoding.utf8().data());
    return priv->settings.encodingCustom;
}

Eina_Bool ewk_view_setting_encoding_custom_set(Evas_Object* ewkView, const char* encoding)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    Evas_Object* main_frame = ewk_view_frame_main_get(ewkView);
    WebCore::Frame* coreFrame = EWKPrivate::coreFrame(main_frame);
    DBG("%s", encoding);
    if (eina_stringshare_replace(&priv->settings.encodingCustom, encoding))
        coreFrame->loader().reloadWithOverrideEncoding(String::fromUTF8(encoding));
    return true;
}

const char* ewk_view_setting_encoding_default_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.encodingDefault;
}

Eina_Bool ewk_view_setting_encoding_default_set(Evas_Object* ewkView, const char* encoding)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (eina_stringshare_replace(&priv->settings.encodingDefault, encoding))
        priv->page->settings().setDefaultTextEncodingName(String::fromUTF8(encoding));
    return true;
}

Eina_Bool ewk_view_setting_encoding_detector_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.encodingDetector != enable) {
        priv->page->settings().setUsesEncodingDetector(enable);
        priv->settings.encodingDetector = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_encoding_detector_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.encodingDetector;
}

Eina_Bool ewk_view_setting_enable_developer_extras_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.enableDeveloperExtras;
}

Eina_Bool ewk_view_setting_enable_developer_extras_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.enableDeveloperExtras != enable) {
        priv->page->settings().setDeveloperExtrasEnabled(enable);
        priv->settings.enableDeveloperExtras = enable;
    }
    return true;
}

int ewk_view_setting_font_minimum_size_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.fontMinimumSize;
}

Eina_Bool ewk_view_setting_font_minimum_size_set(Evas_Object* ewkView, int size)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (priv->settings.fontMinimumSize != size) {
        priv->page->settings().setMinimumFontSize(size);
        priv->settings.fontMinimumSize = size;
    }
    return true;
}

int ewk_view_setting_font_minimum_logical_size_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.fontMinimumLogicalSize;
}

Eina_Bool ewk_view_setting_font_minimum_logical_size_set(Evas_Object* ewkView, int size)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (priv->settings.fontMinimumLogicalSize != size) {
        priv->page->settings().setMinimumLogicalFontSize(size);
        priv->settings.fontMinimumLogicalSize = size;
    }
    return true;
}

int ewk_view_setting_font_default_size_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.fontDefaultSize;
}

Eina_Bool ewk_view_setting_font_default_size_set(Evas_Object* ewkView, int size)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (priv->settings.fontDefaultSize != size) {
        priv->page->settings().setDefaultFontSize(size);
        priv->settings.fontDefaultSize = size;
    }
    return true;
}

int ewk_view_setting_font_monospace_size_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.fontMonospaceSize;
}

Eina_Bool ewk_view_setting_font_monospace_size_set(Evas_Object* ewkView, int size)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (priv->settings.fontMonospaceSize != size) {
        priv->page->settings().setDefaultFixedFontSize(size);
        priv->settings.fontMonospaceSize = size;
    }
    return true;
}

const char* ewk_view_font_family_name_get(const Evas_Object* ewkView, Ewk_Font_Family fontFamily)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    switch (fontFamily) {
    case EWK_FONT_FAMILY_STANDARD:
        return priv->settings.fontStandard;
    case EWK_FONT_FAMILY_CURSIVE:
        return priv->settings.fontCursive;
    case EWK_FONT_FAMILY_FANTASY:
        return priv->settings.fontFantasy;
    case EWK_FONT_FAMILY_MONOSPACE:
        return priv->settings.fontMonospace;
    case EWK_FONT_FAMILY_SERIF:
        return priv->settings.fontSerif;
    case EWK_FONT_FAMILY_SANS_SERIF:
        return priv->settings.fontSansSerif;
    }
    return 0;
}

Eina_Bool ewk_view_font_family_name_set(Evas_Object* ewkView, Ewk_Font_Family fontFamily, const char* name)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    switch (fontFamily) {
    case EWK_FONT_FAMILY_STANDARD:
        eina_stringshare_replace(&priv->settings.fontStandard, name);
        priv->page->settings().setStandardFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_CURSIVE:
        eina_stringshare_replace(&priv->settings.fontCursive, name);
        priv->page->settings().setCursiveFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_FANTASY:
        eina_stringshare_replace(&priv->settings.fontFantasy, name);
        priv->page->settings().setFantasyFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_MONOSPACE:
        eina_stringshare_replace(&priv->settings.fontMonospace, name);
        priv->page->settings().setFixedFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_SERIF:
        eina_stringshare_replace(&priv->settings.fontSerif, name);
        priv->page->settings().setSerifFontFamily(AtomicString::fromUTF8(name));
        break;
    case EWK_FONT_FAMILY_SANS_SERIF:
        eina_stringshare_replace(&priv->settings.fontSansSerif, name);
        priv->page->settings().setSansSerifFontFamily(AtomicString::fromUTF8(name));
        break;
    default:
        return false;
    }

    return true;
}

Eina_Bool ewk_view_setting_spatial_navigation_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.spatialNavigation;
}

Eina_Bool ewk_view_setting_spatial_navigation_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.spatialNavigation != enable) {
        priv->page->settings().setSpatialNavigationEnabled(enable);
        priv->settings.spatialNavigation = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_local_storage_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.localStorage;
}

Eina_Bool ewk_view_setting_local_storage_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.localStorage != enable) {
        priv->page->settings().setLocalStorageEnabled(enable);
        priv->settings.localStorage = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_page_cache_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.pageCache;
}

Eina_Bool ewk_view_setting_page_cache_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.pageCache != enable) {
        priv->page->settings().setUsesPageCache(enable);
        priv->settings.pageCache = enable;
    }
    return true;
}

const char* ewk_view_setting_local_storage_database_path_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->settings.localStorageDatabasePath;
}

Eina_Bool ewk_view_setting_local_storage_database_path_set(Evas_Object* ewkView, const char* path)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (eina_stringshare_replace(&priv->settings.localStorageDatabasePath, path))
        priv->page->settings().setLocalStorageDatabasePath(String::fromUTF8(path));
    return true;
}

Eina_Bool ewk_view_setting_minimum_timer_interval_set(Evas_Object* ewkView, double interval)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    if (fabs(priv->settings.domTimerInterval - interval) >= std::numeric_limits<double>::epsilon()) {
        priv->page->settings().setMinDOMTimerInterval(interval);
        priv->settings.domTimerInterval = interval;
    }
    return true;
}

double ewk_view_setting_minimum_timer_interval_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);
    return priv->settings.domTimerInterval;
}

Eina_Bool ewk_view_setting_enable_webgl_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.webGLEnabled;
}

Eina_Bool ewk_view_setting_enable_webgl_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.webGLEnabled != enable) {
        priv->page->settings().setWebGLEnabled(enable);
        priv->settings.webGLEnabled = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_include_links_in_focus_chain_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.tabsToLinks;
}

Eina_Bool ewk_view_setting_include_links_in_focus_chain_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    priv->settings.tabsToLinks = enable;
    return true;
}

Eina_Bool ewk_view_setting_enable_hyperlink_auditing_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.hyperlinkAuditingEnabled;
}

Eina_Bool ewk_view_setting_enable_hyperlink_auditing_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.hyperlinkAuditingEnabled != enable) {
        priv->page->settings().setHyperlinkAuditingEnabled(enable);
        priv->settings.hyperlinkAuditingEnabled = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_allow_universal_access_from_file_urls_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.allowUniversalAccessFromFileURLs != enable) {
        priv->page->settings().setAllowUniversalAccessFromFileURLs(enable);
        priv->settings.allowUniversalAccessFromFileURLs = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_allow_universal_access_from_file_urls_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.allowUniversalAccessFromFileURLs;
}

Eina_Bool ewk_view_setting_allow_file_access_from_file_urls_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.allowFileAccessFromFileURLs != enable) {
        priv->page->settings().setAllowFileAccessFromFileURLs(enable);
        priv->settings.allowFileAccessFromFileURLs = enable;
    }
    return true;
}

Eina_Bool ewk_view_setting_allow_file_access_from_file_urls_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.allowFileAccessFromFileURLs;
}

Ewk_View_Smart_Data* ewk_view_smart_data_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    return smartData;
}

/**
 * Gets the internal array of repaint requests.
 *
 * This array should not be modified anyhow. It should be processed
 * immediately as any further ewk_view call might change it, like
 * those that add repaints or flush them, so be sure that your code
 * does not call any of those while you process the repaints,
 * otherwise copy the array.
 *
 * @param priv private handle pointer of the view to get repaints.
 * @param count where to return the number of elements of returned array, may be @c 0.
 *
 * @return reference to array of requested repaints.
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
const Eina_Rectangle* ewk_view_repaints_pop(Ewk_View_Private_Data* priv, size_t* count)
{
    if (count)
        *count = 0;
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv, 0);
    if (count)
        *count = priv->repaints.count;

    priv->repaints.count = 0;

    return priv->repaints.array;
}

/**
 * Add a new repaint request to queue.
 *
 * The repaints are assumed to be relative to current viewport.
 *
 * @param priv private handle pointer of the view to add repaint request.
 * @param x horizontal position relative to current view port (scrolled).
 * @param y vertical position relative to current view port (scrolled).
 * @param width width of area to be repainted
 * @param height height of area to be repainted
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
void ewk_view_repaint_add(Ewk_View_Private_Data* priv, Evas_Coord x, Evas_Coord y, Evas_Coord width, Evas_Coord height)
{
    EINA_SAFETY_ON_NULL_RETURN(priv);
    _ewk_view_repaint_add(priv, x, y, width, height);
}

/**
 * Do layout if required, applied recursively.
 *
 * @param priv private handle pointer of the view to layout.
 *
 * @note this is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
void ewk_view_layout_if_needed_recursive(Ewk_View_Private_Data* priv)
{
    EINA_SAFETY_ON_NULL_RETURN(priv);

    WebCore::FrameView* view = priv->page->mainFrame().view();
    if (!view) {
        ERR("no main frame view");
        return;
    }
    view->updateLayoutAndStyleIfNeededRecursive();
}

/* internal methods ****************************************************/
/**
 * @internal
 * Paints using given graphics context the given area.
 *
 * This uses viewport relative area and will also handle scrollbars
 * and other extra elements. See ewk_view_paint_contents() for the
 * alternative function.
 *
 * @param priv the pointer to the private data of the view to use as paint source
 * @param cr the cairo context to use as paint destination, its state will
 *        be saved before operation and restored afterwards
 * @param area viewport relative geometry to paint
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 *
 * @note This is an easy to use version, but internal structures are
 *       always created, then graphics context is clipped, then
 *       painted, restored and destroyed. This might not be optimum,
 *       so using @a Ewk_Paint_Context may be a better solutions
 *       for large number of operations.
 *
 * @see ewk_view_paint_contents()
 * @see ewk_paint_context_paint()
 *
 * @note This is not for general use but just for subclasses that want
 *       to define their own backing store.
*/
Eina_Bool ewk_view_paint(Ewk_View_Private_Data* priv, Ewk_Paint_Context* context, const Eina_Rectangle* area)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(context, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(area, false);
    WebCore::FrameView* view = priv->page->mainFrame().view();
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, false);

    ewk_paint_context_save(context);
    ewk_paint_context_clip(context, area);
    ewk_paint_context_paint(context, view, area);
    ewk_paint_context_restore(context);

    return true;
}

/**
 * @internal
 * Paints just contents using given graphics context the given area.
 *
 * This uses absolute coordinates for area and will just handle
 * contents, no scrollbars or extras. See ewk_view_paint() for the
 * alternative solution.
 *
 * @param priv the pointer to the private data of the view to use as paint source
 * @param cr the cairo context to use as paint destination, its state will
 *        be saved before operation and restored afterwards
 * @param area absolute geometry to paint
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure
 *
 * @note This is an easy to use version, but internal structures are
 *       always created, then graphics context is clipped, then
 *       painted, restored and destroyed. This might not be optimum,
 *       so using @a Ewk_Paint_Context may be a better solutions
 *       for large number of operations.
 *
 * @see ewk_view_paint()
 * @see ewk_paint_context_paint_contents()
 *
 * @note This is not for general use but just for subclasses that want
 *       to define their own backing store.
 */
Eina_Bool ewk_view_paint_contents(Ewk_View_Private_Data* priv, Ewk_Paint_Context* context, const Eina_Rectangle* area)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(context, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(area, false);
    WebCore::FrameView* view = priv->page->mainFrame().view();
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, false);

    view->updateLayoutAndStyleIfNeededRecursive();

    ewk_paint_context_save(context);
    ewk_paint_context_clip(context, area);
    ewk_paint_context_paint_contents(context, view, area);
    ewk_paint_context_restore(context);

    return true;
}

/**
 * @internal
 * Reports the view is ready to be displayed as all elements are aready.
 *
 * Emits signal: "ready" with no parameters.
 */
void ewk_view_ready(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    evas_object_smart_callback_call(ewkView, "ready", 0);
}

/**
 * @internal
 * Reports the state of input method changed. This is triggered, for example
 * when a input field received/lost focus
 *
 * Emits signal: "inputmethod,changed" with a boolean indicating whether it's
 * enabled or not.
 */
void ewk_view_input_method_state_set(Evas_Object* ewkView, bool active)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);
    WebCore::Frame& focusedFrame = priv->page->focusController().focusedOrMainFrame();

    priv->imh = 0;
    if (focusedFrame.document()
        && focusedFrame.document()->focusedElement()
        && isHTMLInputElement(focusedFrame.document()->focusedElement())) {
        WebCore::HTMLInputElement* inputElement;

        inputElement = static_cast<WebCore::HTMLInputElement*>(focusedFrame.document()->focusedElement());
        if (inputElement) {
            // for password fields, active == false
            if (!active) {
                active = inputElement->isPasswordField();
                priv->imh = inputElement->isPasswordField() * EWK_IMH_PASSWORD;
            } else {
                // Set input method hints for "number", "tel", "email", and "url" input elements.
                priv->imh |= inputElement->isTelephoneField() * EWK_IMH_TELEPHONE;
                priv->imh |= inputElement->isNumberField() * EWK_IMH_NUMBER;
                priv->imh |= inputElement->isEmailField() * EWK_IMH_EMAIL;
                priv->imh |= inputElement->isURLField() * EWK_IMH_URL;
            }
        }
    }

    evas_object_smart_callback_call(ewkView, "inputmethod,changed", (void*)active);
}

/**
 * @internal
 * The view title was changed by the frame loader.
 *
 * Emits signal: "title,changed" with pointer to new title string.
 */
void ewk_view_title_set(Evas_Object* ewkView, const Ewk_Text_With_Direction* title)
{
    DBG("ewkView=%p, title=%s", ewkView, (title && title->string) ? title->string : "(null)");
    evas_object_smart_callback_call(ewkView, "title,changed", (void*)title);
}

/**
 * @internal
 * Reports that main frame's uri changed.
 *
 * Emits signal: "uri,changed" with pointer to the new uri string.
 */
void ewk_view_uri_changed(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    const char* uri = ewk_frame_uri_get(smartData->main_frame);
    DBG("ewkView=%p, uri=%s", ewkView, uri ? uri : "(null)");
    evas_object_smart_callback_call(ewkView, "uri,changed", (void*)uri);
}

/**
 * @internal
 * Reports that a DOM document object has finished loading for @p frame.
 *
 * @param ewkView View which contains the frame.
 * @param frame The frame whose load has triggered the event.
 *
 * Emits signal: "load,document,finished" with @p frame as the parameter.
 */
void ewk_view_load_document_finished(Evas_Object* ewkView, Evas_Object* frame)
{
    evas_object_smart_callback_call(ewkView, "load,document,finished", frame);
}

/**
 * @internal
 * Reports the view started loading something.
 *
 * @param ewkView View.
 * @param ewkFrame Frame being loaded.
 *
 * Emits signal: "load,started" with no parameters.
 */
void ewk_view_load_started(Evas_Object* ewkView, Evas_Object* ewkFrame)
{
    DBG("ewkView=%p, ewkFrame=%p", ewkView, ewkFrame);
    evas_object_smart_callback_call(ewkView, "load,started", ewkFrame);
}

/**
 * Reports the frame started loading something.
 *
 * @param ewkView View.
 *
 * Emits signal: "load,started" on main frame with no parameters.
 */
void ewk_view_frame_main_load_started(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    Evas_Object* frame = ewk_view_frame_main_get(ewkView);
    evas_object_smart_callback_call(frame, "load,started", 0);
}

/**
 * @internal
 * Reports the main frame started provisional load.
 *
 * @param ewkView View.
 *
 * Emits signal: "load,provisional" on View with no parameters.
 */
void ewk_view_load_provisional(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    evas_object_smart_callback_call(ewkView, "load,provisional", 0);
}

/**
 * @internal
 * Reports the main frame provisional load failed.
 *
 * @param ewkView View.
 * @param error Load error.
 *
 * Emits signal: "load,provisional" on View with pointer to Ewk_Frame_Load_Error.
 */
void ewk_view_load_provisional_failed(Evas_Object* ewkView, const Ewk_Frame_Load_Error* error)
{
    DBG("ewkView=%p, error=%p", ewkView, error);
    evas_object_smart_callback_call(ewkView, "load,provisional,failed", const_cast<Ewk_Frame_Load_Error*>(error));
}

/**
 * @internal
 * Reports view can be shown after a new window is created.
 *
 * @param ewkView Frame.
 *
 * Emits signal: "load,newwindow,show" on view with no parameters.
 */
void ewk_view_load_show(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    evas_object_smart_callback_call(ewkView, "load,newwindow,show", 0);
}

/**
 * @internal
 * Reports an onload event for @p frame.
 *
 * @param ewkView View which contains the frame.
 * @param frame The frame whose onload event was received.
 *
 * Emits signal: "onload,event" with @p frame as the parameter.
 */
void ewk_view_onload_event(Evas_Object* ewkView, Evas_Object* frame)
{
    evas_object_smart_callback_call(ewkView, "onload,event", frame);
}

/**
 * @internal
 * Reports the main frame was cleared.
 *
 * @param ewkView View.
 */
void ewk_view_frame_main_cleared(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->flush);
    smartData->api->flush(smartData);

    ewk_view_mixed_content_displayed_set(ewkView, false);
    ewk_view_mixed_content_run_set(ewkView, false);
}

/**
 * @internal
 * Reports the main frame received an icon.
 *
 * @param ewkView View.
 *
 * Emits signal: "icon,received" with no parameters.
 */
void ewk_view_frame_main_icon_received(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    Evas_Object* frame = ewk_view_frame_main_get(ewkView);
    evas_object_smart_callback_call(frame, "icon,received", 0);
}

/**
 * @internal
 * Reports load finished, optionally with error information.
 *
 * Emits signal: "load,finished" with pointer to #Ewk_Frame_Load_Error
 * if any error, or @c 0 if successful load.
 *
 * @note there should not be any error stuff here, but trying to be
 *       compatible with previous WebKit.
 */
void ewk_view_load_finished(Evas_Object* ewkView, const Ewk_Frame_Load_Error* error)
{
    DBG("ewkView=%p, error=%p", ewkView, error);
    evas_object_smart_callback_call(ewkView, "load,finished", (void*)error);
}

/**
 * @internal
 * Reports load failed with error information.
 *
 * Emits signal: "load,error" with pointer to Ewk_Frame_Load_Error.
 */
void ewk_view_load_error(Evas_Object* ewkView, const Ewk_Frame_Load_Error* error)
{
    DBG("ewkView=%p, error=%p", ewkView, error);
    evas_object_smart_callback_call(ewkView, "load,error", (void*)error);
}

/**
 * @internal
 * Reports load progress changed.
 *
 * Emits signal: "load,progress" with pointer to a double from 0.0 to 1.0.
 */
void ewk_view_load_progress_changed(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    // Evas_Coord width, height;
    double progress = priv->page->progress().estimatedProgress();

    DBG("ewkView=%p (p=%0.3f)", ewkView, progress);

    evas_object_smart_callback_call(ewkView, "load,progress", &progress);
}

/**
 * @internal
 * Reports view @param ewkView should be restored to default conditions
 *
 * @param ewkView View.
 * @param frame Frame that originated restore.
 *
 * Emits signal: "restore" with frame.
 */
void ewk_view_restore_state(Evas_Object* ewkView, Evas_Object* frame)
{
    evas_object_smart_callback_call(ewkView, "restore", frame);
}

/**
 * @internal
 * Delegates to browser the creation of a new window. If it is not implemented,
 * current view is returned, so navigation might continue in same window. If
 * browser supports the creation of new windows, a new Ewk_Window_Features is
 * created and passed to browser. If it intends to keep the request for opening
 * the window later it must increments the Ewk_Winwdow_Features ref count by
 * calling ewk_window_features_ref(window_features). Otherwise this struct will
 * be freed after returning to this function.
 *
 * @param ewkView Current view.
 * @param javascript @c true if the new window is originated from javascript,
 * @c false otherwise
 * @param window_features Features of the new window being created. If it's @c
 * 0, it will be created a window with default features.
 *
 * @return New view, in case smart class implements the creation of new windows;
 * else, current view @param ewkView or @c 0 on failure.
 *
 * @see ewk_window_features_ref().
 */
Evas_Object* ewk_view_window_create(Evas_Object* ewkView, bool javascript, const WebCore::WindowFeatures* coreFeatures)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);

    if (!smartData->api->window_create)
        return ewkView;

    Ewk_Window_Features* windowFeatures = ewk_window_features_new_from_core(coreFeatures);
    if (!windowFeatures)
        return 0;

    Evas_Object* view = smartData->api->window_create(smartData, javascript, windowFeatures);
    ewk_window_features_unref(windowFeatures);

    return view;
}

/**
 * @internal
 * Reports a window should be closed. It's client responsibility to decide if
 * the window should in fact be closed. So, if only windows created by javascript
 * are allowed to be closed by this call, browser needs to save the javascript
 * flag when the window is created. Since a window can close itself (for example
 * with a 'self.close()' in Javascript) browser must postpone the deletion to an
 * idler.
 *
 * @param ewkView View to be closed.
 */
void ewk_view_window_close(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    ewk_view_stop(ewkView);
    if (!smartData->api->window_close)
        return;
    smartData->api->window_close(smartData);
}

/**
 * @internal
 * Reports mouse has moved over a link.
 *
 * Emits signal: "link,hover,in"
 */
void ewk_view_mouse_link_hover_in(Evas_Object* ewkView, void* data)
{
    evas_object_smart_callback_call(ewkView, "link,hover,in", data);
}

/**
 * @internal
 * Reports mouse is not over a link anymore.
 *
 * Emits signal: "link,hover,out"
 */
void ewk_view_mouse_link_hover_out(Evas_Object* ewkView)
{
    evas_object_smart_callback_call(ewkView, "link,hover,out", 0);
}

/**
 * @internal
 * Set toolbar visible.
 *
 * Emits signal: "toolbars,visible,set" with a pointer to a boolean.
 */
void ewk_view_toolbars_visible_set(Evas_Object* ewkView, bool visible)
{
    DBG("ewkView=%p (visible=%d)", ewkView, !!visible);
    evas_object_smart_callback_call(ewkView, "toolbars,visible,set", &visible);
}

/**
 * @internal
 * Get toolbar visibility.
 *
 * @param ewkView View.
 * @param visible boolean pointer in which to save the result. It defaults
 * to @c FALSE, i.e. if browser does no listen to emitted signal, it means
 * there are no toolbars and therefore they are not visible.
 *
 * Emits signal: "toolbars,visible,get" with a pointer to a boolean.
 */
void ewk_view_toolbars_visible_get(Evas_Object* ewkView, bool* visible)
{
    DBG("%s, ewkView=%p", __func__, ewkView);
    *visible = false;
    evas_object_smart_callback_call(ewkView, "toolbars,visible,get", visible);
}

/**
 * @internal
 * Set statusbar visible.
 *
 * @param ewkView View.
 * @param visible @c TRUE if statusbar are visible, @c FALSE otherwise.
 *
 * Emits signal: "statusbar,visible,set" with a pointer to a boolean.
 */
void ewk_view_statusbar_visible_set(Evas_Object* ewkView, bool  visible)
{
    DBG("ewkView=%p (visible=%d)", ewkView, !!visible);
    evas_object_smart_callback_call(ewkView, "statusbar,visible,set", &visible);
}

/**
 * @internal
 * Get statusbar visibility.
 *
 * @param ewkView View.
 * @param visible boolean pointer in which to save the result. It defaults
 * to @c FALSE, i.e. if browser does no listen to emitted signal, it means
 * there is no statusbar and therefore it is not visible.
 *
 * Emits signal: "statusbar,visible,get" with a pointer to a boolean.
 */
void ewk_view_statusbar_visible_get(Evas_Object* ewkView, bool* visible)
{
    DBG("%s, ewkView=%p", __func__, ewkView);
    *visible = false;
    evas_object_smart_callback_call(ewkView, "statusbar,visible,get", visible);
}

/**
 * @internal
 * Set text of statusbar
 *
 * @param ewkView View.
 * @param text New text to put on statusbar.
 *
 * Emits signal: "statusbar,text,set" with a string.
 */
void ewk_view_statusbar_text_set(Evas_Object* ewkView, const char* text)
{
    DBG("ewkView=%p (text=%s)", ewkView, text);
    INFO("status bar text set: %s", text);
    evas_object_smart_callback_call(ewkView, "statusbar,text,set", (void*)text);
}

/**
 * @internal
 * Set scrollbars visible.
 *
 * @param ewkView View.
 * @param visible @c TRUE if scrollbars are visible, @c FALSE otherwise.
 *
 * Emits signal: "scrollbars,visible,set" with a pointer to a boolean.
 */
void ewk_view_scrollbars_visible_set(Evas_Object* ewkView, bool visible)
{
    DBG("ewkView=%p (visible=%d)", ewkView, !!visible);
    evas_object_smart_callback_call(ewkView, "scrollbars,visible,set", &visible);
}

/**
 * @internal
 * Get scrollbars visibility.
 *
 * @param ewkView View.
 * @param visible boolean pointer in which to save the result. It defaults
 * to @c FALSE, i.e. if browser does no listen to emitted signal, it means
 * there are no scrollbars and therefore they are not visible.
 *
 * Emits signal: "scrollbars,visible,get" with a pointer to a boolean.
 */
void ewk_view_scrollbars_visible_get(Evas_Object* ewkView, bool* visible)
{
    DBG("%s, ewkView=%p", __func__, ewkView);
    *visible = false;
    evas_object_smart_callback_call(ewkView, "scrollbars,visible,get", visible);
}

/**
 * @internal
 * Set menubar visible.
 *
 * @param ewkView View.
 * @param visible @c TRUE if menubar is visible, @c FALSE otherwise.
 *
 * Emits signal: "menubar,visible,set" with a pointer to a boolean.
 */
void ewk_view_menubar_visible_set(Evas_Object* ewkView, bool visible)
{
    DBG("ewkView=%p (visible=%d)", ewkView, !!visible);
    evas_object_smart_callback_call(ewkView, "menubar,visible,set", &visible);
}

/**
 * @internal
 * Get menubar visibility.
 *
 * @param ewkView View.
 * @param visible boolean pointer in which to save the result. It defaults
 * to @c FALSE, i.e. if browser does no listen to emitted signal, it means
 * there is no menubar and therefore it is not visible.
 *
 * Emits signal: "menubar,visible,get" with a pointer to a boolean.
 */
void ewk_view_menubar_visible_get(Evas_Object* ewkView, bool* visible)
{
    DBG("%s, ewkView=%p", __func__, ewkView);
    *visible = false;
    evas_object_smart_callback_call(ewkView, "menubar,visible,get", visible);
}

/**
 * @internal
 */
void ewk_view_tooltip_text_set(Evas_Object* ewkView, const char* text)
{
    DBG("ewkView=%p text=%s", ewkView, text);
    if (text && *text)
        evas_object_smart_callback_call(ewkView, "tooltip,text,set", (void*)text);
    else
        evas_object_smart_callback_call(ewkView, "tooltip,text,unset", 0);
}

/**
 * @internal
 *
 * @param ewkView View.
 * @param message String to show on console.
 * @param lineNumber Line number.
 * @sourceID Source id.
 *
 */
void ewk_view_add_console_message(Evas_Object* ewkView, const char* message, unsigned int lineNumber, const char* sourceID)
{
    DBG("ewkView=%p message=%s lineNumber=%u sourceID=%s", ewkView, message, lineNumber, sourceID);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->add_console_message);
    smartData->api->add_console_message(smartData, message, lineNumber, sourceID);
}

bool ewk_view_focus_can_cycle(Evas_Object* ewkView, Ewk_Focus_Direction direction)
{
    DBG("ewkView=%p direction=%d", ewkView, direction);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!smartData->api->focus_can_cycle)
        return false;

    return smartData->api->focus_can_cycle(smartData, direction);
}

void ewk_view_run_javascript_alert(Evas_Object* ewkView, Evas_Object* frame, const char* message)
{
    DBG("ewkView=%p frame=%p message=%s", ewkView, frame, message);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);

    if (!smartData->api->run_javascript_alert)
        return;

    smartData->api->run_javascript_alert(smartData, frame, message);
}

bool ewk_view_run_javascript_confirm(Evas_Object* ewkView, Evas_Object* frame, const char* message)
{
    DBG("ewkView=%p frame=%p message=%s", ewkView, frame, message);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!smartData->api->run_javascript_confirm)
        return false;

    return smartData->api->run_javascript_confirm(smartData, frame, message);
}

bool ewk_view_run_before_unload_confirm(Evas_Object* ewkView, Evas_Object* frame, const char* message)
{
    DBG("ewkView=%p frame=%p message=%s", ewkView, frame, message);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!smartData->api->run_before_unload_confirm)
        return false;

    return smartData->api->run_before_unload_confirm(smartData, frame, message);
}

bool ewk_view_run_javascript_prompt(Evas_Object* ewkView, Evas_Object* frame, const char* message, const char* defaultValue, const char** value)
{
    DBG("ewkView=%p frame=%p message=%s", ewkView, frame, message);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!smartData->api->run_javascript_prompt)
        return false;

    return smartData->api->run_javascript_prompt(smartData, frame, message, defaultValue, value);
}

/**
 * @internal
 * Delegates to client to decide whether a script must be stopped because it's
 * running for too long. If client does not implement it, it goes to default
 * implementation, which logs and returns false. Client may remove log by
 * setting this function 0, which will just return false.
 *
 * @param ewkView View.
 *
 * @return @c true if script should be stopped; @c false otherwise
 */
bool ewk_view_should_interrupt_javascript(Evas_Object* ewkView)
{
    DBG("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!smartData->api->should_interrupt_javascript)
        return false;

    return smartData->api->should_interrupt_javascript(smartData);
}

/**
 * @internal
 * This is called whenever the application is asking to store data to the cache and the
 * quota allocated to that application is exceeded. Browser may use this to increase the
 * size of quota before the originating operation fails.
 *
 * @param ewkView View.
 * @param origin Security origin.
 * @param defaultOriginQuota Default quota for origin.
 * @param totalSpaceNeeded The total space needed in the cache in order to fulfill
 * application's requirement.
 */
int64_t ewk_view_exceeded_application_cache_quota(Evas_Object* ewkView, Ewk_Security_Origin *origin, int64_t defaultOriginQuota, int64_t totalSpaceNeeded)
{
    DBG("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, 0);
    if (!smartData->api->exceeded_application_cache_quota)
        return 0;

    INFO("defaultOriginQuota=%" PRIu64 " totalSpaceNeeded=%" PRIu64, defaultOriginQuota, totalSpaceNeeded);
    return smartData->api->exceeded_application_cache_quota(smartData, origin, defaultOriginQuota, totalSpaceNeeded);
}

/**
 * @internal
 * This is called whenever the web site shown in @param frame is asking to store data
 * to the database @param databaseName and the quota allocated to that web site
 * is exceeded. Browser may use this to increase the size of quota before the
 * originating operationa fails.
 *
 * @param ewkView View.
 * @param frame The frame whose web page exceeded its database quota.
 * @param databaseName Database name.
 * @param currentSize Current size of this database
 * @param expectedSize The expected size of this database in order to fulfill
 * site's requirement.
 */
uint64_t ewk_view_exceeded_database_quota(Evas_Object* ewkView, Evas_Object* frame, const char* databaseName, uint64_t currentSize, uint64_t expectedSize)
{
    DBG("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, 0);
    if (!smartData->api->exceeded_database_quota)
        return 0;

    INFO("currentSize=%" PRIu64 " expectedSize=%" PRIu64, currentSize, expectedSize);
    return smartData->api->exceeded_database_quota(smartData, frame, databaseName, currentSize, expectedSize);
}

/**
 * @internal
 * Open panel to choose a file.
 *
 * @param ewkView View.
 * @param frame Frame in which operation is required.
 * @param allowsMultipleFiles @c true when more than one file may be selected, @c false otherwise.
 * @param acceptMIMETypes List of accepted mime types. It is passed to child objects as an Eina_List of char pointers that is freed automatically.
 * @param selectedFilenames List of files selected.
 *
 * @return @false if user canceled file selection; @true if confirmed.
 */
bool ewk_view_run_open_panel(Evas_Object* ewkView, Evas_Object* frame, Ewk_File_Chooser* fileChooser, Eina_List** selectedFilenames)
{
    DBG("ewkView=%p frame=%p allows_multiple_files=%d", ewkView, frame, ewk_file_chooser_allows_multiple_files_get(fileChooser));
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!smartData->api->run_open_panel)
        return false;

    *selectedFilenames = 0;
    bool confirm = smartData->api->run_open_panel(smartData, frame, fileChooser, selectedFilenames);    
    if (!confirm && *selectedFilenames)
        ERR("Canceled file selection, but selected filenames != 0. Free names before return.");

    return confirm;
}

void ewk_view_repaint(Evas_Object* ewkView, Evas_Coord x, Evas_Coord y, Evas_Coord width, Evas_Coord height)
{
    DBG("ewkView=%p, region=%d,%d + %dx%d", ewkView, x, y, width, height);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    _ewk_view_repaint_add(priv, x, y, width, height);
    _ewk_view_smart_changed(smartData);
}

void ewk_view_scroll(Evas_Object* ewkView, const WebCore::IntSize& delta, const WebCore::IntRect& rectToScroll, const WebCore::IntRect&)
{
    ASSERT(!rectToScroll.isEmpty());
    ASSERT(delta.width() || delta.height());

    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    ewk_view_mark_for_sync(ewkView);
}

/**
 * @internal
 *
 * Marked the change to call frameRectsChanged.
 */
void ewk_view_frame_rect_changed(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    smartData->changed.frame_rect = true;
    _ewk_view_smart_changed(smartData);
}

WTF::PassRefPtr<WebCore::Widget> ewk_view_plugin_create(Evas_Object* ewkView, Evas_Object* frame, const WebCore::IntSize& pluginSize, WebCore::HTMLPlugInElement* element, const WebCore::URL& url, const WTF::Vector<WTF::String>& paramNames, const WTF::Vector<WTF::String>& paramValues, const WTF::String& mimeType, bool loadManually)
{
    DBG("ewkView=%p, frame=%p, size=%dx%d, element=%p, url=%s, mimeType=%s",
        ewkView, frame, pluginSize.width(), pluginSize.height(), element,
        url.string().utf8().data(), mimeType.utf8().data());

    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    smartData->changed.frame_rect = true;
    _ewk_view_smart_changed(smartData);

    return ewk_frame_plugin_create
               (frame, pluginSize, element, url, paramNames, paramValues,
               mimeType, loadManually);
}

/**
 * @internal
 *
 * Creates a new popup with options when a select widget was clicked.
 *
 * @param client PopupMenuClient instance that allows communication with webkit.
 * @param selected Selected item.
 * @param rect Menu's position.
 *
 * Emits: "popup,create" with a list of Ewk_Menu containing each item's data
 */
void ewk_view_popup_new(Evas_Object* ewkView, WebCore::PopupMenuClient* client, int /*selected*/, const WebCore::IntRect& rect)
{
    INFO("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    if (priv->popup.menuClient)
        ewk_view_popup_destroy(ewkView);

    priv->popup.menuClient = client;

    // populate items
    const int size = client->listSize();
    for (int i = 0; i < size; ++i) {
        Ewk_Menu_Item* item = new Ewk_Menu_Item;
        if (client->itemIsSeparator(i))
            item->type = EWK_MENU_SEPARATOR;
        else if (client->itemIsLabel(i))
            item->type = EWK_MENU_GROUP;
        else
            item->type = EWK_MENU_OPTION;
        item->text = eina_stringshare_add(client->itemText(i).utf8().data());

        priv->popup.menu.items = eina_list_append(priv->popup.menu.items, item);
    }

    priv->popup.menu.x = rect.x();
    priv->popup.menu.y = rect.y();
    priv->popup.menu.width = rect.width();
    priv->popup.menu.height = rect.height();
    evas_object_smart_callback_call(ewkView, "popup,create", &priv->popup.menu);
}

Eina_Bool ewk_view_popup_destroy(Evas_Object* ewkView)
{
    INFO("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    if (!priv->popup.menuClient)
        return false;

    evas_object_smart_callback_call(ewkView, "popup,willdelete", &priv->popup.menu);

    void* itemv;
    EINA_LIST_FREE(priv->popup.menu.items, itemv) {
        Ewk_Menu_Item* item = static_cast<Ewk_Menu_Item*>(itemv);
        eina_stringshare_del(item->text);
        delete item;
    }
    priv->popup.menuClient->popupDidHide();
    priv->popup.menuClient = 0;

    return true;
}

void ewk_view_popup_selected_set(Evas_Object* ewkView, int index)
{
    INFO("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    EINA_SAFETY_ON_NULL_RETURN(priv->popup.menuClient);

    priv->popup.menuClient->valueChanged(index);
}

#if ENABLE(INPUT_TYPE_COLOR)
/**
 * @internal
 *
 * Creates a new color chooser with an initial selected color.
 *
 * @param client ColorChooserClient instance that allows communication with webkit.
 * @param initialColor The initial selected color.
 *
 */
void ewk_view_color_chooser_new(Evas_Object* ewkView, WebCore::ColorChooserClient* client, const WebCore::Color& initialColor)
{
    INFO("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    if (priv->colorChooserClient)
        ewk_view_color_chooser_destroy(ewkView);

    priv->colorChooserClient = client;

    Ewk_Color color;
    color.r = initialColor.red();
    color.g = initialColor.green();
    color.b = initialColor.blue();
    color.a = initialColor.alpha();

    evas_object_smart_callback_call(ewkView, "colorchooser,create", &color);
}

Eina_Bool ewk_view_color_chooser_destroy(Evas_Object* ewkView)
{
    INFO("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    if (!priv->colorChooserClient)
        return false;

    evas_object_smart_callback_call(ewkView, "colorchooser,willdelete", 0);

    priv->colorChooserClient->didEndChooser();
    priv->colorChooserClient = 0;

    return true;
}

void ewk_view_color_chooser_color_set(Evas_Object *ewkView, int r, int g, int b)
{
    INFO("ewkView=%p", ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    EINA_SAFETY_ON_NULL_RETURN(priv->colorChooserClient);

    // Alpha channel is not allowed, see ColorInputType::sanitizeValue().
    priv->colorChooserClient->didChooseColor(WebCore::Color(r, g, b));
}

/**
 * @internal
 *
 * The selected color of the color input associated with the color chooser has
 * changed. Usually the browser should updated the current selected value on the
 * color picker if the user is not interacting.
 *
 * @param newColor The new selected color.
 *
 */
void ewk_view_color_chooser_changed(Evas_Object* ewkView, const WebCore::Color& newColor)
{
    INFO("ewkView=%p", ewkView);

    Ewk_Color color;
    color.r = newColor.red();
    color.g = newColor.green();
    color.b = newColor.blue();
    color.a = newColor.alpha();

    evas_object_smart_callback_call(ewkView, "colorchooser,color,changed", &color);
}
#endif

/**
 * @internal
 * Request a download to user.
 *
 * @param ewkView View.
 * @oaram download Ewk_Download struct to be sent.
 *
 * Emits: "download,request" with an Ewk_Download containing the details of the
 * requested download. The download per se must be handled outside of webkit.
 */
void ewk_view_download_request(Evas_Object* ewkView, Ewk_Download* download)
{
    DBG("ewkView=%p", ewkView);
    evas_object_smart_callback_call(ewkView, "download,request", download);
}

#if ENABLE(NETSCAPE_PLUGIN_API)
/**
 * @internal
 * Reports the JS window object was cleared.
 *
 * @param ewkView view.
 * @param frame the frame.
 */
void ewk_view_js_window_object_clear(Evas_Object* ewkView, Evas_Object* frame)
{
    evas_object_smart_callback_call(ewkView, "js,windowobject,clear", frame);
}
#endif

/**
 * @internal
 * Reports the viewport has changed.
 *
 * @param arguments viewport argument.
 *
 * Emits signal: "viewport,changed" with no parameters.
 */
void ewk_view_viewport_attributes_set(Evas_Object* ewkView, const WebCore::ViewportArguments& arguments)
{
    EWK_VIEW_SD_GET(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);

    priv->viewportArguments = arguments;
    evas_object_smart_callback_call(ewkView, "viewport,changed", 0);
}

void ewk_view_viewport_attributes_get(const Evas_Object* ewkView, int* width, int* height, float* initScale, float* maxScale, float* minScale, float* devicePixelRatio, Eina_Bool* userScalable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    WebCore::ViewportAttributes attributes = _ewk_view_viewport_attributes_compute(priv);

    if (width)
        *width = attributes.layoutSize.width();
    if (height)
        *height = attributes.layoutSize.height();
    if (initScale)
        *initScale = attributes.initialScale;
    if (maxScale)
        *maxScale = attributes.maximumScale;
    if (minScale)
        *minScale = attributes.minimumScale;
    if (devicePixelRatio)
        *devicePixelRatio = priv->page->deviceScaleFactor();
    if (userScalable)
        *userScalable = static_cast<bool>(attributes.userScalable);
}

Eina_Bool ewk_view_zoom_range_set(Evas_Object* ewkView, float minScale, float maxScale)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    if (maxScale < minScale) {
        WARN("minScale is larger than maxScale");
        return false;
    }

    priv->settings.zoomRange.minScale = minScale;
    priv->settings.zoomRange.maxScale = maxScale;

    return true;
}

float ewk_view_zoom_range_min_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);

    return priv->settings.zoomRange.minScale;
}

float ewk_view_zoom_range_max_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);

    return priv->settings.zoomRange.maxScale;
}

Eina_Bool ewk_view_user_scalable_set(Evas_Object* ewkView, Eina_Bool userScalable)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->settings.zoomRange.userScalable = userScalable;

    return true;
}

Eina_Bool ewk_view_user_scalable_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    return priv->settings.zoomRange.userScalable;
}

Eina_Bool ewk_view_device_pixel_ratio_set(Evas_Object* ewkView, float ratio)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->page->setDeviceScaleFactor(ratio);

    return true;
}

float ewk_view_device_pixel_ratio_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, -1.0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, -1.0);

    return priv->page->deviceScaleFactor();
}

void ewk_view_text_direction_set(Evas_Object* ewkView, Ewk_Text_Direction direction)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    // The Editor::setBaseWritingDirection() function checks if we can change
    // the text direction of the selected node and updates its DOM "dir"
    // attribute and its CSS "direction" property.
    // So, we just call the function as Safari does.
    WebCore::Frame& focusedFrame = priv->page->focusController().focusedOrMainFrame();

    WebCore::Editor& editor = focusedFrame.editor();
    if (!editor.canEdit())
        return;

    switch (direction) {
    case EWK_TEXT_DIRECTION_DEFAULT:
        editor.setBaseWritingDirection(NaturalWritingDirection);
    case EWK_TEXT_DIRECTION_LEFT_TO_RIGHT:
        editor.setBaseWritingDirection(LeftToRightWritingDirection);
    case EWK_TEXT_DIRECTION_RIGHT_TO_LEFT:
        editor.setBaseWritingDirection(RightToLeftWritingDirection);
    }

    ASSERT_NOT_REACHED();
}

void ewk_view_did_first_visually_nonempty_layout(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);
    if (!priv->flags.viewCleared) {
        ewk_view_frame_main_cleared(ewkView);
        ewk_view_enable_render(ewkView);
        priv->flags.viewCleared = true;
    }
}

/**
 * @internal
 * Dispatch finished loading.
 *
 * @param ewkView view.
 */
void ewk_view_dispatch_did_finish_loading(Evas_Object* ewkView)
{
    /* If we reach this point and rendering is still disabled, WebCore will not
     * trigger the didFirstVisuallyNonEmptyLayout signal anymore. So, we
     * forcefully re-enable the rendering.
     */
    ewk_view_did_first_visually_nonempty_layout(ewkView);
}

void ewk_view_transition_to_commited_for_newpage(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    ewk_view_disable_render(ewkView);
    priv->flags.viewCleared = false;
}


/**
 * @internal
 * Reports that a navigation policy decision should be taken. If @return
 * is true, the navigation request will be accepted, otherwise it will be
 * ignored.
 *
 * @param ewkView View to load
 * @param request Request which contain url to navigate
 * @param navigationType navigation type
 *
 * @return true if the client accepted the navigation request, false otherwise. If the
 * client did not make a decision, we return true by default since the default policy
 * is to accept.
 */
bool ewk_view_navigation_policy_decision(Evas_Object* ewkView, Ewk_Frame_Resource_Request* request, WebCore::NavigationType navigationType)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(smartData->api, false);

    if (!smartData->api->navigation_policy_decision)
        return true;

    Ewk_Navigation_Type type;
    switch (navigationType) {
    case WebCore::NavigationTypeLinkClicked:
        type = EWK_NAVIGATION_TYPE_LINK_CLICKED;
    case WebCore::NavigationTypeFormSubmitted:
        type = EWK_NAVIGATION_TYPE_FORM_SUBMITTED;
    case WebCore::NavigationTypeBackForward:
        type = EWK_NAVIGATION_TYPE_BACK_FORWARD;
    case WebCore::NavigationTypeReload:
        type = EWK_NAVIGATION_TYPE_RELOAD;
    case WebCore::NavigationTypeFormResubmitted:
        type = EWK_NAVIGATION_TYPE_FORM_RESUBMITTED;
    case WebCore::NavigationTypeOther:
        type = EWK_NAVIGATION_TYPE_OTHER;
    }

    return smartData->api->navigation_policy_decision(smartData, request, type);
}

Eina_Bool ewk_view_js_object_add(Evas_Object* ewkView, Ewk_JS_Object* object, const char* objectName)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    if (object->view) // object has already been added to another ewk_view
        return false;
    object->name = eina_stringshare_add(objectName);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    WebCore::JSDOMWindow* window = toJSDOMWindow(&priv->page->mainFrame(), WebCore::mainThreadNormalWorld());
    JSC::JSLockHolder lock(window->globalExec());
    JSC::Bindings::RootObject* root;
    root = priv->page->mainFrame().script().bindingRootObject();

    if (!window) {
        ERR("Warning: couldn't get window object");
        return false;
    }

    JSC::ExecState* executeState = window->globalExec();

    object->view = ewkView;
    JSC::JSObject* runtimeObject = (JSC::JSObject*)JSC::Bindings::CInstance::create((NPObject*)object, root)->createRuntimeObject(executeState);
    JSC::Identifier id = JSC::Identifier(executeState, objectName);

    JSC::PutPropertySlot slot(window);
    window->methodTable()->put(window, executeState, id, runtimeObject, slot);
    return true;
#else
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(object);
    UNUSED_PARAM(objectName);
    return false;
#endif // ENABLE(NETSCAPE_PLUGIN_API)
}

/**
 * @internal
 * Reports that the contents have resized. The ewk_view calls contents_resize,
 * which can be reimplemented as needed.
 *
 * @param ewkView view.
 * @param width new content width.
 * @param height new content height.
 */
void ewk_view_contents_size_changed(Evas_Object* ewkView, int width, int height)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api);
    EINA_SAFETY_ON_NULL_RETURN(smartData->api->contents_resize);

    if (!smartData->api->contents_resize(smartData, width, height))
        ERR("failed to resize contents to %dx%d", width, height);
}

/**
 * @internal
 * Gets page size from frameview.
 *
 * @param ewkView view.
 *
 * @return page size, or -1.0 size on failure
 */
WebCore::FloatRect ewk_view_page_rect_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, WebCore::FloatRect(-1.0, -1.0, -1.0, -1.0));
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, WebCore::FloatRect(-1.0, -1.0, -1.0, -1.0));

    return priv->page->mainFrame().view()->frameRect();
}

#if ENABLE(TOUCH_EVENTS)
void ewk_view_need_touch_events_set(Evas_Object* ewkView, bool needed)
{
    EWK_VIEW_SD_GET(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);

    priv->flags.needTouchEvents = needed;
}

bool ewk_view_need_touch_events_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->flags.needTouchEvents;
}
#endif

inline static WebCore::Page::ViewMode toViewMode(Ewk_View_Mode viewMode)
{
    switch (viewMode) {
    case EWK_VIEW_MODE_INVALID:
        return WebCore::Page::ViewModeInvalid;
    case EWK_VIEW_MODE_WINDOWED:
        return WebCore::Page::ViewModeWindowed;
    case EWK_VIEW_MODE_FLOATING:
        return WebCore::Page::ViewModeFloating;
    case EWK_VIEW_MODE_FULLSCREEN:
        return WebCore::Page::ViewModeFullscreen;
    case EWK_VIEW_MODE_MAXIMIZED:
        return WebCore::Page::ViewModeMaximized;
    case EWK_VIEW_MODE_MINIMIZED:
        return WebCore::Page::ViewModeMinimized;
    }
    ASSERT_NOT_REACHED();

    return WebCore::Page::ViewModeInvalid;
}

inline static Ewk_View_Mode toEwkViewMode(WebCore::Page::ViewMode viewMode)
{
    switch (viewMode) {
    case WebCore::Page::ViewModeInvalid:
        return EWK_VIEW_MODE_INVALID;
    case WebCore::Page::ViewModeWindowed:
        return EWK_VIEW_MODE_WINDOWED;
    case WebCore::Page::ViewModeFloating:
        return EWK_VIEW_MODE_FLOATING;
    case WebCore::Page::ViewModeFullscreen:
        return EWK_VIEW_MODE_FULLSCREEN;
    case WebCore::Page::ViewModeMaximized:
        return EWK_VIEW_MODE_MAXIMIZED;
    case WebCore::Page::ViewModeMinimized:
        return EWK_VIEW_MODE_MINIMIZED;
    }
    ASSERT_NOT_REACHED();

    return EWK_VIEW_MODE_INVALID;
}

Eina_Bool ewk_view_mode_set(Evas_Object* ewkView, Ewk_View_Mode viewMode)
{
#if ENABLE(VIEW_MODE_CSS_MEDIA)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->page->setViewMode(toViewMode(viewMode));
    return true;
#else
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(viewMode);
    return false;
#endif
}

Ewk_View_Mode ewk_view_mode_get(const Evas_Object* ewkView)
{
#if ENABLE(VIEW_MODE_CSS_MEDIA)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, EWK_VIEW_MODE_INVALID);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, EWK_VIEW_MODE_INVALID);

    return toEwkViewMode(priv->page->viewMode());
#else
    UNUSED_PARAM(ewkView);
    return EWK_VIEW_MODE_INVALID;
#endif
}

Eina_Bool ewk_view_mixed_content_displayed_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->flags.hasDisplayedMixedContent;
}

Eina_Bool ewk_view_mixed_content_run_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->flags.hasRunMixedContent;
}

/**
 * @internal
 * Reports the view that editor client selection has changed.
 *
 * @param ewkView View.
 *
 * Emits signal: "editorclientselection,changed" with no parameters.
 */
void ewk_view_editor_client_selection_changed(Evas_Object* ewkView)
{
    evas_object_smart_callback_call(ewkView, "editorclient,selection,changed", 0);
}

/**
 * @internal
 * Reports to the view that editor client's contents were changed.
 *
 * @param ewkView View.
 *
 * Emits signal: "editorclient,contents,changed" with no parameters.
 */
void ewk_view_editor_client_contents_changed(Evas_Object* ewkView)
{
    evas_object_smart_callback_call(ewkView, "editorclient,contents,changed", 0);
}

/**
 * @internal
 * Defines whether the view has displayed mixed content.
 *
 * When a view has displayed mixed content, any of its frames has loaded an HTTPS URI
 * which has itself loaded and displayed a resource (such as an image) from an insecure,
 * that is, non-HTTPS, URI.
 *
 * @param hasDisplayed Do or do not clear the flag from the view.
 *
 * Emits signal: "mixedcontent,displayed" with no parameters when @p hasDisplayed is @c true.
 */
void ewk_view_mixed_content_displayed_set(Evas_Object* ewkView, bool hasDisplayed)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    priv->flags.hasDisplayedMixedContent = hasDisplayed;

    if (hasDisplayed)
        evas_object_smart_callback_call(ewkView, "mixedcontent,displayed", 0);
}

/**
 * @internal
 * Defines whether the view has run mixed content.
 *
 * When a view has run mixed content, any of its frames has loaded an HTTPS URI
 * which has itself loaded and run a resource (such as a script) from an insecure,
 * that is, non-HTTPS, URI.
 *
 * @param hasRun Do or do not clear the flag from the view.
 *
 * Emits signal: "mixedcontent,run" with no parameters when @p hasRun is @c true.
 */
void ewk_view_mixed_content_run_set(Evas_Object* ewkView, bool hasRun)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    priv->flags.hasRunMixedContent = hasRun;

    if (hasRun)
        evas_object_smart_callback_call(ewkView, "mixedcontent,run", 0);
}

Eina_Bool ewk_view_visibility_state_set(Evas_Object* ewkView, Ewk_Page_Visibility_State pageVisibilityState, Eina_Bool)
{
#if ENABLE(PAGE_VISIBILITY_API)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    // WebKit does not curently support the "unloaded" state.
    if (pageVisibilityState == EWK_PAGE_VISIBILITY_STATE_UNLOADED)
        return false;

    priv->page->setIsVisible(pageVisibilityState == EWK_PAGE_VISIBILITY_STATE_VISIBLE);
    if (pageVisibilityState == EWK_PAGE_VISIBILITY_STATE_PRERENDER)
        priv->page->setIsPrerender();

    return true;
#else
    DBG("PAGE_VISIBILITY_API is disabled.");
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(pageVisibilityState);
    return false;
#endif
}

Ewk_Page_Visibility_State ewk_view_visibility_state_get(const Evas_Object* ewkView)
{
#if ENABLE(PAGE_VISIBILITY_API)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, EWK_PAGE_VISIBILITY_STATE_VISIBLE);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, EWK_PAGE_VISIBILITY_STATE_VISIBLE);


    switch (priv->page->visibilityState()) {
    case WebCore::PageVisibilityStateVisible:
        return EWK_PAGE_VISIBILITY_STATE_VISIBLE;
    case WebCore::PageVisibilityStateHidden:
        return EWK_PAGE_VISIBILITY_STATE_HIDDEN;
    case WebCore::PageVisibilityStatePrerender:
        return EWK_PAGE_VISIBILITY_STATE_PRERENDER;
    default:
        ASSERT_NOT_REACHED();
    }
#else
    DBG("PAGE_VISIBILITY_API is disabled.");
    UNUSED_PARAM(ewkView);
#endif

    return EWK_PAGE_VISIBILITY_STATE_VISIBLE;
}

Eina_Bool ewk_view_setting_enable_xss_auditor_get(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.enableXSSAuditor;
}

void ewk_view_setting_enable_xss_auditor_set(Evas_Object* ewkView, Eina_Bool enable)
{
    EWK_VIEW_SD_GET(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);
    enable = !!enable;
    if (priv->settings.enableXSSAuditor != enable) {
        priv->page->settings().setXSSAuditorEnabled(enable);
        priv->settings.enableXSSAuditor = enable;
    }
}

Eina_Bool ewk_view_setting_should_display_subtitles_get(const Evas_Object *ewkView)
{
#if ENABLE(VIDEO_TRACK)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.shouldDisplaySubtitles;
#else
    UNUSED_PARAM(ewkView);
    return false;
#endif
}

Eina_Bool ewk_view_setting_should_display_captions_get(const Evas_Object *ewkView)
{
#if ENABLE(VIDEO_TRACK)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.shouldDisplayCaptions;
#else
    UNUSED_PARAM(ewkView);
    return false;
#endif
}

void ewk_view_setting_should_display_captions_set(Evas_Object *ewkView, Eina_Bool enable)
{
#if ENABLE(VIDEO_TRACK)
    EWK_VIEW_SD_GET(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);
    enable = !!enable;
    if (priv->settings.shouldDisplayCaptions != enable) {
        priv->page->settings().setShouldDisplayCaptions(enable);
        priv->settings.shouldDisplayCaptions = enable;
    }
#else
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(enable);
#endif
}

void ewk_view_setting_should_display_subtitles_set(Evas_Object *ewkView, Eina_Bool enable)
{
#if ENABLE(VIDEO_TRACK)
    EWK_VIEW_SD_GET(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);
    enable = !!enable;
    if (priv->settings.shouldDisplaySubtitles != enable) {
        priv->page->settings().setShouldDisplaySubtitles(enable);
        priv->settings.shouldDisplaySubtitles = enable;
    }
#else
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(enable);
#endif
}

Eina_Bool ewk_view_setting_should_display_text_descriptions_get(const Evas_Object *ewkView)
{
#if ENABLE(VIDEO_TRACK)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.shouldDisplayTextDescriptions;
#else
    UNUSED_PARAM(ewkView);
    return false;
#endif
}

void ewk_view_setting_should_display_text_descriptions_set(Evas_Object *ewkView, Eina_Bool enable)
{
#if ENABLE(VIDEO_TRACK)
    EWK_VIEW_SD_GET(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);
    enable = !!enable;
    if (priv->settings.shouldDisplayTextDescriptions != enable) {
        priv->page->settings().setShouldDisplayTextDescriptions(enable);
        priv->settings.shouldDisplayTextDescriptions = enable;
    }
#else
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(enable);
#endif
}

void ewk_view_inspector_show(const Evas_Object* ewkView)
{
#if ENABLE(INSPECTOR)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    priv->page->inspectorController().show();
#else
    UNUSED_PARAM(ewkView);
#endif
}

void ewk_view_inspector_close(const Evas_Object* ewkView)
{
#if ENABLE(INSPECTOR)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    priv->page->inspectorController().close();
#else
    UNUSED_PARAM(ewkView);
#endif
}

Evas_Object* ewk_view_inspector_view_get(const Evas_Object* ewkView)
{
#if ENABLE(INSPECTOR)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    return priv->inspectorView;
#else
    UNUSED_PARAM(ewkView);
    return 0;
#endif
}

void ewk_view_inspector_view_set(Evas_Object* ewkView, Evas_Object* inspectorView)
{
#if ENABLE(INSPECTOR)
    EWK_VIEW_SD_GET(ewkView, smartData);
    EWK_VIEW_PRIV_GET(smartData, priv);

    priv->inspectorView = inspectorView;
#else
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(inspectorView);
#endif
}

void ewk_view_root_graphics_layer_set(Evas_Object* ewkView, WebCore::GraphicsLayer* rootLayer)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    priv->acceleratedCompositingContext->setRootGraphicsLayer(rootLayer);
}

void ewk_view_mark_for_sync(Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    // Mark the image as "dirty" meaning it needs an update next time evas renders.
    // It will call the pixel get callback then.
    evas_object_image_pixels_dirty_set(smartData->image, true);
}

void ewk_view_cursor_set(Evas_Object* ewkView, const WebCore::Cursor& cursor)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    const char* group = cursor.platformCursor();
    if (!group || group == priv->cursorGroup)
        return;

    priv->cursorGroup = group;

    if (priv->cursorObject)
        evas_object_del(priv->cursorObject);
    priv->cursorObject = edje_object_add(smartData->base.evas);

    Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(smartData->base.evas);
    if (!priv->settings.theme || !edje_object_file_set(priv->cursorObject, priv->settings.theme, group)) {
        evas_object_del(priv->cursorObject);
        priv->cursorObject = 0;

        ecore_evas_object_cursor_set(ecoreEvas, 0, 0, 0, 0);
#ifdef HAVE_ECORE_X
        if (priv->isUsingEcoreX)
            WebCore::applyFallbackCursor(ecoreEvas, group);
#endif
    } else {
        Evas_Coord width, height;
        edje_object_size_min_get(priv->cursorObject, &width, &height);
        if (width <= 0 || height <= 0)
            edje_object_size_min_calc(priv->cursorObject, &width, &height);
        if (width <= 0 || height <= 0) {
            width = 16;
            height = 16;
        }
        evas_object_resize(priv->cursorObject, width, height);

        const char* data;
        int hotspotX = 0;
        data = edje_object_data_get(priv->cursorObject, "hot.x");
        if (data)
            hotspotX = atoi(data);

        int hotspotY = 0;
        data = edje_object_data_get(priv->cursorObject, "hot.y");
        if (data)
            hotspotY = atoi(data);

        ecore_evas_object_cursor_set(ecoreEvas, priv->cursorObject, EVAS_LAYER_MAX, hotspotX, hotspotY);
    }
}

Eina_Bool ewk_view_setting_enable_fullscreen_get(const Evas_Object* ewkView)
{
#if ENABLE(FULLSCREEN_API)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    return priv->settings.enableFullscreen;
#else
    UNUSED_PARAM(ewkView);
    return false;
#endif
}

Eina_Bool ewk_view_setting_enable_fullscreen_set(Evas_Object* ewkView, Eina_Bool enable)
{
#if ENABLE(FULLSCREEN_API)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);
    enable = !!enable;
    if (priv->settings.enableFullscreen != enable) {
        priv->page->settings().setFullScreenEnabled(enable);
        priv->settings.enableFullscreen = enable;
    }
    return true;
#else
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(enable);
    return false;
#endif
}

#if ENABLE(FULLSCREEN_API)
/**
 * @internal
 * Calls fullscreen_enter callback or falls back to default behavior and enables fullscreen mode.
 */
void ewk_view_fullscreen_enter(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    if (!smartData->api->fullscreen_enter || !smartData->api->fullscreen_enter(smartData)) {
        Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(smartData->base.evas);
        ecore_evas_fullscreen_set(ecoreEvas, true);
    }
}

/**
 * @internal
 * Calls fullscreen_exit callback or falls back to default behavior and disables fullscreen mode.
 */
void ewk_view_fullscreen_exit(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);

    if (!smartData->api->fullscreen_exit || !smartData->api->fullscreen_exit(smartData)) {
        Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(smartData->base.evas);
        ecore_evas_fullscreen_set(ecoreEvas, false);
    }
}
#endif

Ewk_Context_Menu* ewk_view_context_menu_get(const Evas_Object* ewkView)
{
#if ENABLE(CONTEXT_MENUS)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    return priv->contextMenu;
#else
    UNUSED_PARAM(ewkView);
    return 0;
#endif
}

Evas_Object* ewk_view_screenshot_contents_get(const Evas_Object* ewkView, const Eina_Rectangle* area, float scale)
{
    if (!area || !area->w || !area->h) {
        ERR("empty area is not allowed");
        return 0;
    }

    if (scale < std::numeric_limits<float>::epsilon()) {
        ERR("scale factor should be bigger than zero");
        return 0;
    }

    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);

    Evas* canvas = evas_object_evas_get(ewkView);
    EINA_SAFETY_ON_NULL_RETURN_VAL(canvas, 0);

    Evas_Object* screenshotImage = evas_object_image_add(canvas);
    int surfaceWidth = area->w * scale;
    int surfaceHeight = area->h * scale;
    evas_object_image_size_set(screenshotImage, surfaceWidth, surfaceHeight);
    evas_object_image_fill_set(screenshotImage, 0, 0, surfaceWidth, surfaceHeight);
    evas_object_resize(screenshotImage, surfaceWidth, surfaceHeight);
    evas_object_image_colorspace_set(screenshotImage, EVAS_COLORSPACE_ARGB8888);
    evas_object_image_smooth_scale_set(screenshotImage, true);

    Ewk_Paint_Context* context = ewk_paint_context_from_image_new(screenshotImage);
    ewk_paint_context_save(context);
    ewk_paint_context_scale(context, scale, scale);
    ewk_paint_context_translate(context, -1 * area->x, -1 * area->y);

    ewk_view_paint(priv, context, area);

    ewk_paint_context_restore(context);
    ewk_paint_context_free(context);

    return screenshotImage;
}

Eina_Bool ewk_view_setting_tiled_backing_store_enabled_set(Evas_Object* ewkView, Eina_Bool enable)
{
#if USE(TILED_BACKING_STORE)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    priv->page->settings().setTiledBackingStoreEnabled(enable);

    return true;
#else
    UNUSED_PARAM(ewkView);
    UNUSED_PARAM(enable);
    return false;
#endif
}

Eina_Bool ewk_view_setting_tiled_backing_store_enabled_get(Evas_Object* ewkView)
{
#if USE(TILED_BACKING_STORE)
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, false);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, false);

    return priv->page->settings().tiledBackingStoreEnabled();
#else
    UNUSED_PARAM(ewkView);
    return false;
#endif
}

#if USE(TILED_BACKING_STORE)
/**
 * @internal
 * Invalidate given area to repaint. The backing store will mark tiles that are
 * in the area as dirty.
 *
 * @param ewkView View.
 * @param area Area to invalidate
 */
void ewk_view_tiled_backing_store_invalidate(Evas_Object* ewkView, const WebCore::IntRect& area)
{
    EINA_SAFETY_ON_NULL_RETURN(ewkView);
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv);

    if (priv->page->mainFrame().tiledBackingStore())
        priv->page->mainFrame().tiledBackingStore()->invalidate(area);
}
#endif

namespace EWKPrivate {

WebCore::Page* corePage(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return priv->page.get();
}

WebCore::NetworkStorageSession* storageSession(const Evas_Object* ewkView)
{
    EWK_VIEW_SD_GET_OR_RETURN(ewkView, smartData, 0);
    EWK_VIEW_PRIV_GET_OR_RETURN(smartData, priv, 0);
    return &WebCore::NetworkStorageSession::defaultStorageSession();
}

} // namespace EWKPrivate
