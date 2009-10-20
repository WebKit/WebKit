/*
 *  Copyright (C) 2007, 2008 Holger Hans Peter Freyther
 *  Copyright (C) 2007, 2008, 2009 Christian Dywan <christian@imendio.com>
 *  Copyright (C) 2007 Xan Lopez <xan@gnome.org>
 *  Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
 *  Copyright (C) 2008 Jan Alonzo <jmalonzo@unpluggable.com>
 *  Copyright (C) 2008 Gustavo Noronha Silva <gns@gnome.org>
 *  Copyright (C) 2008 Nuanti Ltd.
 *  Copyright (C) 2008, 2009 Collabora Ltd.
 *  Copyright (C) 2009 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "webkitwebview.h"

#include "webkitdownload.h"
#include "webkitenumtypes.h"
#include "webkitmarshal.h"
#include "webkitnetworkrequest.h"
#include "webkitnetworkresponse.h"
#include "webkitprivate.h"
#include "webkitwebinspector.h"
#include "webkitwebbackforwardlist.h"
#include "webkitwebhistoryitem.h"

#include "AXObjectCache.h"
#include "NotImplemented.h"
#include "BackForwardList.h"
#include "CString.h"
#include "ChromeClientGtk.h"
#include "ContextMenu.h"
#include "ContextMenuClientGtk.h"
#include "ContextMenuController.h"
#include "Cursor.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DragClientGtk.h"
#include "Editor.h"
#include "EditorClientGtk.h"
#include "EventHandler.h"
#include "FloatQuad.h"
#include "FocusController.h"
#include "FrameLoaderTypes.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include <glib/gi18n-lib.h>
#include "GraphicsContext.h"
#include "InspectorClientGtk.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "MouseEventWithHitTestResults.h"
#include "PasteboardHelper.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "ProgressTracker.h"
#include "ResourceHandle.h"
#include "RenderView.h"
#include "ScriptValue.h"
#include "Scrollbar.h"
#include <wtf/GOwnPtr.h>

#include <gdk/gdkkeysyms.h>

/**
 * SECTION:webkitwebview
 * @short_description: The central class of the WebKitGTK+ API
 * @see_also: #WebKitWebSettings, #WebKitWebFrame
 *
 * #WebKitWebView is the central class of the WebKitGTK+ API. It is a
 * #GtkWidget implementing the scrolling interface which means you can
 * embed in a #GtkScrolledWindow. It is responsible for managing the
 * drawing of the content, forwarding of events. You can load any URI
 * into the #WebKitWebView or any kind of data string. With #WebKitWebSettings
 * you can control various aspects of the rendering and loading of the content.
 * Each #WebKitWebView has exactly one #WebKitWebFrame as main frame. A
 * #WebKitWebFrame can have n children.
 *
 * <programlisting>
 * /<!-- -->* Create the widgets *<!-- -->/
 * GtkWidget *main_window = gtk_window_new (GTK_WIDGET_TOPLEVEL);
 * GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
 * GtkWidget *web_view = webkit_web_view_new ();
 *
 * /<!-- -->* Place the WebKitWebView in the GtkScrolledWindow *<!-- -->/
 * gtk_container_add (GTK_CONTAINER (scrolled_window), web_view);
 * gtk_container_add (GTK_CONTAINER (main_window), scrolled_window);
 *
 * /<!-- -->* Open a webpage *<!-- -->/
 * webkit_web_view_load_uri (WEBKIT_WEB_VIEW (web_view), "http://www.gnome.org");
 *
 * /<!-- -->* Show the result *<!-- -->/
 * gtk_window_set_default_size (GTK_WINDOW (main_window), 800, 600);
 * gtk_widget_show_all (main_window);
 * </programlisting>
 */

static const double defaultDPI = 96.0;

using namespace WebKit;
using namespace WebCore;

enum {
    /* normal signals */
    NAVIGATION_REQUESTED,
    NEW_WINDOW_POLICY_DECISION_REQUESTED,
    NAVIGATION_POLICY_DECISION_REQUESTED,
    MIME_TYPE_POLICY_DECISION_REQUESTED,
    CREATE_WEB_VIEW,
    WEB_VIEW_READY,
    WINDOW_OBJECT_CLEARED,
    LOAD_STARTED,
    LOAD_COMMITTED,
    LOAD_PROGRESS_CHANGED,
    LOAD_ERROR,
    LOAD_FINISHED,
    TITLE_CHANGED,
    HOVERING_OVER_LINK,
    POPULATE_POPUP,
    STATUS_BAR_TEXT_CHANGED,
    ICOND_LOADED,
    SELECTION_CHANGED,
    CONSOLE_MESSAGE,
    SCRIPT_ALERT,
    SCRIPT_CONFIRM,
    SCRIPT_PROMPT,
    SELECT_ALL,
    COPY_CLIPBOARD,
    PASTE_CLIPBOARD,
    CUT_CLIPBOARD,
    DOWNLOAD_REQUESTED,
    MOVE_CURSOR,
    PRINT_REQUESTED,
    PLUGIN_WIDGET,
    CLOSE_WEB_VIEW,
    UNDO,
    REDO,
    DATABASE_QUOTA_EXCEEDED,
    RESOURCE_REQUEST_STARTING,
    LAST_SIGNAL
};

enum {
    PROP_0,

    PROP_TITLE,
    PROP_URI,
    PROP_COPY_TARGET_LIST,
    PROP_PASTE_TARGET_LIST,
    PROP_EDITABLE,
    PROP_SETTINGS,
    PROP_WEB_INSPECTOR,
    PROP_WINDOW_FEATURES,
    PROP_TRANSPARENT,
    PROP_ZOOM_LEVEL,
    PROP_FULL_CONTENT_ZOOM,
    PROP_LOAD_STATUS,
    PROP_PROGRESS,
    PROP_ENCODING,
    PROP_CUSTOM_ENCODING
};

static guint webkit_web_view_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(WebKitWebView, webkit_web_view, GTK_TYPE_CONTAINER)

static void webkit_web_view_settings_notify(WebKitWebSettings* webSettings, GParamSpec* pspec, WebKitWebView* webView);
static void webkit_web_view_set_window_features(WebKitWebView* webView, WebKitWebWindowFeatures* webWindowFeatures);

static void destroy_menu_cb(GtkObject* object, gpointer data)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(data);
    WebKitWebViewPrivate* priv = WEBKIT_WEB_VIEW_GET_PRIVATE(webView);

    g_object_unref(priv->currentMenu);
    priv->currentMenu = NULL;
}

static gboolean webkit_web_view_forward_context_menu_event(WebKitWebView* webView, const PlatformMouseEvent& event)
{
    Page* page = core(webView);
    page->contextMenuController()->clearContextMenu();
    Frame* focusedFrame = page->focusController()->focusedOrMainFrame();

    if (!focusedFrame->view())
        return FALSE;

    focusedFrame->view()->setCursor(pointerCursor());
    bool handledEvent = focusedFrame->eventHandler()->sendContextMenuEvent(event);
    if (!handledEvent)
        return FALSE;

    ContextMenu* coreMenu = page->contextMenuController()->contextMenu();
    if (!coreMenu)
        return FALSE;

    GtkMenu* menu = GTK_MENU(coreMenu->platformDescription());
    if (!menu)
        return FALSE;

    g_signal_emit(webView, webkit_web_view_signals[POPULATE_POPUP], 0, menu);

    GList* items = gtk_container_get_children(GTK_CONTAINER(menu));
    bool empty = !g_list_nth(items, 0);
    g_list_free(items);
    if (empty)
        return FALSE;

    WebKitWebViewPrivate* priv = WEBKIT_WEB_VIEW_GET_PRIVATE(webView);
    priv->currentMenu = GTK_MENU(g_object_ref(menu));
    priv->lastPopupXPosition = event.globalX();
    priv->lastPopupYPosition = event.globalY();

    g_signal_connect(menu, "destroy",
                     G_CALLBACK(destroy_menu_cb),
                     NULL);

    gtk_menu_popup(menu, NULL, NULL,
                   NULL,
                   priv, event.button() + 1, gtk_get_current_event_time());
    return TRUE;
}

static gboolean webkit_web_view_popup_menu_handler(GtkWidget* widget)
{
    static const int contextMenuMargin = 1;

    // The context menu event was generated from the keyboard, so show the context menu by the current selection.
    Page* page = core(WEBKIT_WEB_VIEW(widget));
    FrameView* view = page->mainFrame()->view();
    if (!view)
        return FALSE;    

    Position start = page->mainFrame()->selection()->selection().start();
    Position end = page->mainFrame()->selection()->selection().end();

    int rightAligned = FALSE;
    IntPoint location;

    if (!start.node() || !end.node())
        location = IntPoint(rightAligned ? view->contentsWidth() - contextMenuMargin : contextMenuMargin, contextMenuMargin);
    else {
        RenderObject* renderer = start.node()->renderer();
        if (!renderer)
            return FALSE;

        // Calculate the rect of the first line of the selection (cribbed from -[WebCoreFrameBridge firstRectForDOMRange:],
        // now Frame::firstRectForRange(), which perhaps this should call).
        int extraWidthToEndOfLine = 0;

        InlineBox* startInlineBox;
        int startCaretOffset;
        start.getInlineBoxAndOffset(DOWNSTREAM, startInlineBox, startCaretOffset);
        IntRect startCaretRect = renderer->localCaretRect(startInlineBox, startCaretOffset, &extraWidthToEndOfLine);
        if (startCaretRect != IntRect())
            startCaretRect = renderer->localToAbsoluteQuad(FloatRect(startCaretRect)).enclosingBoundingBox();

        InlineBox* endInlineBox;
        int endCaretOffset;
        end.getInlineBoxAndOffset(UPSTREAM, endInlineBox, endCaretOffset);
        IntRect endCaretRect = renderer->localCaretRect(endInlineBox, endCaretOffset);
        if (endCaretRect != IntRect())
            endCaretRect = renderer->localToAbsoluteQuad(FloatRect(endCaretRect)).enclosingBoundingBox();

        IntRect firstRect;
        if (startCaretRect.y() == endCaretRect.y())
            firstRect = IntRect(MIN(startCaretRect.x(), endCaretRect.x()),
                                startCaretRect.y(),
                                abs(endCaretRect.x() - startCaretRect.x()),
                                MAX(startCaretRect.height(), endCaretRect.height()));
        else
            firstRect = IntRect(startCaretRect.x(),
                                startCaretRect.y(),
                                startCaretRect.width() + extraWidthToEndOfLine,
                                startCaretRect.height());

        location = IntPoint(rightAligned ? firstRect.right() : firstRect.x(), firstRect.bottom());
    }

    int x, y;
    gdk_window_get_origin(GTK_WIDGET(view->hostWindow()->platformPageClient())->window, &x, &y);

    // FIXME: The IntSize(0, -1) is a hack to get the hit-testing to result in the selected element.
    // Ideally we'd have the position of a context menu event be separate from its target node.
    location = view->contentsToWindow(location) + IntSize(0, -1);
    IntPoint global = location + IntSize(x, y);
    PlatformMouseEvent event(location, global, NoButton, MouseEventPressed, 0, false, false, false, false, gtk_get_current_event_time());

    return webkit_web_view_forward_context_menu_event(WEBKIT_WEB_VIEW(widget), event);
}

static void webkit_web_view_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);

    switch(prop_id) {
    case PROP_TITLE:
        g_value_set_string(value, webkit_web_view_get_title(webView));
        break;
    case PROP_URI:
        g_value_set_string(value, webkit_web_view_get_uri(webView));
        break;
    case PROP_COPY_TARGET_LIST:
        g_value_set_boxed(value, webkit_web_view_get_copy_target_list(webView));
        break;
    case PROP_PASTE_TARGET_LIST:
        g_value_set_boxed(value, webkit_web_view_get_paste_target_list(webView));
        break;
    case PROP_EDITABLE:
        g_value_set_boolean(value, webkit_web_view_get_editable(webView));
        break;
    case PROP_SETTINGS:
        g_value_set_object(value, webkit_web_view_get_settings(webView));
        break;
    case PROP_WEB_INSPECTOR:
        g_value_set_object(value, webkit_web_view_get_inspector(webView));
        break;
    case PROP_WINDOW_FEATURES:
        g_value_set_object(value, webkit_web_view_get_window_features(webView));
        break;
    case PROP_TRANSPARENT:
        g_value_set_boolean(value, webkit_web_view_get_transparent(webView));
        break;
    case PROP_ZOOM_LEVEL:
        g_value_set_float(value, webkit_web_view_get_zoom_level(webView));
        break;
    case PROP_FULL_CONTENT_ZOOM:
        g_value_set_boolean(value, webkit_web_view_get_full_content_zoom(webView));
        break;
    case PROP_ENCODING:
        g_value_set_string(value, webkit_web_view_get_encoding(webView));
        break;
    case PROP_CUSTOM_ENCODING:
        g_value_set_string(value, webkit_web_view_get_custom_encoding(webView));
        break;
    case PROP_LOAD_STATUS:
        g_value_set_enum(value, webkit_web_view_get_load_status(webView));
        break;
    case PROP_PROGRESS:
        g_value_set_double(value, webkit_web_view_get_progress(webView));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void webkit_web_view_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec *pspec)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);

    switch(prop_id) {
    case PROP_EDITABLE:
        webkit_web_view_set_editable(webView, g_value_get_boolean(value));
        break;
    case PROP_SETTINGS:
        webkit_web_view_set_settings(webView, WEBKIT_WEB_SETTINGS(g_value_get_object(value)));
        break;
    case PROP_WINDOW_FEATURES:
        webkit_web_view_set_window_features(webView, WEBKIT_WEB_WINDOW_FEATURES(g_value_get_object(value)));
        break;
    case PROP_TRANSPARENT:
        webkit_web_view_set_transparent(webView, g_value_get_boolean(value));
        break;
    case PROP_ZOOM_LEVEL:
        webkit_web_view_set_zoom_level(webView, g_value_get_float(value));
        break;
    case PROP_FULL_CONTENT_ZOOM:
        webkit_web_view_set_full_content_zoom(webView, g_value_get_boolean(value));
        break;
    case PROP_CUSTOM_ENCODING:
        webkit_web_view_set_custom_encoding(webView, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static bool shouldCoalesce(GdkRectangle rect, GdkRectangle* rects, int count)
{
    const int cRectThreshold = 10;
    const float cWastedSpaceThreshold = 0.75f;
    bool useUnionedRect = (count <= 1) || (count > cRectThreshold);
    if (!useUnionedRect) {
        // Attempt to guess whether or not we should use the unioned rect or the individual rects.
        // We do this by computing the percentage of "wasted space" in the union.  If that wasted space
        // is too large, then we will do individual rect painting instead.
        float unionPixels = (rect.width * rect.height);
        float singlePixels = 0;
        for (int i = 0; i < count; ++i)
            singlePixels += rects[i].width * rects[i].height;
        float wastedSpace = 1 - (singlePixels / unionPixels);
        if (wastedSpace <= cWastedSpaceThreshold)
            useUnionedRect = true;
    }
    return useUnionedRect;
}

static gboolean webkit_web_view_expose_event(GtkWidget* widget, GdkEventExpose* event)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);
    WebKitWebViewPrivate* priv = webView->priv;

    Frame* frame = core(webView)->mainFrame();
    if (frame->contentRenderer() && frame->view()) {
        frame->view()->layoutIfNeededRecursive();

        cairo_t* cr = gdk_cairo_create(event->window);
        GraphicsContext ctx(cr);
        cairo_destroy(cr);
        ctx.setGdkExposeEvent(event);

        GOwnPtr<GdkRectangle> rects;
        int rectCount;
        gdk_region_get_rectangles(event->region, &rects.outPtr(), &rectCount);

        // Avoid recursing into the render tree excessively
        bool coalesce = shouldCoalesce(event->area, rects.get(), rectCount);

        if (coalesce) {
            IntRect rect = event->area;
            ctx.clip(rect);
            if (priv->transparent)
                ctx.clearRect(rect);
            frame->view()->paint(&ctx, rect);
        } else {
            for (int i = 0; i < rectCount; i++) {
                IntRect rect = rects.get()[i];
                ctx.save();
                ctx.clip(rect);
                if (priv->transparent)
                    ctx.clearRect(rect);
                frame->view()->paint(&ctx, rect);
                ctx.restore();
            }
        }
    }

    return FALSE;
}

static gboolean webkit_web_view_key_press_event(GtkWidget* widget, GdkEventKey* event)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    PlatformKeyboardEvent keyboardEvent(event);

    if (!frame->view())
        return FALSE;

    if (frame->eventHandler()->keyEvent(keyboardEvent))
        return TRUE;

    /* Chain up to our parent class for binding activation */
    return GTK_WIDGET_CLASS(webkit_web_view_parent_class)->key_press_event(widget, event);
}

static gboolean webkit_web_view_key_release_event(GtkWidget* widget, GdkEventKey* event)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    if (!frame->view())
        return FALSE;

    PlatformKeyboardEvent keyboardEvent(event);

    if (frame->eventHandler()->keyEvent(keyboardEvent))
        return TRUE;

    /* Chain up to our parent class for binding activation */
    return GTK_WIDGET_CLASS(webkit_web_view_parent_class)->key_release_event(widget, event);
}

static gboolean webkit_web_view_button_press_event(GtkWidget* widget, GdkEventButton* event)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);

    // FIXME: need to keep track of subframe focus for key events
    gtk_widget_grab_focus(widget);

    if (event->button == 3)
        return webkit_web_view_forward_context_menu_event(webView, PlatformMouseEvent(event));

    Frame* frame = core(webView)->mainFrame();
    if (!frame->view())
        return FALSE;

    gboolean result = frame->eventHandler()->handleMousePressEvent(PlatformMouseEvent(event));

#if PLATFORM(X11)
    /* Copy selection to the X11 selection clipboard */
    if (event->button == 2) {
        bool primary = webView->priv->usePrimaryForPaste;
        webView->priv->usePrimaryForPaste = true;

        Editor* editor = webView->priv->corePage->focusController()->focusedOrMainFrame()->editor();
        result = result || editor->canPaste() || editor->canDHTMLPaste();
        editor->paste();

        webView->priv->usePrimaryForPaste = primary;
    }
#endif

    return result;
}

static gboolean webkit_web_view_button_release_event(GtkWidget* widget, GdkEventButton* event)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);

    Frame* focusedFrame = core(webView)->focusController()->focusedFrame();

    if (focusedFrame && focusedFrame->editor()->canEdit()) {
#ifdef MAEMO_CHANGES
        WebKitWebViewPrivate* priv = webView->priv;
        hildon_gtk_im_context_filter_event(priv->imContext, (GdkEvent*)event);
#endif
    }

    Frame* mainFrame = core(webView)->mainFrame();
    if (mainFrame->view())
        mainFrame->eventHandler()->handleMouseReleaseEvent(PlatformMouseEvent(event));

    /* We always return FALSE here because WebKit can, for the same click, decide
     * to not handle press-event but handle release-event, which can totally confuse
     * some GTK+ containers when there are no other events in between. This way we
     * guarantee that this case never happens, and that if press-event goes through
     * release-event also goes through.
     */

    return FALSE;
}

static gboolean webkit_web_view_motion_event(GtkWidget* widget, GdkEventMotion* event)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);

    Frame* frame = core(webView)->mainFrame();
    if (!frame->view())
        return FALSE;

    return frame->eventHandler()->mouseMoved(PlatformMouseEvent(event));
}

static gboolean webkit_web_view_scroll_event(GtkWidget* widget, GdkEventScroll* event)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);

    Frame* frame = core(webView)->mainFrame();
    if (!frame->view())
        return FALSE;

    PlatformWheelEvent wheelEvent(event);
    return frame->eventHandler()->handleWheelEvent(wheelEvent);
}

static void webkit_web_view_size_request(GtkWidget* widget, GtkRequisition* requisition)
{
    WebKitWebView* web_view = WEBKIT_WEB_VIEW(widget);
    Frame* coreFrame = core(webkit_web_view_get_main_frame(web_view));
    if (!coreFrame)
        return;

    FrameView* view = coreFrame->view();
    if (!view)
        return;

    requisition->width = view->contentsWidth();
    requisition->height = view->contentsHeight();
}

static void webkit_web_view_size_allocate(GtkWidget* widget, GtkAllocation* allocation)
{
    GTK_WIDGET_CLASS(webkit_web_view_parent_class)->size_allocate(widget,allocation);

    WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);

    Frame* frame = core(webView)->mainFrame();
    if (!frame->view())
        return;

    frame->view()->resize(allocation->width, allocation->height);
    frame->view()->forceLayout();
    frame->view()->adjustViewSize();
}

static void webkit_web_view_grab_focus(GtkWidget* widget)
{
    if (GTK_WIDGET_IS_SENSITIVE(widget)) {
        WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);
        FocusController* focusController = core(webView)->focusController();

        if (focusController->focusedFrame())
            focusController->setFocused(true);
        else
            focusController->setFocusedFrame(core(webView)->mainFrame());
    }

    return GTK_WIDGET_CLASS(webkit_web_view_parent_class)->grab_focus(widget);
}

static gboolean webkit_web_view_focus_in_event(GtkWidget* widget, GdkEventFocus* event)
{
    // TODO: Improve focus handling as suggested in
    // http://bugs.webkit.org/show_bug.cgi?id=16910
    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    if (GTK_WIDGET_TOPLEVEL(toplevel) && gtk_window_has_toplevel_focus(GTK_WINDOW(toplevel))) {
        WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);
        FocusController* focusController = core(webView)->focusController();

        focusController->setActive(true);

        if (focusController->focusedFrame())
            focusController->setFocused(true);
        else
            focusController->setFocusedFrame(core(webView)->mainFrame());
    }
    return GTK_WIDGET_CLASS(webkit_web_view_parent_class)->focus_in_event(widget, event);
}

static gboolean webkit_web_view_focus_out_event(GtkWidget* widget, GdkEventFocus* event)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);

    core(webView)->focusController()->setActive(false);
    core(webView)->focusController()->setFocused(false);

    return GTK_WIDGET_CLASS(webkit_web_view_parent_class)->focus_out_event(widget, event);
}

static void webkit_web_view_realize(GtkWidget* widget)
{
    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

    GdkWindowAttr attributes;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual (widget);
    attributes.colormap = gtk_widget_get_colormap (widget);
    attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK
                            | GDK_EXPOSURE_MASK
                            | GDK_BUTTON_PRESS_MASK
                            | GDK_BUTTON_RELEASE_MASK
                            | GDK_POINTER_MOTION_MASK
                            | GDK_KEY_PRESS_MASK
                            | GDK_KEY_RELEASE_MASK
                            | GDK_BUTTON_MOTION_MASK
                            | GDK_BUTTON1_MOTION_MASK
                            | GDK_BUTTON2_MOTION_MASK
                            | GDK_BUTTON3_MOTION_MASK;

    gint attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new(gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, widget);

    widget->style = gtk_style_attach(widget->style, widget->window);
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

    WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);
    WebKitWebViewPrivate* priv = webView->priv;
    gtk_im_context_set_client_window(priv->imContext, widget->window);
}

static void webkit_web_view_set_scroll_adjustments(WebKitWebView* webView, GtkAdjustment* hadj, GtkAdjustment* vadj)
{
    if (!core(webView))
        return;

    FrameView* view = core(webkit_web_view_get_main_frame(webView))->view();

    if (hadj)
        g_object_ref(hadj);
    if (vadj)
        g_object_ref(vadj);

    WebKitWebViewPrivate* priv = webView->priv;

    if (priv->horizontalAdjustment)
        g_object_unref(priv->horizontalAdjustment);
    if (priv->verticalAdjustment)
        g_object_unref(priv->verticalAdjustment);

    priv->horizontalAdjustment = hadj;
    priv->verticalAdjustment = vadj;

    if (!view)
        return;

    view->setGtkAdjustments(hadj, vadj);
}

static void webkit_web_view_container_add(GtkContainer* container, GtkWidget* widget)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(container);
    WebKitWebViewPrivate* priv = webView->priv;

    priv->children.add(widget);
    gtk_widget_set_parent(widget, GTK_WIDGET(container));
}

static void webkit_web_view_container_remove(GtkContainer* container, GtkWidget* widget)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(container);
    WebKitWebViewPrivate* priv = webView->priv;

    if (priv->children.contains(widget)) {
        gtk_widget_unparent(widget);
        priv->children.remove(widget);
    }
}

static void webkit_web_view_container_forall(GtkContainer* container, gboolean, GtkCallback callback, gpointer callbackData)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(container);
    WebKitWebViewPrivate* priv = webView->priv;

    HashSet<GtkWidget*> children = priv->children;
    HashSet<GtkWidget*>::const_iterator end = children.end();
    for (HashSet<GtkWidget*>::const_iterator current = children.begin(); current != end; ++current)
        (*callback)(*current, callbackData);
}

static WebKitWebView* webkit_web_view_real_create_web_view(WebKitWebView*, WebKitWebFrame*)
{
    return 0;
}

static gboolean webkit_web_view_real_web_view_ready(WebKitWebView*)
{
    return FALSE;
}

static gboolean webkit_web_view_real_close_web_view(WebKitWebView*)
{
    return FALSE;
}

static WebKitNavigationResponse webkit_web_view_real_navigation_requested(WebKitWebView*, WebKitWebFrame*, WebKitNetworkRequest*)
{
    return WEBKIT_NAVIGATION_RESPONSE_ACCEPT;
}

static void webkit_web_view_real_window_object_cleared(WebKitWebView*, WebKitWebFrame*, JSGlobalContextRef context, JSObjectRef window_object)
{
    notImplemented();
}

static gchar* webkit_web_view_real_choose_file(WebKitWebView*, WebKitWebFrame*, const gchar* old_name)
{
    notImplemented();
    return g_strdup(old_name);
}

typedef enum {
    WEBKIT_SCRIPT_DIALOG_ALERT,
    WEBKIT_SCRIPT_DIALOG_CONFIRM,
    WEBKIT_SCRIPT_DIALOG_PROMPT
 } WebKitScriptDialogType;

static gboolean webkit_web_view_script_dialog(WebKitWebView* webView, WebKitWebFrame* frame, const gchar* message, WebKitScriptDialogType type, const gchar* defaultValue, gchar** value)
{
    GtkMessageType messageType;
    GtkButtonsType buttons;
    gint defaultResponse;
    GtkWidget* window;
    GtkWidget* dialog;
    GtkWidget* entry = 0;
    gboolean didConfirm = FALSE;

    switch (type) {
    case WEBKIT_SCRIPT_DIALOG_ALERT:
        messageType = GTK_MESSAGE_WARNING;
        buttons = GTK_BUTTONS_CLOSE;
        defaultResponse = GTK_RESPONSE_CLOSE;
        break;
    case WEBKIT_SCRIPT_DIALOG_CONFIRM:
        messageType = GTK_MESSAGE_QUESTION;
        buttons = GTK_BUTTONS_YES_NO;
        defaultResponse = GTK_RESPONSE_YES;
        break;
    case WEBKIT_SCRIPT_DIALOG_PROMPT:
        messageType = GTK_MESSAGE_QUESTION;
        buttons = GTK_BUTTONS_OK_CANCEL;
        defaultResponse = GTK_RESPONSE_OK;
        break;
    default:
        g_warning("Unknown value for WebKitScriptDialogType.");
        return FALSE;
    }

    window = gtk_widget_get_toplevel(GTK_WIDGET(webView));
    dialog = gtk_message_dialog_new(GTK_WIDGET_TOPLEVEL(window) ? GTK_WINDOW(window) : 0, GTK_DIALOG_DESTROY_WITH_PARENT, messageType, buttons, "%s", message);
    gchar* title = g_strconcat("JavaScript - ", webkit_web_frame_get_uri(frame), NULL);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    g_free(title);

    if (type == WEBKIT_SCRIPT_DIALOG_PROMPT) {
        entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry), defaultValue);
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entry);
        gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
        gtk_widget_show(entry);
    }

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), defaultResponse);
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    switch (response) {
    case GTK_RESPONSE_YES:
        didConfirm = TRUE;
        break;
    case GTK_RESPONSE_OK:
        didConfirm = TRUE;
        if (entry)
            *value = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
        else
            *value = 0;
        break;
    case GTK_RESPONSE_NO:
    case GTK_RESPONSE_CANCEL:
        didConfirm = FALSE;
        break;

    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
    return didConfirm;
}

static gboolean webkit_web_view_real_script_alert(WebKitWebView* webView, WebKitWebFrame* frame, const gchar* message)
{
    webkit_web_view_script_dialog(webView, frame, message, WEBKIT_SCRIPT_DIALOG_ALERT, 0, 0);
    return TRUE;
}

static gboolean webkit_web_view_real_script_confirm(WebKitWebView* webView, WebKitWebFrame* frame, const gchar* message, gboolean* didConfirm)
{
    *didConfirm = webkit_web_view_script_dialog(webView, frame, message, WEBKIT_SCRIPT_DIALOG_CONFIRM, 0, 0);
    return TRUE;
}

static gboolean webkit_web_view_real_script_prompt(WebKitWebView* webView, WebKitWebFrame* frame, const gchar* message, const gchar* defaultValue, gchar** value)
{
    if (!webkit_web_view_script_dialog(webView, frame, message, WEBKIT_SCRIPT_DIALOG_PROMPT, defaultValue, value))
        *value = NULL;
    return TRUE;
}

static gboolean webkit_web_view_real_console_message(WebKitWebView* webView, const gchar* message, unsigned int line, const gchar* sourceId)
{
    g_print("console message: %s @%d: %s\n", sourceId, line, message);
    return TRUE;
}

static void webkit_web_view_real_select_all(WebKitWebView* webView)
{
    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    frame->editor()->command("SelectAll").execute();
}

static void webkit_web_view_real_cut_clipboard(WebKitWebView* webView)
{
    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    frame->editor()->command("Cut").execute();
}

static void webkit_web_view_real_copy_clipboard(WebKitWebView* webView)
{
    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    frame->editor()->command("Copy").execute();
}

static void webkit_web_view_real_undo(WebKitWebView* webView)
{
    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    frame->editor()->command("Undo").execute();
}

static void webkit_web_view_real_redo(WebKitWebView* webView)
{
    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    frame->editor()->command("Redo").execute();
}

static gboolean webkit_web_view_real_move_cursor (WebKitWebView* webView, GtkMovementStep step, gint count)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW (webView), FALSE);
    g_return_val_if_fail(step == GTK_MOVEMENT_VISUAL_POSITIONS ||
                         step == GTK_MOVEMENT_DISPLAY_LINES ||
                         step == GTK_MOVEMENT_PAGES ||
                         step == GTK_MOVEMENT_BUFFER_ENDS, FALSE);
    g_return_val_if_fail(count == 1 || count == -1, FALSE);

    ScrollDirection direction;
    ScrollGranularity granularity;

    switch (step) {
    case GTK_MOVEMENT_DISPLAY_LINES:
        granularity = ScrollByLine;
        if (count == 1)
            direction = ScrollDown;
        else
            direction = ScrollUp;
        break;
    case GTK_MOVEMENT_VISUAL_POSITIONS:
        granularity = ScrollByLine;
        if (count == 1)
            direction = ScrollRight;
        else
            direction = ScrollLeft;
        break;
    case GTK_MOVEMENT_PAGES:
        granularity = ScrollByPage;
        if (count == 1)
            direction = ScrollDown;
        else
            direction = ScrollUp;
        break;
    case GTK_MOVEMENT_BUFFER_ENDS:
        granularity = ScrollByDocument;
        if (count == 1)
            direction = ScrollDown;
        else
            direction = ScrollUp;
        break;
    default:
        g_assert_not_reached();
        return false;
    }

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    if (!frame->eventHandler()->scrollOverflow(direction, granularity))
        frame->view()->scroll(direction, granularity);

    return true;
}

static void webkit_web_view_real_paste_clipboard(WebKitWebView* webView)
{
    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    frame->editor()->command("Paste").execute();
}

static void webkit_web_view_dispose(GObject* object)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);
    WebKitWebViewPrivate* priv = webView->priv;

    priv->disposing = TRUE;

    if (priv->horizontalAdjustment) {
        g_object_unref(priv->horizontalAdjustment);
        priv->horizontalAdjustment = NULL;
    }

    if (priv->verticalAdjustment) {
        g_object_unref(priv->verticalAdjustment);
        priv->verticalAdjustment = NULL;
    }

    if (priv->backForwardList) {
        g_object_unref(priv->backForwardList);
        priv->backForwardList = NULL;
    }

    if (priv->corePage) {
        webkit_web_view_stop_loading(WEBKIT_WEB_VIEW(object));

        core(priv->mainFrame)->loader()->detachFromParent();
        delete priv->corePage;
        priv->corePage = NULL;
    }

    if (priv->webSettings) {
        g_signal_handlers_disconnect_by_func(priv->webSettings, (gpointer)webkit_web_view_settings_notify, webView);
        g_object_unref(priv->webSettings);
        priv->webSettings = NULL;

        g_object_unref(priv->webInspector);
        priv->webInspector = NULL;

        g_object_unref(priv->webWindowFeatures);
        priv->webWindowFeatures = NULL;

        g_object_unref(priv->imContext);
        priv->imContext = NULL;

        gtk_target_list_unref(priv->copy_target_list);
        priv->copy_target_list = NULL;

        gtk_target_list_unref(priv->paste_target_list);
        priv->paste_target_list = NULL;
    }

    if (priv->mainResource) {
        g_object_unref(priv->mainResource);
        priv->mainResource = NULL;
    }

    if (priv->subResources) {
        g_hash_table_unref(priv->subResources);
        priv->subResources = NULL;
    }

    G_OBJECT_CLASS(webkit_web_view_parent_class)->dispose(object);
}

static void webkit_web_view_finalize(GObject* object)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);
    WebKitWebViewPrivate* priv = webView->priv;

    g_free(priv->mainResourceIdentifier);
    g_free(priv->encoding);
    g_free(priv->customEncoding);

    G_OBJECT_CLASS(webkit_web_view_parent_class)->finalize(object);
}

static gboolean webkit_signal_accumulator_object_handled(GSignalInvocationHint* ihint, GValue* returnAccu, const GValue* handlerReturn, gpointer dummy)
{
    gpointer newWebView = g_value_get_object(handlerReturn);
    g_value_set_object(returnAccu, newWebView);

    // Continue if we don't have a newWebView
    return !newWebView;
}

static gboolean webkit_navigation_request_handled(GSignalInvocationHint* ihint, GValue* returnAccu, const GValue* handlerReturn, gpointer dummy)
{
    WebKitNavigationResponse navigationResponse = (WebKitNavigationResponse)g_value_get_enum(handlerReturn);
    g_value_set_enum(returnAccu, navigationResponse);

    if (navigationResponse != WEBKIT_NAVIGATION_RESPONSE_ACCEPT)
        return FALSE;

    return TRUE;
}

static AtkObject* webkit_web_view_get_accessible(GtkWidget* widget)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);
    if (!core(webView))
        return NULL;

    AXObjectCache::enableAccessibility();

    Frame* coreFrame = core(webView)->mainFrame();
    if (!coreFrame)
        return NULL;

    Document* doc = coreFrame->document();
    if (!doc)
        return NULL;

    AccessibilityObject* coreAccessible = doc->axObjectCache()->getOrCreate(doc->renderer());
    if (!coreAccessible || !coreAccessible->wrapper())
        return NULL;

    return coreAccessible->wrapper();
}

static gdouble webViewGetDPI(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = webView->priv;
    WebKitWebSettings* webSettings = priv->webSettings;
    gboolean enforce96DPI;
    g_object_get(webSettings, "enforce-96-dpi", &enforce96DPI, NULL);
    if (enforce96DPI)
        return 96.0;

    gdouble DPI = defaultDPI;
    GdkScreen* screen = gtk_widget_has_screen(GTK_WIDGET(webView)) ? gtk_widget_get_screen(GTK_WIDGET(webView)) : gdk_screen_get_default();
    if (screen) {
        DPI = gdk_screen_get_resolution(screen);
        // gdk_screen_get_resolution() returns -1 when no DPI is set.
        if (DPI == -1)
            DPI = defaultDPI;
    }
    ASSERT(DPI > 0);
    return DPI;
}

static void webkit_web_view_screen_changed(GtkWidget* widget, GdkScreen* previousScreen)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(widget);
    WebKitWebViewPrivate* priv = webView->priv;

    if (priv->disposing)
        return;

    WebKitWebSettings* webSettings = priv->webSettings;
    Settings* settings = core(webView)->settings();
    gdouble DPI = webViewGetDPI(webView);

    guint defaultFontSize, defaultMonospaceFontSize, minimumFontSize, minimumLogicalFontSize;

    g_object_get(webSettings,
                 "default-font-size", &defaultFontSize,
                 "default-monospace-font-size", &defaultMonospaceFontSize,
                 "minimum-font-size", &minimumFontSize,
                 "minimum-logical-font-size", &minimumLogicalFontSize,
                 NULL);

    settings->setDefaultFontSize(defaultFontSize / 72.0 * DPI);
    settings->setDefaultFixedFontSize(defaultMonospaceFontSize / 72.0 * DPI);
    settings->setMinimumFontSize(minimumFontSize / 72.0 * DPI);
    settings->setMinimumLogicalFontSize(minimumLogicalFontSize / 72.0 * DPI);
}

static void webkit_web_view_drag_end(GtkWidget* widget, GdkDragContext* context)
{
    g_object_unref(context);
}

struct DNDContentsRequest
{
    gint info;
    GtkSelectionData* dnd_selection_data;

    gboolean is_url_label_request;
    gchar* url;
};

void clipboard_contents_received(GtkClipboard* clipboard, GtkSelectionData* selection_data, gpointer data)
{
    DNDContentsRequest* contents_request = reinterpret_cast<DNDContentsRequest*>(data);

    if (contents_request->is_url_label_request) {
        // We have received contents of the label clipboard. Use them to form
        // required structures. When formed, enhance the dnd's selection data
        // with them and return.

        // If the label is empty, use the url itself.
        gchar* url_label = reinterpret_cast<gchar*>(gtk_selection_data_get_text(selection_data));
        if (!url_label)
            url_label = g_strdup(contents_request->url);

        gchar* data = 0;
        switch (contents_request->info) {
        case WEBKIT_WEB_VIEW_TARGET_INFO_URI_LIST:
            data = g_strdup_printf("%s\r\n%s\r\n", contents_request->url, url_label);
            break;
        case WEBKIT_WEB_VIEW_TARGET_INFO_NETSCAPE_URL:
            data = g_strdup_printf("%s\n%s", contents_request->url, url_label);
            break;
        }

        if (data) {
            gtk_selection_data_set(contents_request->dnd_selection_data,
                                   contents_request->dnd_selection_data->target, 8,
                                   reinterpret_cast<const guchar*>(data), strlen(data));
            g_free(data);
        }

        g_free(url_label);
        g_free(contents_request->url);
        g_free(contents_request);

        return;
    }

    switch (contents_request->info) {
    case WEBKIT_WEB_VIEW_TARGET_INFO_HTML:
    case WEBKIT_WEB_VIEW_TARGET_INFO_TEXT:
        {
        gchar* data = reinterpret_cast<gchar*>(gtk_selection_data_get_text(selection_data));
        if (data) {
            gtk_selection_data_set(contents_request->dnd_selection_data,
                                   contents_request->dnd_selection_data->target, 8,
                                   reinterpret_cast<const guchar*>(data),
                                   strlen(data));
            g_free(data);
        }
        break;
        }
    case WEBKIT_WEB_VIEW_TARGET_INFO_IMAGE:
        {
        GdkPixbuf* pixbuf = gtk_selection_data_get_pixbuf(selection_data);
        if (pixbuf) {
            gtk_selection_data_set_pixbuf(contents_request->dnd_selection_data, pixbuf);
            g_object_unref(pixbuf);
        }
        break;
        }
    case WEBKIT_WEB_VIEW_TARGET_INFO_URI_LIST:
    case WEBKIT_WEB_VIEW_TARGET_INFO_NETSCAPE_URL:
        // URL's label is stored in another clipboard, so we store URL into
        // contents request, mark the latter as an url label request
        // and request for contents of the label clipboard.
        contents_request->is_url_label_request = TRUE;
        contents_request->url = reinterpret_cast<gchar*>(gtk_selection_data_get_text(selection_data));

        gtk_clipboard_request_contents(gtk_clipboard_get(gdk_atom_intern_static_string("WebKitClipboardUrlLabel")),
                                       selection_data->target, clipboard_contents_received, contents_request);
        break;
    }
}

static void webkit_web_view_drag_data_get(GtkWidget* widget, GdkDragContext* context, GtkSelectionData* selection_data, guint info, guint time_)
{
    GdkAtom selection_atom = GDK_NONE;
    GdkAtom target_atom = selection_data->target;

    switch (info) {
        case WEBKIT_WEB_VIEW_TARGET_INFO_HTML:
            selection_atom = gdk_atom_intern_static_string("WebKitClipboardHtml");
            // HTML markup data is set as text, therefor, we need a text-like target atom
            target_atom = gdk_atom_intern_static_string("UTF8_STRING");
            break;
        case WEBKIT_WEB_VIEW_TARGET_INFO_TEXT:
            selection_atom = gdk_atom_intern_static_string("WebKitClipboardText");
            break;
        case WEBKIT_WEB_VIEW_TARGET_INFO_IMAGE:
            selection_atom = gdk_atom_intern_static_string("WebKitClipboardImage");
            break;
        case WEBKIT_WEB_VIEW_TARGET_INFO_URI_LIST:
        case WEBKIT_WEB_VIEW_TARGET_INFO_NETSCAPE_URL:
            selection_atom = gdk_atom_intern_static_string("WebKitClipboardUrl");
            // We require URL and label, which are both stored in text format
            // and are needed to be retrieved as such.
            target_atom = gdk_atom_intern_static_string("UTF8_STRING");
            break;
    }

    DNDContentsRequest* contents_request = g_new(DNDContentsRequest, 1);
    contents_request->info = info;
    contents_request->is_url_label_request = FALSE;
    contents_request->dnd_selection_data = selection_data;

    gtk_clipboard_request_contents(gtk_clipboard_get(selection_atom), target_atom,
                                   clipboard_contents_received, contents_request);
}

static void webkit_web_view_class_init(WebKitWebViewClass* webViewClass)
{
    GtkBindingSet* binding_set;

    webkit_init();

    /*
     * Signals
     */

    /**
     * WebKitWebView::create-web-view:
     * @web_view: the object on which the signal is emitted
     * @frame: the #WebKitWebFrame
     * @return: a newly allocated #WebKitWebView or %NULL
     *
     * Emitted when the creation of a new window is requested.
     * If this signal is handled the signal handler should return the
     * newly created #WebKitWebView.
     *
     * The new #WebKitWebView should not be displayed to the user
     * until the #WebKitWebView::web-view-ready signal is emitted.
     *
     * The signal handlers should not try to deal with the reference count for
     * the new #WebKitWebView. The widget to which the widget is added will
     * handle that.
     *
     * Since: 1.0.3
     */
    webkit_web_view_signals[CREATE_WEB_VIEW] = g_signal_new("create-web-view",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (WebKitWebViewClass, create_web_view),
            webkit_signal_accumulator_object_handled,
            NULL,
            webkit_marshal_OBJECT__OBJECT,
            WEBKIT_TYPE_WEB_VIEW , 1,
            WEBKIT_TYPE_WEB_FRAME);

    /**
     * WebKitWebView::web-view-ready:
     * @web_view: the object on which the signal is emitted
     * @return: %TRUE to stop other handlers from being invoked for
     * the event, %FALSE to propagate the event further
     *
     * Emitted after #WebKitWebView::create-web-view when the new #WebKitWebView
     * should be displayed to the user. When this signal is emitted
     * all the information about how the window should look, including
     * size, position, whether the location, status and scroll bars
     * should be displayed, is already set on the
     * #WebKitWebWindowFeatures object contained by the #WebKitWebView.
     *
     * Notice that some of that information may change during the life
     * time of the window, so you may want to connect to the ::notify
     * signal of the #WebKitWebWindowFeatures object to handle those.
     *
     * Since: 1.0.3
     */
    webkit_web_view_signals[WEB_VIEW_READY] = g_signal_new("web-view-ready",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (WebKitWebViewClass, web_view_ready),
            g_signal_accumulator_true_handled,
            NULL,
            webkit_marshal_BOOLEAN__VOID,
            G_TYPE_BOOLEAN, 0);

    /**
     * WebKitWebView::close-web-view:
     * @web_view: the object on which the signal is emitted
     * @return: %TRUE to stop handlers from being invoked for the event or
     * %FALSE to propagate the event furter
     *
     * Emitted when closing a #WebKitWebView is requested. This occurs when a
     * call is made from JavaScript's window.close function. The default
     * signal handler does not do anything. It is the owner's responsibility
     * to hide or delete the web view, if necessary.
     *
     * Since: 1.1.11
     */
    webkit_web_view_signals[CLOSE_WEB_VIEW] = g_signal_new("close-web-view",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (WebKitWebViewClass, close_web_view),
            g_signal_accumulator_true_handled,
            NULL,
            webkit_marshal_BOOLEAN__VOID,
            G_TYPE_BOOLEAN, 0);

    /**
     * WebKitWebView::navigation-requested:
     * @web_view: the object on which the signal is emitted
     * @frame: the #WebKitWebFrame that required the navigation
     * @request: a #WebKitNetworkRequest
     * @return: a WebKitNavigationResponse
     *
     * Emitted when @frame requests a navigation to another page.
     *
     * Deprecated: Use WebKitWebView::navigation-policy-decision-requested
     * instead
     */
    webkit_web_view_signals[NAVIGATION_REQUESTED] = g_signal_new("navigation-requested",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (WebKitWebViewClass, navigation_requested),
            webkit_navigation_request_handled,
            NULL,
            webkit_marshal_ENUM__OBJECT_OBJECT,
            WEBKIT_TYPE_NAVIGATION_RESPONSE, 2,
            WEBKIT_TYPE_WEB_FRAME,
            WEBKIT_TYPE_NETWORK_REQUEST);

    /**
     * WebKitWebView::new-window-policy-decision-requested:
     * @web_view: the object on which the signal is emitted
     * @frame: the #WebKitWebFrame that required the navigation
     * @request: a #WebKitNetworkRequest
     * @navigation_action: a #WebKitWebNavigation
     * @policy_decision: a #WebKitWebPolicyDecision
     * @return: TRUE if a decision was made, FALSE to have the
     *          default behavior apply
     *
     * Emitted when @frame requests opening a new window. With this
     * signal the browser can use the context of the request to decide
     * about the new window. If the request is not handled the default
     * behavior is to allow opening the new window to load the URI,
     * which will cause a create-web-view signal emission where the
     * browser handles the new window action but without information
     * of the context that caused the navigation. The following
     * navigation-policy-decision-requested emissions will load the
     * page after the creation of the new window just with the
     * information of this new navigation context, without any
     * information about the action that made this new window to be
     * opened.
     *
     * Notice that if you return TRUE, meaning that you handled the
     * signal, you are expected to have decided what to do, by calling
     * webkit_web_policy_decision_ignore(),
     * webkit_web_policy_decision_use(), or
     * webkit_web_policy_decision_download() on the @policy_decision
     * object.
     *
     * Since: 1.1.4
     */
    webkit_web_view_signals[NEW_WINDOW_POLICY_DECISION_REQUESTED] =
        g_signal_new("new-window-policy-decision-requested",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            g_signal_accumulator_true_handled,
            NULL,
            webkit_marshal_BOOLEAN__OBJECT_OBJECT_OBJECT_OBJECT,
            G_TYPE_BOOLEAN, 4,
            WEBKIT_TYPE_WEB_FRAME,
            WEBKIT_TYPE_NETWORK_REQUEST,
            WEBKIT_TYPE_WEB_NAVIGATION_ACTION,
            WEBKIT_TYPE_WEB_POLICY_DECISION);

    /**
     * WebKitWebView::navigation-policy-decision-requested:
     * @web_view: the object on which the signal is emitted
     * @frame: the #WebKitWebFrame that required the navigation
     * @request: a #WebKitNetworkRequest
     * @navigation_action: a #WebKitWebNavigation
     * @policy_decision: a #WebKitWebPolicyDecision
     * @return: TRUE if a decision was made, FALSE to have the
     *          default behavior apply
     *
     * Emitted when @frame requests a navigation to another page.
     * If this signal is not handled, the default behavior is to allow the
     * navigation.
     *
     * Notice that if you return TRUE, meaning that you handled the
     * signal, you are expected to have decided what to do, by calling
     * webkit_web_policy_decision_ignore(),
     * webkit_web_policy_decision_use(), or
     * webkit_web_policy_decision_download() on the @policy_decision
     * object.
     *
     * Since: 1.0.3
     */
    webkit_web_view_signals[NAVIGATION_POLICY_DECISION_REQUESTED] = g_signal_new("navigation-policy-decision-requested",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            g_signal_accumulator_true_handled,
            NULL,
            webkit_marshal_BOOLEAN__OBJECT_OBJECT_OBJECT_OBJECT,
            G_TYPE_BOOLEAN, 4,
            WEBKIT_TYPE_WEB_FRAME,
            WEBKIT_TYPE_NETWORK_REQUEST,
            WEBKIT_TYPE_WEB_NAVIGATION_ACTION,
            WEBKIT_TYPE_WEB_POLICY_DECISION);

    /**
     * WebKitWebView::mime-type-policy-decision-requested:
     * @web_view: the object on which the signal is emitted
     * @frame: the #WebKitWebFrame that required the policy decision
     * @request: a WebKitNetworkRequest
     * @mimetype: the MIME type attempted to load
     * @policy_decision: a #WebKitWebPolicyDecision
     * @return: TRUE if a decision was made, FALSE to have the
     *          default behavior apply
     *
     * Decide whether or not to display the given MIME type.  If this
     * signal is not handled, the default behavior is to show the
     * content of the requested URI if WebKit can show this MIME
     * type; if WebKit is not able to show the MIME type nothing
     * happens.
     *
     * Notice that if you return TRUE, meaning that you handled the
     * signal, you are expected to have decided what to do, by calling
     * webkit_web_policy_decision_ignore(),
     * webkit_web_policy_decision_use(), or
     * webkit_web_policy_decision_download() on the @policy_decision
     * object.
     *
     * Since: 1.0.3
     */
    webkit_web_view_signals[MIME_TYPE_POLICY_DECISION_REQUESTED] = g_signal_new("mime-type-policy-decision-requested",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            g_signal_accumulator_true_handled,
            NULL,
            webkit_marshal_BOOLEAN__OBJECT_OBJECT_STRING_OBJECT,
            G_TYPE_BOOLEAN, 4,
            WEBKIT_TYPE_WEB_FRAME,
            WEBKIT_TYPE_NETWORK_REQUEST,
            G_TYPE_STRING,
            WEBKIT_TYPE_WEB_POLICY_DECISION);

    /**
     * WebKitWebView::window-object-cleared:
     * @web_view: the object on which the signal is emitted
     * @frame: the #WebKitWebFrame to which @window_object belongs
     * @context: the #JSGlobalContextRef holding the global object and other
     * execution state; equivalent to the return value of
     * webkit_web_frame_get_global_context(@frame)
     *
     * @window_object: the #JSObjectRef representing the frame's JavaScript
     * window object
     *
     * Emitted when the JavaScript window object in a #WebKitWebFrame has been
     * cleared in preparation for a new load. This is the preferred place to
     * set custom properties on the window object using the JavaScriptCore API.
     */
    webkit_web_view_signals[WINDOW_OBJECT_CLEARED] = g_signal_new("window-object-cleared",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (WebKitWebViewClass, window_object_cleared),
            NULL,
            NULL,
            webkit_marshal_VOID__OBJECT_POINTER_POINTER,
            G_TYPE_NONE, 3,
            WEBKIT_TYPE_WEB_FRAME,
            G_TYPE_POINTER,
            G_TYPE_POINTER);

    /**
     * WebKitWebView::download-requested:
     * @web_view: the object on which the signal is emitted
     * @download: a #WebKitDownload object that lets you control the
     * download process
     * @return: %TRUE if the download should be performed, %FALSE to cancel it.
     *
     * A new Download is being requested. By default, if the signal is
     * not handled, the download is cancelled. Notice that while
     * handling this signal you must set the target URI using
     * webkit_download_set_target_uri().
     *
     * If you intend to handle downloads yourself rather than using
     * the #WebKitDownload helper object you must handle this signal,
     * and return %FALSE.
     *
     * Also, keep in mind that the default policy for WebKitGTK+ is to
     * ignore files with a MIME type that it does not know how to
     * handle, which means this signal won't be emitted in the default
     * setup. One way to trigger downloads is to connect to
     * WebKitWebView::mime-type-policy-decision-requested and call
     * webkit_web_policy_decision_download() on the
     * #WebKitWebPolicyDecision in the parameter list for the kind of
     * files you want your application to download (a common solution
     * is to download anything that WebKit can't handle, which you can
     * figure out by using webkit_web_view_can_show_mime_type()).
     *
     * Since: 1.1.2
     */
    webkit_web_view_signals[DOWNLOAD_REQUESTED] = g_signal_new("download-requested",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            g_signal_accumulator_true_handled,
            NULL,
            webkit_marshal_BOOLEAN__OBJECT,
            G_TYPE_BOOLEAN, 1,
            G_TYPE_OBJECT);

    /**
     * WebKitWebView::load-started:
     * @web_view: the object on which the signal is emitted
     * @frame: the frame going to do the load
     *
     * When a #WebKitWebFrame begins to load this signal is emitted.
     */
    webkit_web_view_signals[LOAD_STARTED] = g_signal_new("load-started",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            WEBKIT_TYPE_WEB_FRAME);

    /**
     * WebKitWebView::load-committed:
     * @web_view: the object on which the signal is emitted
     * @frame: the main frame that received the first data
     *
     * When a #WebKitWebFrame loaded the first data this signal is emitted.
     */
    webkit_web_view_signals[LOAD_COMMITTED] = g_signal_new("load-committed",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            WEBKIT_TYPE_WEB_FRAME);


    /**
     * WebKitWebView::load-progress-changed:
     * @web_view: the #WebKitWebView
     * @progress: the global progress
     */
    webkit_web_view_signals[LOAD_PROGRESS_CHANGED] = g_signal_new("load-progress-changed",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__INT,
            G_TYPE_NONE, 1,
            G_TYPE_INT);

    /**
     * WebKitWebView::load-error
     * @web_view: the object on which the signal is emitted
     * @web_frame: the #WebKitWebFrame
     * @uri: the URI that triggered the error
     * @web_error: the #GError that was triggered
     *
     * An error occurred while loading. By default, if the signal is not
     * handled, the @web_view will display a stock error page. You need to
     * handle the signal if you want to provide your own error page.
     *
     * Since: 1.1.6
     */
    webkit_web_view_signals[LOAD_ERROR] = g_signal_new("load-error",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST),
            0,
            g_signal_accumulator_true_handled,
            NULL,
            webkit_marshal_BOOLEAN__OBJECT_STRING_POINTER,
            G_TYPE_BOOLEAN, 3,
            WEBKIT_TYPE_WEB_FRAME,
            G_TYPE_STRING,
            G_TYPE_POINTER);

    webkit_web_view_signals[LOAD_FINISHED] = g_signal_new("load-finished",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            WEBKIT_TYPE_WEB_FRAME);

    /**
     * WebKitWebView::title-changed:
     * @web_view: the object on which the signal is emitted
     * @frame: the main frame
     * @title: the new title
     *
     * When a #WebKitWebFrame changes the document title this signal is emitted.
     *
     * Deprecated: 1.1.4: Use "notify::title" instead.
     */
    webkit_web_view_signals[TITLE_CHANGED] = g_signal_new("title-changed",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            webkit_marshal_VOID__OBJECT_STRING,
            G_TYPE_NONE, 2,
            WEBKIT_TYPE_WEB_FRAME,
            G_TYPE_STRING);

    /**
     * WebKitWebView::hovering-over-link:
     * @web_view: the object on which the signal is emitted
     * @title: the link's title
     * @uri: the URI the link points to
     *
     * When the cursor is over a link, this signal is emitted.
     */
    webkit_web_view_signals[HOVERING_OVER_LINK] = g_signal_new("hovering-over-link",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            webkit_marshal_VOID__STRING_STRING,
            G_TYPE_NONE, 2,
            G_TYPE_STRING,
            G_TYPE_STRING);

    /**
     * WebKitWebView::populate-popup:
     * @web_view: the object on which the signal is emitted
     * @menu: the context menu
     *
     * When a context menu is about to be displayed this signal is emitted.
     *
     * Add menu items to #menu to extend the context menu.
     */
    webkit_web_view_signals[POPULATE_POPUP] = g_signal_new("populate-popup",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            GTK_TYPE_MENU);

    /**
     * WebKitWebView::print-requested
     * @web_view: the object in which the signal is emitted
     * @web_frame: the frame that is requesting to be printed
     * @return: %TRUE if the print request has been handled, %FALSE if
     * the default handler should run
     *
     * Emitted when printing is requested by the frame, usually
     * because of a javascript call. When handling this signal you
     * should call webkit_web_frame_print_full() or
     * webkit_web_frame_print() to do the actual printing.
     *
     * The default handler will present a print dialog and carry a
     * print operation. Notice that this means that if you intend to
     * ignore a print request you must connect to this signal, and
     * return %TRUE.
     *
     * Since: 1.1.5
     */
    webkit_web_view_signals[PRINT_REQUESTED] = g_signal_new("print-requested",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            g_signal_accumulator_true_handled,
            NULL,
            webkit_marshal_BOOLEAN__OBJECT,
            G_TYPE_BOOLEAN, 1,
            WEBKIT_TYPE_WEB_FRAME);

    webkit_web_view_signals[STATUS_BAR_TEXT_CHANGED] = g_signal_new("status-bar-text-changed",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__STRING,
            G_TYPE_NONE, 1,
            G_TYPE_STRING);

    webkit_web_view_signals[ICOND_LOADED] = g_signal_new("icon-loaded",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    webkit_web_view_signals[SELECTION_CHANGED] = g_signal_new("selection-changed",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebView::console-message:
     * @web_view: the object on which the signal is emitted
     * @message: the message text
     * @line: the line where the error occured
     * @source_id: the source id
     * @return: TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
     *
     * A JavaScript console message was created.
     */
    webkit_web_view_signals[CONSOLE_MESSAGE] = g_signal_new("console-message",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebViewClass, console_message),
            g_signal_accumulator_true_handled,
            NULL,
            webkit_marshal_BOOLEAN__STRING_INT_STRING,
            G_TYPE_BOOLEAN, 3,
            G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING);

    /**
     * WebKitWebView::script-alert:
     * @web_view: the object on which the signal is emitted
     * @frame: the relevant frame
     * @message: the message text
     * @return: TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
     *
     * A JavaScript alert dialog was created.
     */
    webkit_web_view_signals[SCRIPT_ALERT] = g_signal_new("script-alert",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebViewClass, script_alert),
            g_signal_accumulator_true_handled,
            NULL,
            webkit_marshal_BOOLEAN__OBJECT_STRING,
            G_TYPE_BOOLEAN, 2,
            WEBKIT_TYPE_WEB_FRAME, G_TYPE_STRING);

    /**
     * WebKitWebView::script-confirm:
     * @web_view: the object on which the signal is emitted
     * @frame: the relevant frame
     * @message: the message text
     * @confirmed: whether the dialog has been confirmed
     * @return: TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
     *
     * A JavaScript confirm dialog was created, providing Yes and No buttons.
     */
    webkit_web_view_signals[SCRIPT_CONFIRM] = g_signal_new("script-confirm",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebViewClass, script_confirm),
            g_signal_accumulator_true_handled,
            NULL,
            webkit_marshal_BOOLEAN__OBJECT_STRING_POINTER,
            G_TYPE_BOOLEAN, 3,
            WEBKIT_TYPE_WEB_FRAME, G_TYPE_STRING, G_TYPE_POINTER);

    /**
     * WebKitWebView::script-prompt:
     * @web_view: the object on which the signal is emitted
     * @frame: the relevant frame
     * @message: the message text
     * @default: the default value
     * @text: To be filled with the return value or NULL if the dialog was cancelled.
     * @return: TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
     *
     * A JavaScript prompt dialog was created, providing an entry to input text.
     */
    webkit_web_view_signals[SCRIPT_PROMPT] = g_signal_new("script-prompt",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebViewClass, script_prompt),
            g_signal_accumulator_true_handled,
            NULL,
            webkit_marshal_BOOLEAN__OBJECT_STRING_STRING_STRING,
            G_TYPE_BOOLEAN, 4,
            WEBKIT_TYPE_WEB_FRAME, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

    /**
     * WebKitWebView::select-all:
     * @web_view: the object which received the signal
     *
     * The #WebKitWebView::select-all signal is a keybinding signal which gets emitted to
     * select the complete contents of the text view.
     *
     * The default bindings for this signal is Ctrl-a.
     */
    webkit_web_view_signals[SELECT_ALL] = g_signal_new("select-all",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            G_STRUCT_OFFSET(WebKitWebViewClass, select_all),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebView::cut-clipboard:
     * @web_view: the object which received the signal
     *
     * The #WebKitWebView::cut-clipboard signal is a keybinding signal which gets emitted to
     * cut the selection to the clipboard.
     *
     * The default bindings for this signal are Ctrl-x and Shift-Delete.
     */
    webkit_web_view_signals[CUT_CLIPBOARD] = g_signal_new("cut-clipboard",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            G_STRUCT_OFFSET(WebKitWebViewClass, cut_clipboard),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebView::copy-clipboard:
     * @web_view: the object which received the signal
     *
     * The #WebKitWebView::copy-clipboard signal is a keybinding signal which gets emitted to
     * copy the selection to the clipboard.
     *
     * The default bindings for this signal are Ctrl-c and Ctrl-Insert.
     */
    webkit_web_view_signals[COPY_CLIPBOARD] = g_signal_new("copy-clipboard",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            G_STRUCT_OFFSET(WebKitWebViewClass, copy_clipboard),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebView::paste-clipboard:
     * @web_view: the object which received the signal
     *
     * The #WebKitWebView::paste-clipboard signal is a keybinding signal which gets emitted to
     * paste the contents of the clipboard into the Web view.
     *
     * The default bindings for this signal are Ctrl-v and Shift-Insert.
     */
    webkit_web_view_signals[PASTE_CLIPBOARD] = g_signal_new("paste-clipboard",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            G_STRUCT_OFFSET(WebKitWebViewClass, paste_clipboard),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebView::undo
     * @web_view: the object which received the signal
     *
     * The #WebKitWebView::undo signal is a keybinding signal which gets emitted to
     * undo the last editing command.
     *
     * The default binding for this signal is Ctrl-z
     *
     * Since: 1.1.14
     */
    webkit_web_view_signals[UNDO] = g_signal_new("undo",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            G_STRUCT_OFFSET(WebKitWebViewClass, undo),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebView::redo
     * @web_view: the object which received the signal
     *
     * The #WebKitWebView::redo signal is a keybinding signal which gets emitted to
     * redo the last editing command.
     *
     * The default binding for this signal is Ctrl-Shift-z
     *
     * Since: 1.1.14
     */
    webkit_web_view_signals[REDO] = g_signal_new("redo",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            G_STRUCT_OFFSET(WebKitWebViewClass, redo),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebView::move-cursor:
     * @web_view: the object which received the signal
     * @step: the type of movement, one of #GtkMovementStep
     * @count: an integer indicating the subtype of movement. Currently
     *         the permitted values are '1' = forward, '-1' = backwards.
     *
     * The #WebKitWebView::move-cursor will be emitted to apply the
     * cursor movement described by its parameters to the @view.
     * 
     * Since: 1.1.4
     */
    webkit_web_view_signals[MOVE_CURSOR] = g_signal_new("move-cursor",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            G_STRUCT_OFFSET(WebKitWebViewClass, move_cursor),
            NULL, NULL,
            webkit_marshal_BOOLEAN__ENUM_INT,
            G_TYPE_BOOLEAN, 2,
            GTK_TYPE_MOVEMENT_STEP,
            G_TYPE_INT);

    /**
     * WebKitWebView::create-plugin-widget:
     * @web_view: the object which received the signal
     * @mime_type: the mimetype of the requested object
     * @uri: the URI to load
     * @param: a #GHashTable with additional attributes (strings)
     *
     * The #WebKitWebView::create-plugin signal will be emitted to
     * create a plugin widget for embed or object HTML tags. This
     * allows to embed a GtkWidget as a plugin into HTML content. In
     * case of a textual selection of the GtkWidget WebCore will attempt
     * to set the property value of "webkit-widget-is-selected". This can
     * be used to draw a visual indicator of the selection.
     *
     * Since: 1.1.8
     */
    webkit_web_view_signals[PLUGIN_WIDGET] = g_signal_new("create-plugin-widget",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags) (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            webkit_signal_accumulator_object_handled,
            NULL,
            webkit_marshal_OBJECT__STRING_STRING_POINTER,
            GTK_TYPE_WIDGET, 3,
            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_HASH_TABLE);

    /**
     * WebKitWebView::database-quota-exceeded
     * @web_view: the object which received the signal
     * @frame: the relevant frame
     * @database: the #WebKitWebDatabase which exceeded the quota of its #WebKitSecurityOrigin
     *
     * The #WebKitWebView::database-exceeded-quota signal will be emitted when
     * a Web Database exceeds the quota of its security origin. This signal
     * may be used to increase the size of the quota before the originating
     * operation fails.
     *
     * Since: 1.1.14
     */
    webkit_web_view_signals[DATABASE_QUOTA_EXCEEDED] = g_signal_new("database-quota-exceeded",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags) (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL, NULL,
            webkit_marshal_VOID__OBJECT_OBJECT,
            G_TYPE_NONE, 2,
            G_TYPE_OBJECT, G_TYPE_OBJECT);

    /**
     * WebKitWebView::resource-request-starting:
     * @web_view: the object which received the signal
     * @web_frame: the #WebKitWebFrame whose load dispatched this request
     * @web_resource: an empty #WebKitWebResource object
     * @request: the #WebKitNetworkRequest that will be dispatched
     * @response: the #WebKitNetworkResponse representing the redirect
     * response, if any
     *
     * Emitted when a request is about to be sent. You can modify the
     * request while handling this signal. You can set the URI in the
     * #WebKitNetworkRequest object itself, and add/remove/replace
     * headers using the #SoupMessage object it carries, if it is
     * present. See webkit_network_request_get_message(). Setting the
     * request URI to "about:blank" will effectively cause the request
     * to load nothing, and can be used to disable the loading of
     * specific resources.
     *
     * Notice that information about an eventual redirect is available
     * in @response's #SoupMessage, not in the #SoupMessage carried by
     * the @request. If @response is %NULL, then this is not a
     * redirected request.
     *
     * The #WebKitWebResource object will be the same throughout all
     * the lifetime of the resource, but the contents may change from
     * inbetween signal emissions.
     *
     * Since: 1.1.14
     */
    webkit_web_view_signals[RESOURCE_REQUEST_STARTING] = g_signal_new("resource-request-starting",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL, NULL,
            webkit_marshal_VOID__OBJECT_OBJECT_OBJECT_OBJECT,
            G_TYPE_NONE, 4,
            WEBKIT_TYPE_WEB_FRAME,
            WEBKIT_TYPE_WEB_RESOURCE,
            WEBKIT_TYPE_NETWORK_REQUEST,
            WEBKIT_TYPE_NETWORK_RESPONSE);

    /*
     * implementations of virtual methods
     */
    webViewClass->create_web_view = webkit_web_view_real_create_web_view;
    webViewClass->web_view_ready = webkit_web_view_real_web_view_ready;
    webViewClass->close_web_view = webkit_web_view_real_close_web_view;
    webViewClass->navigation_requested = webkit_web_view_real_navigation_requested;
    webViewClass->window_object_cleared = webkit_web_view_real_window_object_cleared;
    webViewClass->choose_file = webkit_web_view_real_choose_file;
    webViewClass->script_alert = webkit_web_view_real_script_alert;
    webViewClass->script_confirm = webkit_web_view_real_script_confirm;
    webViewClass->script_prompt = webkit_web_view_real_script_prompt;
    webViewClass->console_message = webkit_web_view_real_console_message;
    webViewClass->select_all = webkit_web_view_real_select_all;
    webViewClass->cut_clipboard = webkit_web_view_real_cut_clipboard;
    webViewClass->copy_clipboard = webkit_web_view_real_copy_clipboard;
    webViewClass->paste_clipboard = webkit_web_view_real_paste_clipboard;
    webViewClass->undo = webkit_web_view_real_undo;
    webViewClass->redo = webkit_web_view_real_redo;
    webViewClass->move_cursor = webkit_web_view_real_move_cursor;

    GObjectClass* objectClass = G_OBJECT_CLASS(webViewClass);
    objectClass->dispose = webkit_web_view_dispose;
    objectClass->finalize = webkit_web_view_finalize;
    objectClass->get_property = webkit_web_view_get_property;
    objectClass->set_property = webkit_web_view_set_property;

    GtkWidgetClass* widgetClass = GTK_WIDGET_CLASS(webViewClass);
    widgetClass->realize = webkit_web_view_realize;
    widgetClass->expose_event = webkit_web_view_expose_event;
    widgetClass->key_press_event = webkit_web_view_key_press_event;
    widgetClass->key_release_event = webkit_web_view_key_release_event;
    widgetClass->button_press_event = webkit_web_view_button_press_event;
    widgetClass->button_release_event = webkit_web_view_button_release_event;
    widgetClass->motion_notify_event = webkit_web_view_motion_event;
    widgetClass->scroll_event = webkit_web_view_scroll_event;
    widgetClass->size_allocate = webkit_web_view_size_allocate;
    widgetClass->size_request = webkit_web_view_size_request;
    widgetClass->popup_menu = webkit_web_view_popup_menu_handler;
    widgetClass->grab_focus = webkit_web_view_grab_focus;
    widgetClass->focus_in_event = webkit_web_view_focus_in_event;
    widgetClass->focus_out_event = webkit_web_view_focus_out_event;
    widgetClass->get_accessible = webkit_web_view_get_accessible;
    widgetClass->screen_changed = webkit_web_view_screen_changed;
    widgetClass->drag_end = webkit_web_view_drag_end;
    widgetClass->drag_data_get = webkit_web_view_drag_data_get;

    GtkContainerClass* containerClass = GTK_CONTAINER_CLASS(webViewClass);
    containerClass->add = webkit_web_view_container_add;
    containerClass->remove = webkit_web_view_container_remove;
    containerClass->forall = webkit_web_view_container_forall;

    /*
     * make us scrollable (e.g. addable to a GtkScrolledWindow)
     */
    webViewClass->set_scroll_adjustments = webkit_web_view_set_scroll_adjustments;
    GTK_WIDGET_CLASS(webViewClass)->set_scroll_adjustments_signal = g_signal_new("set-scroll-adjustments",
            G_TYPE_FROM_CLASS(webViewClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            G_STRUCT_OFFSET(WebKitWebViewClass, set_scroll_adjustments),
            NULL, NULL,
            webkit_marshal_VOID__OBJECT_OBJECT,
            G_TYPE_NONE, 2,
            GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);

    /*
     * Key bindings
     */

    binding_set = gtk_binding_set_by_class(webViewClass);

    gtk_binding_entry_add_signal(binding_set, GDK_a, GDK_CONTROL_MASK,
                                 "select_all", 0);

    /* Cut/copy/paste */

    gtk_binding_entry_add_signal(binding_set, GDK_x, GDK_CONTROL_MASK,
                                 "cut_clipboard", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_c, GDK_CONTROL_MASK,
                                 "copy_clipboard", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_v, GDK_CONTROL_MASK,
                                 "paste_clipboard", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_z, GDK_CONTROL_MASK,
                                 "undo", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_z, static_cast<GdkModifierType>(GDK_CONTROL_MASK | GDK_SHIFT_MASK),
                                 "redo", 0);

    gtk_binding_entry_add_signal(binding_set, GDK_Delete, GDK_SHIFT_MASK,
                                 "cut_clipboard", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_Insert, GDK_CONTROL_MASK,
                                 "copy_clipboard", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_Insert, GDK_SHIFT_MASK,
                                 "paste_clipboard", 0);

    /* Movement */
    
    gtk_binding_entry_add_signal(binding_set, GDK_Down, static_cast<GdkModifierType>(0),
                                 "move-cursor", 2,
                                 G_TYPE_ENUM, GTK_MOVEMENT_DISPLAY_LINES,
                                 G_TYPE_INT, 1);
    gtk_binding_entry_add_signal(binding_set, GDK_Up, static_cast<GdkModifierType>(0),
                                 "move-cursor", 2,
                                 G_TYPE_ENUM, GTK_MOVEMENT_DISPLAY_LINES,
                                 G_TYPE_INT, -1);
    gtk_binding_entry_add_signal(binding_set, GDK_Right, static_cast<GdkModifierType>(0),
                                 "move-cursor", 2,
                                 G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
                                 G_TYPE_INT, 1);
    gtk_binding_entry_add_signal(binding_set, GDK_Left, static_cast<GdkModifierType>(0),
                                 "move-cursor", 2,
                                 G_TYPE_ENUM, GTK_MOVEMENT_VISUAL_POSITIONS,
                                 G_TYPE_INT, -1);
    gtk_binding_entry_add_signal(binding_set, GDK_space, static_cast<GdkModifierType>(0),
                                 "move-cursor", 2,
                                 G_TYPE_ENUM, GTK_MOVEMENT_PAGES,
                                 G_TYPE_INT, 1);
    gtk_binding_entry_add_signal(binding_set, GDK_space, GDK_SHIFT_MASK,
                                 "move-cursor", 2,
                                 G_TYPE_ENUM, GTK_MOVEMENT_PAGES,
                                 G_TYPE_INT, -1);
    gtk_binding_entry_add_signal(binding_set, GDK_Page_Down, static_cast<GdkModifierType>(0),
                                 "move-cursor", 2,
                                 G_TYPE_ENUM, GTK_MOVEMENT_PAGES,
                                 G_TYPE_INT, 1);
    gtk_binding_entry_add_signal(binding_set, GDK_Page_Up, static_cast<GdkModifierType>(0),
                                 "move-cursor", 2,
                                 G_TYPE_ENUM, GTK_MOVEMENT_PAGES,
                                 G_TYPE_INT, -1);
    gtk_binding_entry_add_signal(binding_set, GDK_End, static_cast<GdkModifierType>(0),
                                 "move-cursor", 2,
                                 G_TYPE_ENUM, GTK_MOVEMENT_BUFFER_ENDS,
                                 G_TYPE_INT, 1);
    gtk_binding_entry_add_signal(binding_set, GDK_Home, static_cast<GdkModifierType>(0),
                                 "move-cursor", 2,
                                 G_TYPE_ENUM, GTK_MOVEMENT_BUFFER_ENDS,
                                 G_TYPE_INT, -1);

    /*
     * properties
     */

    /**
    * WebKitWebView:title:
    *
    * Returns the @web_view's document title.
    *
    * Since: 1.1.4
    */
    g_object_class_install_property(objectClass, PROP_TITLE,
                                    g_param_spec_string("title",
                                                        _("Title"),
                                                        _("Returns the @web_view's document title"),
                                                        NULL,
                                                        WEBKIT_PARAM_READABLE));

    /**
    * WebKitWebView:uri:
    *
    * Returns the current URI of the contents displayed by the @web_view.
    *
    * Since: 1.1.4
    */
    g_object_class_install_property(objectClass, PROP_URI,
                                    g_param_spec_string("uri",
                                                        _("URI"),
                                                        _("Returns the current URI of the contents displayed by the @web_view"),
                                                        NULL,
                                                        WEBKIT_PARAM_READABLE));

    /**
    * WebKitWebView:copy-target-list:
    *
    * The list of targets this web view supports for clipboard copying.
    *
    * Since: 1.0.2
    */
    g_object_class_install_property(objectClass, PROP_COPY_TARGET_LIST,
                                    g_param_spec_boxed("copy-target-list",
                                                       _("Copy target list"),
                                                       _("The list of targets this web view supports for clipboard copying"),
                                                       GTK_TYPE_TARGET_LIST,
                                                       WEBKIT_PARAM_READABLE));

    /**
    * WebKitWebView:paste-target-list:
    *
    * The list of targets this web view supports for clipboard pasting.
    *
    * Since: 1.0.2
    */
    g_object_class_install_property(objectClass, PROP_PASTE_TARGET_LIST,
                                    g_param_spec_boxed("paste-target-list",
                                                       _("Paste target list"),
                                                       _("The list of targets this web view supports for clipboard pasting"),
                                                       GTK_TYPE_TARGET_LIST,
                                                       WEBKIT_PARAM_READABLE));

    g_object_class_install_property(objectClass, PROP_SETTINGS,
                                    g_param_spec_object("settings",
                                                        _("Settings"),
                                                        _("An associated WebKitWebSettings instance"),
                                                        WEBKIT_TYPE_WEB_SETTINGS,
                                                        WEBKIT_PARAM_READWRITE));

    /**
    * WebKitWebView:web-inspector:
    *
    * The associated WebKitWebInspector instance.
    *
    * Since: 1.0.3
    */
    g_object_class_install_property(objectClass, PROP_WEB_INSPECTOR,
                                    g_param_spec_object("web-inspector",
                                                        _("Web Inspector"),
                                                        _("The associated WebKitWebInspector instance"),
                                                        WEBKIT_TYPE_WEB_INSPECTOR,
                                                        WEBKIT_PARAM_READABLE));

    /**
    * WebKitWebView:window-features:
    *
    * An associated WebKitWebWindowFeatures instance.
    *
    * Since: 1.0.3
    */
    g_object_class_install_property(objectClass, PROP_WINDOW_FEATURES,
                                    g_param_spec_object("window-features",
                                                        "Window Features",
                                                        "An associated WebKitWebWindowFeatures instance",
                                                        WEBKIT_TYPE_WEB_WINDOW_FEATURES,
                                                        WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(objectClass, PROP_EDITABLE,
                                    g_param_spec_boolean("editable",
                                                         _("Editable"),
                                                         _("Whether content can be modified by the user"),
                                                         FALSE,
                                                         WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(objectClass, PROP_TRANSPARENT,
                                    g_param_spec_boolean("transparent",
                                                         _("Transparent"),
                                                         _("Whether content has a transparent background"),
                                                         FALSE,
                                                         WEBKIT_PARAM_READWRITE));

    /**
    * WebKitWebView:zoom-level:
    *
    * The level of zoom of the content.
    *
    * Since: 1.0.1
    */
    g_object_class_install_property(objectClass, PROP_ZOOM_LEVEL,
                                    g_param_spec_float("zoom-level",
                                                       _("Zoom level"),
                                                       _("The level of zoom of the content"),
                                                       G_MINFLOAT,
                                                       G_MAXFLOAT,
                                                       1.0f,
                                                       WEBKIT_PARAM_READWRITE));

    /**
    * WebKitWebView:full-content-zoom:
    *
    * Whether the full content is scaled when zooming.
    *
    * Since: 1.0.1
    */
    g_object_class_install_property(objectClass, PROP_FULL_CONTENT_ZOOM,
                                    g_param_spec_boolean("full-content-zoom",
                                                         _("Full content zoom"),
                                                         _("Whether the full content is scaled when zooming"),
                                                         FALSE,
                                                         WEBKIT_PARAM_READWRITE));

    /**
     * WebKitWebView:encoding:
     *
     * The default encoding of the web view.
     *
     * Since: 1.1.2
     */
    g_object_class_install_property(objectClass, PROP_ENCODING,
                                    g_param_spec_string("encoding",
                                                        _("Encoding"),
                                                        _("The default encoding of the web view"),
                                                        NULL,
                                                        WEBKIT_PARAM_READABLE));

    /**
     * WebKitWebView:custom-encoding:
     *
     * The custom encoding of the web view.
     *
     * Since: 1.1.2
     */
    g_object_class_install_property(objectClass, PROP_CUSTOM_ENCODING,
                                    g_param_spec_string("custom-encoding",
                                                        _("Custom Encoding"),
                                                        _("The custom encoding of the web view"),
                                                        NULL,
                                                        WEBKIT_PARAM_READWRITE));

    /**
    * WebKitWebView:load-status:
    *
    * Determines the current status of the load.
    *
    * Since: 1.1.7
    */
    g_object_class_install_property(objectClass, PROP_LOAD_STATUS,
                                    g_param_spec_enum("load-status",
                                                      "Load Status",
                                                      "Determines the current status of the load",
                                                      WEBKIT_TYPE_LOAD_STATUS,
                                                      WEBKIT_LOAD_FINISHED,
                                                      WEBKIT_PARAM_READABLE));

    /**
    * WebKitWebView:progress:
    *
    * Determines the current progress of the load.
    *
    * Since: 1.1.7
    */
    g_object_class_install_property(objectClass, PROP_PROGRESS,
                                    g_param_spec_double("progress",
                                                        "Progress",
                                                        "Determines the current progress of the load",
                                                        0.0, 1.0, 1.0,
                                                        WEBKIT_PARAM_READABLE));

    g_type_class_add_private(webViewClass, sizeof(WebKitWebViewPrivate));
}

static void webkit_web_view_update_settings(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = webView->priv;
    WebKitWebSettings* webSettings = priv->webSettings;
    Settings* settings = core(webView)->settings();

    gchar* defaultEncoding, *cursiveFontFamily, *defaultFontFamily, *fantasyFontFamily, *monospaceFontFamily, *sansSerifFontFamily, *serifFontFamily, *userStylesheetUri;
    gboolean autoLoadImages, autoShrinkImages, printBackgrounds,
        enableScripts, enablePlugins, enableDeveloperExtras, resizableTextAreas,
        enablePrivateBrowsing, enableCaretBrowsing, enableHTML5Database, enableHTML5LocalStorage,
        enableXSSAuditor, javascriptCanOpenWindows, enableOfflineWebAppCache,
        enableUniversalAccessFromFileURI, enableDOMPaste;

    WebKitEditingBehavior editingBehavior;

    g_object_get(webSettings,
                 "default-encoding", &defaultEncoding,
                 "cursive-font-family", &cursiveFontFamily,
                 "default-font-family", &defaultFontFamily,
                 "fantasy-font-family", &fantasyFontFamily,
                 "monospace-font-family", &monospaceFontFamily,
                 "sans-serif-font-family", &sansSerifFontFamily,
                 "serif-font-family", &serifFontFamily,
                 "auto-load-images", &autoLoadImages,
                 "auto-shrink-images", &autoShrinkImages,
                 "print-backgrounds", &printBackgrounds,
                 "enable-scripts", &enableScripts,
                 "enable-plugins", &enablePlugins,
                 "resizable-text-areas", &resizableTextAreas,
                 "user-stylesheet-uri", &userStylesheetUri,
                 "enable-developer-extras", &enableDeveloperExtras,
                 "enable-private-browsing", &enablePrivateBrowsing,
                 "enable-caret-browsing", &enableCaretBrowsing,
                 "enable-html5-database", &enableHTML5Database,
                 "enable-html5-local-storage", &enableHTML5LocalStorage,
                 "enable-xss-auditor", &enableXSSAuditor,
                 "javascript-can-open-windows-automatically", &javascriptCanOpenWindows,
                 "enable-offline-web-application-cache", &enableOfflineWebAppCache,
                 "editing-behavior", &editingBehavior,
                 "enable-universal-access-from-file-uris", &enableUniversalAccessFromFileURI,
                 "enable-dom-paste", &enableDOMPaste,
                 NULL);

    settings->setDefaultTextEncodingName(defaultEncoding);
    settings->setCursiveFontFamily(cursiveFontFamily);
    settings->setStandardFontFamily(defaultFontFamily);
    settings->setFantasyFontFamily(fantasyFontFamily);
    settings->setFixedFontFamily(monospaceFontFamily);
    settings->setSansSerifFontFamily(sansSerifFontFamily);
    settings->setSerifFontFamily(serifFontFamily);
    settings->setLoadsImagesAutomatically(autoLoadImages);
    settings->setShrinksStandaloneImagesToFit(autoShrinkImages);
    settings->setShouldPrintBackgrounds(printBackgrounds);
    settings->setJavaScriptEnabled(enableScripts);
    settings->setPluginsEnabled(enablePlugins);
    settings->setTextAreasAreResizable(resizableTextAreas);
    settings->setUserStyleSheetLocation(KURL(KURL(), userStylesheetUri));
    settings->setDeveloperExtrasEnabled(enableDeveloperExtras);
    settings->setPrivateBrowsingEnabled(enablePrivateBrowsing);
    settings->setCaretBrowsingEnabled(enableCaretBrowsing);
    settings->setDatabasesEnabled(enableHTML5Database);
    settings->setLocalStorageEnabled(enableHTML5LocalStorage);
    settings->setXSSAuditorEnabled(enableXSSAuditor);
    settings->setJavaScriptCanOpenWindowsAutomatically(javascriptCanOpenWindows);
    settings->setOfflineWebApplicationCacheEnabled(enableOfflineWebAppCache);
    settings->setEditingBehavior(core(editingBehavior));
    settings->setAllowUniversalAccessFromFileURLs(enableUniversalAccessFromFileURI);
    settings->setDOMPasteAllowed(enableDOMPaste);

    g_free(defaultEncoding);
    g_free(cursiveFontFamily);
    g_free(defaultFontFamily);
    g_free(fantasyFontFamily);
    g_free(monospaceFontFamily);
    g_free(sansSerifFontFamily);
    g_free(serifFontFamily);
    g_free(userStylesheetUri);

    webkit_web_view_screen_changed(GTK_WIDGET(webView), NULL);
}

static inline gint pixelsFromSize(WebKitWebView* webView, gint size)
{
    gdouble DPI = webViewGetDPI(webView);
    return size / 72.0 * DPI;
}

static void webkit_web_view_settings_notify(WebKitWebSettings* webSettings, GParamSpec* pspec, WebKitWebView* webView)
{
    Settings* settings = core(webView)->settings();

    const gchar* name = g_intern_string(pspec->name);
    GValue value = { 0, { { 0 } } };
    g_value_init(&value, pspec->value_type);
    g_object_get_property(G_OBJECT(webSettings), name, &value);

    if (name == g_intern_string("default-encoding"))
        settings->setDefaultTextEncodingName(g_value_get_string(&value));
    else if (name == g_intern_string("cursive-font-family"))
        settings->setCursiveFontFamily(g_value_get_string(&value));
    else if (name == g_intern_string("default-font-family"))
        settings->setStandardFontFamily(g_value_get_string(&value));
    else if (name == g_intern_string("fantasy-font-family"))
        settings->setFantasyFontFamily(g_value_get_string(&value));
    else if (name == g_intern_string("monospace-font-family"))
        settings->setFixedFontFamily(g_value_get_string(&value));
    else if (name == g_intern_string("sans-serif-font-family"))
        settings->setSansSerifFontFamily(g_value_get_string(&value));
    else if (name == g_intern_string("serif-font-family"))
        settings->setSerifFontFamily(g_value_get_string(&value));
    else if (name == g_intern_string("default-font-size"))
        settings->setDefaultFontSize(pixelsFromSize(webView, g_value_get_int(&value)));
    else if (name == g_intern_string("default-monospace-font-size"))
        settings->setDefaultFixedFontSize(pixelsFromSize(webView, g_value_get_int(&value)));
    else if (name == g_intern_string("minimum-font-size"))
        settings->setMinimumFontSize(pixelsFromSize(webView, g_value_get_int(&value)));
    else if (name == g_intern_string("minimum-logical-font-size"))
        settings->setMinimumLogicalFontSize(pixelsFromSize(webView, g_value_get_int(&value)));
    else if (name == g_intern_string("enforce-96-dpi"))
        webkit_web_view_screen_changed(GTK_WIDGET(webView), NULL);
    else if (name == g_intern_string("auto-load-images"))
        settings->setLoadsImagesAutomatically(g_value_get_boolean(&value));
    else if (name == g_intern_string("auto-shrink-images"))
        settings->setShrinksStandaloneImagesToFit(g_value_get_boolean(&value));
    else if (name == g_intern_string("print-backgrounds"))
        settings->setShouldPrintBackgrounds(g_value_get_boolean(&value));
    else if (name == g_intern_string("enable-scripts"))
        settings->setJavaScriptEnabled(g_value_get_boolean(&value));
    else if (name == g_intern_string("enable-plugins"))
        settings->setPluginsEnabled(g_value_get_boolean(&value));
    else if (name == g_intern_string("resizable-text-areas"))
        settings->setTextAreasAreResizable(g_value_get_boolean(&value));
    else if (name == g_intern_string("user-stylesheet-uri"))
        settings->setUserStyleSheetLocation(KURL(KURL(), g_value_get_string(&value)));
    else if (name == g_intern_string("enable-developer-extras"))
        settings->setDeveloperExtrasEnabled(g_value_get_boolean(&value));
    else if (name == g_intern_string("enable-private-browsing"))
        settings->setPrivateBrowsingEnabled(g_value_get_boolean(&value));
    else if (name == g_intern_string("enable-caret-browsing"))
        settings->setCaretBrowsingEnabled(g_value_get_boolean(&value));
    else if (name == g_intern_string("enable-html5-database"))
        settings->setDatabasesEnabled(g_value_get_boolean(&value));
    else if (name == g_intern_string("enable-html5-local-storage"))
        settings->setLocalStorageEnabled(g_value_get_boolean(&value));
    else if (name == g_intern_string("enable-xss-auditor"))
        settings->setXSSAuditorEnabled(g_value_get_boolean(&value));
    else if (name == g_intern_string("javascript-can-open-windows-automatically"))
        settings->setJavaScriptCanOpenWindowsAutomatically(g_value_get_boolean(&value));
    else if (name == g_intern_string("enable-offline-web-application-cache"))
        settings->setOfflineWebApplicationCacheEnabled(g_value_get_boolean(&value));
    else if (name == g_intern_string("editing-behavior"))
        settings->setEditingBehavior(core(static_cast<WebKitEditingBehavior>(g_value_get_enum(&value))));
    else if (name == g_intern_string("enable-universal-access-from-file-uris"))
        settings->setAllowUniversalAccessFromFileURLs(g_value_get_boolean(&value));
    else if (name == g_intern_string("enable-dom-paste"))
        settings->setDOMPasteAllowed(g_value_get_boolean(&value));
    else if (!g_object_class_find_property(G_OBJECT_GET_CLASS(webSettings), name))
        g_warning("Unexpected setting '%s'", name);
    g_value_unset(&value);
}

static void webkit_web_view_init(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = WEBKIT_WEB_VIEW_GET_PRIVATE(webView);
    webView->priv = priv;

    priv->imContext = gtk_im_multicontext_new();

    WebKit::InspectorClient* inspectorClient = new WebKit::InspectorClient(webView);
    priv->corePage = new Page(new WebKit::ChromeClient(webView), new WebKit::ContextMenuClient(webView), new WebKit::EditorClient(webView), new WebKit::DragClient(webView), inspectorClient, 0);

    // We also add a simple wrapper class to provide the public
    // interface for the Web Inspector.
    priv->webInspector = WEBKIT_WEB_INSPECTOR(g_object_new(WEBKIT_TYPE_WEB_INSPECTOR, NULL));
    webkit_web_inspector_set_inspector_client(priv->webInspector, priv->corePage);

    priv->horizontalAdjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    priv->verticalAdjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

    g_object_ref_sink(priv->horizontalAdjustment);
    g_object_ref_sink(priv->verticalAdjustment);

    GTK_WIDGET_SET_FLAGS(webView, GTK_CAN_FOCUS);
    priv->mainFrame = WEBKIT_WEB_FRAME(webkit_web_frame_new(webView));
    priv->lastPopupXPosition = priv->lastPopupYPosition = -1;
    priv->editable = false;

    priv->backForwardList = webkit_web_back_forward_list_new_with_web_view(webView);

    priv->zoomFullContent = FALSE;

    GdkAtom textHtml = gdk_atom_intern_static_string("text/html");
    /* Targets for copy */
    priv->copy_target_list = gtk_target_list_new(NULL, 0);
    gtk_target_list_add(priv->copy_target_list, textHtml, 0, WEBKIT_WEB_VIEW_TARGET_INFO_HTML);
    gtk_target_list_add_text_targets(priv->copy_target_list, WEBKIT_WEB_VIEW_TARGET_INFO_TEXT);

    /* Targets for pasting */
    priv->paste_target_list = gtk_target_list_new(NULL, 0);
    gtk_target_list_add(priv->paste_target_list, textHtml, 0, WEBKIT_WEB_VIEW_TARGET_INFO_HTML);
    gtk_target_list_add_text_targets(priv->paste_target_list, WEBKIT_WEB_VIEW_TARGET_INFO_TEXT);

    priv->webSettings = webkit_web_settings_new();
    webkit_web_view_update_settings(webView);
    g_signal_connect(priv->webSettings, "notify", G_CALLBACK(webkit_web_view_settings_notify), webView);

    priv->webWindowFeatures = webkit_web_window_features_new();

    priv->subResources = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
}

GtkWidget* webkit_web_view_new(void)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW, NULL));

    return GTK_WIDGET(webView);
}

// for internal use only
void webkit_web_view_notify_ready(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    gboolean isHandled = FALSE;
    g_signal_emit(webView, webkit_web_view_signals[WEB_VIEW_READY], 0, &isHandled);
}

void webkit_web_view_request_download(WebKitWebView* webView, WebKitNetworkRequest* request, const ResourceResponse& response, ResourceHandle* handle)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WebKitDownload* download;

    if (handle)
        download = webkit_download_new_with_handle(request, handle, response);
    else
        download = webkit_download_new(request);

    gboolean handled;
    g_signal_emit(webView, webkit_web_view_signals[DOWNLOAD_REQUESTED], 0, download, &handled);

    if (!handled) {
        webkit_download_cancel(download);
        g_object_unref(download);
        return;
    }

    webkit_download_start(download);
}

bool webkit_web_view_use_primary_for_paste(WebKitWebView* webView)
{
    return webView->priv->usePrimaryForPaste;
}

void webkit_web_view_set_settings(WebKitWebView* webView, WebKitWebSettings* webSettings)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(WEBKIT_IS_WEB_SETTINGS(webSettings));

    WebKitWebViewPrivate* priv = webView->priv;
    g_signal_handlers_disconnect_by_func(priv->webSettings, (gpointer)webkit_web_view_settings_notify, webView);
    g_object_unref(priv->webSettings);
    g_object_ref(webSettings);
    priv->webSettings = webSettings;
    webkit_web_view_update_settings(webView);
    g_signal_connect(webSettings, "notify", G_CALLBACK(webkit_web_view_settings_notify), webView);
    g_object_notify(G_OBJECT(webView), "settings");
}

WebKitWebSettings* webkit_web_view_get_settings(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    WebKitWebViewPrivate* priv = webView->priv;
    return priv->webSettings;
}

/**
 * webkit_web_view_get_inspector:
 * @web_view: a #WebKitWebView
 *
 * Obtains the #WebKitWebInspector associated with the
 * #WebKitWebView. Every #WebKitWebView object has a
 * #WebKitWebInspector object attached to it as soon as it is created,
 * so this function will only return NULL if the argument is not a
 * valid #WebKitWebView.
 *
 * Returns: the #WebKitWebInspector instance associated with the
 * #WebKitWebView; %NULL is only returned if the argument is not a
 * valid #WebKitWebView.
 *
 * Since: 1.0.3
 */
WebKitWebInspector* webkit_web_view_get_inspector(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    WebKitWebViewPrivate* priv = webView->priv;
    return priv->webInspector;
}

// internal
static void webkit_web_view_set_window_features(WebKitWebView* webView, WebKitWebWindowFeatures* webWindowFeatures)
{
    WebKitWebViewPrivate* priv = webView->priv;

    if(webkit_web_window_features_equal(priv->webWindowFeatures, webWindowFeatures))
      return;

    g_object_unref(priv->webWindowFeatures);
    g_object_ref(webWindowFeatures);
    priv->webWindowFeatures = webWindowFeatures;
}

/**
 * webkit_web_view_get_window_features
 * @web_view: a #WebKitWebView
 *
 * Returns the instance of #WebKitWebWindowFeatures held by the given
 * #WebKitWebView.
 *
 * Return value: the #WebKitWebWindowFeatures
 *
 * Since: 1.0.3
 */
WebKitWebWindowFeatures* webkit_web_view_get_window_features(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    WebKitWebViewPrivate* priv = webView->priv;
    return priv->webWindowFeatures;
}

/**
 * webkit_web_view_get_title:
 * @web_view: a #WebKitWebView
 *
 * Returns the @web_view's document title
 *
 * Since: 1.1.4
 *
 * Return value: the title of @web_view
 */
G_CONST_RETURN gchar* webkit_web_view_get_title(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    WebKitWebViewPrivate* priv = webView->priv;
    return priv->mainFrame->priv->title;
}

/**
 * webkit_web_view_get_uri:
 * @web_view: a #WebKitWebView
 *
 * Returns the current URI of the contents displayed by the @web_view
 *
 * Since: 1.1.4
 *
 * Return value: the URI of @web_view
 */
G_CONST_RETURN gchar* webkit_web_view_get_uri(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    WebKitWebViewPrivate* priv = webView->priv;
    return priv->mainFrame->priv->uri;
}

/**
 * webkit_web_view_set_maintains_back_forward_list:
 * @web_view: a #WebKitWebView
 * @flag: to tell the view to maintain a back or forward list
 *
 * Set the view to maintain a back or forward list of history items.
 */
void webkit_web_view_set_maintains_back_forward_list(WebKitWebView* webView, gboolean flag)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    core(webView)->backForwardList()->setEnabled(flag);
}

/**
 * webkit_web_view_get_back_forward_list:
 * @web_view: a #WebKitWebView
 *
 * Returns a #WebKitWebBackForwardList
 *
 * Return value: the #WebKitWebBackForwardList
 */
WebKitWebBackForwardList* webkit_web_view_get_back_forward_list(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    WebKitWebViewPrivate* priv = webView->priv;

    if (!core(webView) || !core(webView)->backForwardList()->enabled())
        return NULL;

    return priv->backForwardList;
}

/**
 * webkit_web_view_go_to_back_forward_item:
 * @web_view: a #WebKitWebView
 * @item: a #WebKitWebHistoryItem*
 *
 * Go to the specified #WebKitWebHistoryItem
 *
 * Return value: %TRUE if loading of item is successful, %FALSE if not
 */
gboolean webkit_web_view_go_to_back_forward_item(WebKitWebView* webView, WebKitWebHistoryItem* item)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);
    g_return_val_if_fail(WEBKIT_IS_WEB_HISTORY_ITEM(item), FALSE);

    WebKitWebBackForwardList* backForwardList = webkit_web_view_get_back_forward_list(webView);
    if (!webkit_web_back_forward_list_contains_item(backForwardList, item))
        return FALSE;

    core(webView)->goToItem(core(item), FrameLoadTypeIndexedBackForward);
    return TRUE;
}

/**
 * webkit_web_view_go_back:
 * @web_view: a #WebKitWebView
 *
 * Loads the previous history item.
 */
void webkit_web_view_go_back(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    core(webView)->goBack();
}

/**
 * webkit_web_view_go_back_or_forward:
 * @web_view: a #WebKitWebView
 * @steps: the number of steps
 *
 * Loads the history item that is the number of @steps away from the current
 * item. Negative values represent steps backward while positive values
 * represent steps forward.
 */
void webkit_web_view_go_back_or_forward(WebKitWebView* webView, gint steps)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    core(webView)->goBackOrForward(steps);
}

/**
 * webkit_web_view_go_forward:
 * @web_view: a #WebKitWebView
 *
 * Loads the next history item.
 */
void webkit_web_view_go_forward(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    core(webView)->goForward();
}

/**
 * webkit_web_view_can_go_back:
 * @web_view: a #WebKitWebView
 *
 * Determines whether #web_view has a previous history item.
 *
 * Return value: %TRUE if able to move back, %FALSE otherwise
 */
gboolean webkit_web_view_can_go_back(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    if (!core(webView) || !core(webView)->backForwardList()->backItem())
        return FALSE;

    return TRUE;
}

/**
 * webkit_web_view_can_go_back_or_forward:
 * @web_view: a #WebKitWebView
 * @steps: the number of steps
 *
 * Determines whether #web_view has a history item of @steps. Negative values
 * represent steps backward while positive values represent steps forward.
 *
 * Return value: %TRUE if able to move back or forward the given number of
 * steps, %FALSE otherwise
 */
gboolean webkit_web_view_can_go_back_or_forward(WebKitWebView* webView, gint steps)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return core(webView)->canGoBackOrForward(steps);
}

/**
 * webkit_web_view_can_go_forward:
 * @web_view: a #WebKitWebView
 *
 * Determines whether #web_view has a next history item.
 *
 * Return value: %TRUE if able to move forward, %FALSE otherwise
 */
gboolean webkit_web_view_can_go_forward(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    Page* page = core(webView);

    if (!page)
        return FALSE;

    if (!page->backForwardList()->forwardItem())
        return FALSE;

    return TRUE;
}

/**
 * webkit_web_view_open:
 * @web_view: a #WebKitWebView
 * @uri: an URI
 *
 * Requests loading of the specified URI string.
 *
 * Deprecated: 1.1.1: Use webkit_web_view_load_uri() instead.
  */
void webkit_web_view_open(WebKitWebView* webView, const gchar* uri)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(uri);

    // We used to support local paths, unlike the newer
    // function webkit_web_view_load_uri
    if (g_path_is_absolute(uri)) {
        gchar* fileUri = g_filename_to_uri(uri, NULL, NULL);
        webkit_web_view_load_uri(webView, fileUri);
        g_free(fileUri);
    }
    else
        webkit_web_view_load_uri(webView, uri);
}

void webkit_web_view_reload(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    core(webView)->mainFrame()->loader()->reload();
}

/**
 * webkit_web_view_reload_bypass_cache:
 * @web_view: a #WebKitWebView
 *
 * Reloads the @web_view without using any cached data.
 *
 * Since: 1.0.3
 */
void webkit_web_view_reload_bypass_cache(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    core(webView)->mainFrame()->loader()->reload(true);
}

/**
 * webkit_web_view_load_uri:
 * @web_view: a #WebKitWebView
 * @uri: an URI string
 *
 * Requests loading of the specified URI string.
 *
 * Since: 1.1.1
 */
void webkit_web_view_load_uri(WebKitWebView* webView, const gchar* uri)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(uri);

    WebKitWebFrame* frame = webView->priv->mainFrame;
    webkit_web_frame_load_uri(frame, uri);
}

/**
+  * webkit_web_view_load_string:
+  * @web_view: a #WebKitWebView
+  * @content: an URI string
+  * @mime_type: the MIME type, or %NULL
+  * @encoding: the encoding, or %NULL
+  * @base_uri: the base URI for relative locations
+  *
+  * Requests loading of the given @content with the specified @mime_type,
+  * @encoding and @base_uri.
+  *
+  * If @mime_type is %NULL, "text/html" is assumed.
+  *
+  * If @encoding is %NULL, "UTF-8" is assumed.
+  */
void webkit_web_view_load_string(WebKitWebView* webView, const gchar* content, const gchar* mimeType, const gchar* encoding, const gchar* baseUri)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(content);

    WebKitWebFrame* frame = webView->priv->mainFrame;
    webkit_web_frame_load_string(frame, content, mimeType, encoding, baseUri);
}
/**
 * webkit_web_view_load_html_string:
 * @web_view: a #WebKitWebView
 * @content: an URI string
 * @base_uri: the base URI for relative locations
 *
 * Requests loading of the given @content with the specified @base_uri.
 *
 * Deprecated: 1.1.1: Use webkit_web_view_load_string() instead.
 */
void webkit_web_view_load_html_string(WebKitWebView* webView, const gchar* content, const gchar* baseUri)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(content);

    webkit_web_view_load_string(webView, content, NULL, NULL, baseUri);
}

/**
 * webkit_web_view_load_request:
 * @web_view: a #WebKitWebView
 * @request: a #WebKitNetworkRequest
 *
 * Requests loading of the specified asynchronous client request.
 *
 * Creates a provisional data source that will transition to a committed data
 * source once any data has been received. Use webkit_web_view_stop_loading() to
 * stop the load.
 *
 * Since: 1.1.1
 */
void webkit_web_view_load_request(WebKitWebView* webView, WebKitNetworkRequest* request)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(WEBKIT_IS_NETWORK_REQUEST(request));

    WebKitWebFrame* frame = webView->priv->mainFrame;
    webkit_web_frame_load_request(frame, request);
}

/**
 * webkit_web_view_stop_loading:
 * @webView: a #WebKitWebView
 * 
 * Stops any ongoing load in the @webView.
 **/
void webkit_web_view_stop_loading(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    Frame* frame = core(webView)->mainFrame();

    if (FrameLoader* loader = frame->loader())
        loader->stopForUserCancel();
}

/**
 * webkit_web_view_search_text:
 * @web_view: a #WebKitWebView
 * @text: a string to look for
 * @forward: whether to find forward or not
 * @case_sensitive: whether to respect the case of text
 * @wrap: whether to continue looking at the beginning after reaching the end
 *
 * Looks for a specified string inside #web_view.
 *
 * Return value: %TRUE on success or %FALSE on failure
 */
gboolean webkit_web_view_search_text(WebKitWebView* webView, const gchar* string, gboolean caseSensitive, gboolean forward, gboolean shouldWrap)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);
    g_return_val_if_fail(string, FALSE);

    TextCaseSensitivity caseSensitivity = caseSensitive ? TextCaseSensitive : TextCaseInsensitive;
    FindDirection direction = forward ? FindDirectionForward : FindDirectionBackward;

    return core(webView)->findString(String::fromUTF8(string), caseSensitivity, direction, shouldWrap);
}

/**
 * webkit_web_view_mark_text_matches:
 * @web_view: a #WebKitWebView
 * @string: a string to look for
 * @case_sensitive: whether to respect the case of text
 * @limit: the maximum number of strings to look for or %0 for all
 *
 * Attempts to highlight all occurances of #string inside #web_view.
 *
 * Return value: the number of strings highlighted
 */
guint webkit_web_view_mark_text_matches(WebKitWebView* webView, const gchar* string, gboolean caseSensitive, guint limit)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    g_return_val_if_fail(string, 0);

    TextCaseSensitivity caseSensitivity = caseSensitive ? TextCaseSensitive : TextCaseInsensitive;

    return core(webView)->markAllMatchesForText(String::fromUTF8(string), caseSensitivity, false, limit);
}

/**
 * webkit_web_view_set_highlight_text_matches:
 * @web_view: a #WebKitWebView
 * @highlight: whether to highlight text matches
 *
 * Highlights text matches previously marked by webkit_web_view_mark_text_matches.
 */
void webkit_web_view_set_highlight_text_matches(WebKitWebView* webView, gboolean shouldHighlight)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    Frame *frame = core(webView)->mainFrame();
    do {
        frame->setMarkedTextMatchesAreHighlighted(shouldHighlight);
        frame = frame->tree()->traverseNextWithWrap(false);
    } while (frame);
}

/**
 * webkit_web_view_unmark_text_matches:
 * @web_view: a #WebKitWebView
 *
 * Removes highlighting previously set by webkit_web_view_mark_text_matches.
 */
void webkit_web_view_unmark_text_matches(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    return core(webView)->unmarkAllTextMatches();
}

WebKitWebFrame* webkit_web_view_get_main_frame(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    WebKitWebViewPrivate* priv = webView->priv;
    return priv->mainFrame;
}

/**
 * webkit_web_view_get_focused_frame:
 * @web_view: a #WebKitWebView
 *
 * Returns the frame that has focus or an active text selection.
 *
 * Return value: The focused #WebKitWebFrame or %NULL if no frame is focused
 */
WebKitWebFrame* webkit_web_view_get_focused_frame(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    Frame* focusedFrame = core(webView)->focusController()->focusedFrame();
    return kit(focusedFrame);
}

void webkit_web_view_execute_script(WebKitWebView* webView, const gchar* script)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(script);

    core(webView)->mainFrame()->script()->executeScript(String::fromUTF8(script), true);
}

/**
 * webkit_web_view_cut_clipboard:
 * @web_view: a #WebKitWebView
 *
 * Determines whether or not it is currently possible to cut to the clipboard.
 *
 * Return value: %TRUE if a selection can be cut, %FALSE if not
 */
gboolean webkit_web_view_can_cut_clipboard(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    return frame->editor()->canCut() || frame->editor()->canDHTMLCut();
}

/**
 * webkit_web_view_copy_clipboard:
 * @web_view: a #WebKitWebView
 *
 * Determines whether or not it is currently possible to copy to the clipboard.
 *
 * Return value: %TRUE if a selection can be copied, %FALSE if not
 */
gboolean webkit_web_view_can_copy_clipboard(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    return frame->editor()->canCopy() || frame->editor()->canDHTMLCopy();
}

/**
 * webkit_web_view_paste_clipboard:
 * @web_view: a #WebKitWebView
 *
 * Determines whether or not it is currently possible to paste from the clipboard.
 *
 * Return value: %TRUE if a selection can be pasted, %FALSE if not
 */
gboolean webkit_web_view_can_paste_clipboard(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    return frame->editor()->canPaste() || frame->editor()->canDHTMLPaste();
}

/**
 * webkit_web_view_cut_clipboard:
 * @web_view: a #WebKitWebView
 *
 * Cuts the current selection inside the @web_view to the clipboard.
 */
void webkit_web_view_cut_clipboard(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (webkit_web_view_can_cut_clipboard(webView))
        g_signal_emit(webView, webkit_web_view_signals[CUT_CLIPBOARD], 0);
}

/**
 * webkit_web_view_copy_clipboard:
 * @web_view: a #WebKitWebView
 *
 * Copies the current selection inside the @web_view to the clipboard.
 */
void webkit_web_view_copy_clipboard(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (webkit_web_view_can_copy_clipboard(webView))
        g_signal_emit(webView, webkit_web_view_signals[COPY_CLIPBOARD], 0);
}

/**
 * webkit_web_view_paste_clipboard:
 * @web_view: a #WebKitWebView
 *
 * Pastes the current contents of the clipboard to the @web_view.
 */
void webkit_web_view_paste_clipboard(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (webkit_web_view_can_paste_clipboard(webView))
        g_signal_emit(webView, webkit_web_view_signals[PASTE_CLIPBOARD], 0);
}

/**
 * webkit_web_view_delete_selection:
 * @web_view: a #WebKitWebView
 *
 * Deletes the current selection inside the @web_view.
 */
void webkit_web_view_delete_selection(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    frame->editor()->performDelete();
}

/**
 * webkit_web_view_has_selection:
 * @web_view: a #WebKitWebView
 *
 * Determines whether text was selected.
 *
 * Return value: %TRUE if there is selected text, %FALSE if not
 */
gboolean webkit_web_view_has_selection(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return !core(webView)->selection().isNone();
}

/**
 * webkit_web_view_get_selected_text:
 * @web_view: a #WebKitWebView
 *
 * Retrieves the selected text if any.
 *
 * Return value: a newly allocated string with the selection or %NULL
 */
gchar* webkit_web_view_get_selected_text(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    return g_strdup(frame->selectedText().utf8().data());
}

/**
 * webkit_web_view_select_all:
 * @web_view: a #WebKitWebView
 *
 * Attempts to select everything inside the @web_view.
 */
void webkit_web_view_select_all(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    g_signal_emit(webView, webkit_web_view_signals[SELECT_ALL], 0);
}

/**
 * webkit_web_view_get_editable:
 * @web_view: a #WebKitWebView
 *
 * Returns whether the user is allowed to edit the document.
 *
 * Returns %TRUE if @web_view allows the user to edit the HTML document, %FALSE if
 * it doesn't. You can change @web_view's document programmatically regardless of
 * this setting.
 *
 * Return value: a #gboolean indicating the editable state
 */
gboolean webkit_web_view_get_editable(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    WebKitWebViewPrivate* priv = webView->priv;

    return priv->editable;
}

/**
 * webkit_web_view_set_editable:
 * @web_view: a #WebKitWebView
 * @flag: a #gboolean indicating the editable state
 *
 * Sets whether @web_view allows the user to edit its HTML document.
 *
 * If @flag is %TRUE, @web_view allows the user to edit the document. If @flag is
 * %FALSE, an element in @web_view's document can only be edited if the
 * CONTENTEDITABLE attribute has been set on the element or one of its parent
 * elements. You can change @web_view's document programmatically regardless of
 * this setting. By default a #WebKitWebView is not editable.

 * Normally, an HTML document is not editable unless the elements within the
 * document are editable. This function provides a low-level way to make the
 * contents of a #WebKitWebView editable without altering the document or DOM
 * structure.
 */
void webkit_web_view_set_editable(WebKitWebView* webView, gboolean flag)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WebKitWebViewPrivate* priv = webView->priv;

    Frame* frame = core(webView)->mainFrame();
    g_return_if_fail(frame);

    // TODO: What happens when the frame is replaced?
    flag = flag != FALSE;
    if (flag == priv->editable)
        return;

    priv->editable = flag;

    if (flag) {
        frame->applyEditingStyleToBodyElement();
        // TODO: If the WebKitWebView is made editable and the selection is empty, set it to something.
        //if (!webkit_web_view_get_selected_dom_range(webView))
        //    mainFrame->setSelectionFromNone();
    } else
        frame->removeEditingStyleFromBodyElement();
    g_object_notify(G_OBJECT(webView), "editable");
}

/**
 * webkit_web_view_get_copy_target_list:
 * @web_view: a #WebKitWebView
 *
 * This function returns the list of targets this #WebKitWebView can
 * provide for clipboard copying and as DND source. The targets in the list are
 * added with %info values from the #WebKitWebViewTargetInfo enum,
 * using gtk_target_list_add() and
 * gtk_target_list_add_text_targets().
 *
 * Return value: the #GtkTargetList
 **/
GtkTargetList* webkit_web_view_get_copy_target_list(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    WebKitWebViewPrivate* priv = webView->priv;
    return priv->copy_target_list;
}

/**
 * webkit_web_view_get_paste_target_list:
 * @web_view: a #WebKitWebView
 *
 * This function returns the list of targets this #WebKitWebView can
 * provide for clipboard pasting and as DND destination. The targets in the list are
 * added with %info values from the #WebKitWebViewTargetInfo enum,
 * using gtk_target_list_add() and
 * gtk_target_list_add_text_targets().
 *
 * Return value: the #GtkTargetList
 **/
GtkTargetList* webkit_web_view_get_paste_target_list(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    WebKitWebViewPrivate* priv = webView->priv;
    return priv->paste_target_list;
}

/**
 * webkit_web_view_can_show_mime_type:
 * @web_view: a #WebKitWebView
 * @mime_type: a MIME type
 *
 * This functions returns whether or not a MIME type can be displayed using this view.
 *
 * Return value: a #gboolean indicating if the MIME type can be displayed
 *
 * Since: 1.0.3
 **/

gboolean webkit_web_view_can_show_mime_type(WebKitWebView* webView, const gchar* mimeType)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    Frame* frame = core(webkit_web_view_get_main_frame(webView));
    if (FrameLoader* loader = frame->loader())
        return loader->canShowMIMEType(String::fromUTF8(mimeType));
    else
        return FALSE;
}

/**
 * webkit_web_view_get_transparent:
 * @web_view: a #WebKitWebView
 *
 * Returns whether the #WebKitWebView has a transparent background.
 *
 * Return value: %FALSE when the #WebKitWebView draws a solid background
 * (the default), otherwise %TRUE.
 */
gboolean webkit_web_view_get_transparent(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    WebKitWebViewPrivate* priv = webView->priv;
    return priv->transparent;
}

/**
 * webkit_web_view_set_transparent:
 * @web_view: a #WebKitWebView
 *
 * Sets whether the #WebKitWebView has a transparent background.
 *
 * Pass %FALSE to have the #WebKitWebView draw a solid background
 * (the default), otherwise %TRUE.
 */
void webkit_web_view_set_transparent(WebKitWebView* webView, gboolean flag)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WebKitWebViewPrivate* priv = webView->priv;
    priv->transparent = flag;

    // TODO: This needs to be made persistent or it could become a problem when
    // the main frame is replaced.
    Frame* frame = core(webView)->mainFrame();
    g_return_if_fail(frame);
    frame->view()->setTransparent(flag);
    g_object_notify(G_OBJECT(webView), "transparent");
}

/**
 * webkit_web_view_get_zoom_level:
 * @web_view: a #WebKitWebView
 *
 * Returns the zoom level of @web_view, i.e. the factor by which elements in
 * the page are scaled with respect to their original size.
 * If the "full-content-zoom" property is set to %FALSE (the default)
 * the zoom level changes the text size, or if %TRUE, scales all
 * elements in the page.
 *
 * Return value: the zoom level of @web_view
 *
 * Since: 1.0.1
 */
gfloat webkit_web_view_get_zoom_level(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 1.0f);

    Frame* frame = core(webView)->mainFrame();
    if (!frame)
        return 1.0f;

    return frame->zoomFactor();
}

static void webkit_web_view_apply_zoom_level(WebKitWebView* webView, gfloat zoomLevel)
{
    Frame* frame = core(webView)->mainFrame();
    if (!frame)
        return;

    WebKitWebViewPrivate* priv = webView->priv;
    frame->setZoomFactor(zoomLevel, !priv->zoomFullContent);
}

/**
 * webkit_web_view_set_zoom_level:
 * @web_view: a #WebKitWebView
 * @zoom_level: the new zoom level
 *
 * Sets the zoom level of @web_view, i.e. the factor by which elements in
 * the page are scaled with respect to their original size.
 * If the "full-content-zoom" property is set to %FALSE (the default)
 * the zoom level changes the text size, or if %TRUE, scales all
 * elements in the page.
 *
 * Since: 1.0.1
 */
void webkit_web_view_set_zoom_level(WebKitWebView* webView, gfloat zoomLevel)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    webkit_web_view_apply_zoom_level(webView, zoomLevel);
    g_object_notify(G_OBJECT(webView), "zoom-level");
}

/**
 * webkit_web_view_zoom_in:
 * @web_view: a #WebKitWebView
 *
 * Increases the zoom level of @web_view. The current zoom
 * level is incremented by the value of the "zoom-step"
 * property of the #WebKitWebSettings associated with @web_view.
 *
 * Since: 1.0.1
 */
void webkit_web_view_zoom_in(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WebKitWebViewPrivate* priv = webView->priv;
    gfloat zoomMultiplierRatio;
    g_object_get(priv->webSettings, "zoom-step", &zoomMultiplierRatio, NULL);

    webkit_web_view_set_zoom_level(webView, webkit_web_view_get_zoom_level(webView) + zoomMultiplierRatio);
}

/**
 * webkit_web_view_zoom_out:
 * @web_view: a #WebKitWebView
 *
 * Decreases the zoom level of @web_view. The current zoom
 * level is decremented by the value of the "zoom-step"
 * property of the #WebKitWebSettings associated with @web_view.
 *
 * Since: 1.0.1
 */
void webkit_web_view_zoom_out(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WebKitWebViewPrivate* priv = webView->priv;
    gfloat zoomMultiplierRatio;
    g_object_get(priv->webSettings, "zoom-step", &zoomMultiplierRatio, NULL);

    webkit_web_view_set_zoom_level(webView, webkit_web_view_get_zoom_level(webView) - zoomMultiplierRatio);
}

/**
 * webkit_web_view_get_full_content_zoom:
 * @web_view: a #WebKitWebView
 *
 * Returns whether the zoom level affects only text or all elements.
 *
 * Return value: %FALSE if only text should be scaled (the default),
 * %TRUE if the full content of the view should be scaled.
 *
 * Since: 1.0.1
 */
gboolean webkit_web_view_get_full_content_zoom(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    WebKitWebViewPrivate* priv = webView->priv;
    return priv->zoomFullContent;
}

/**
 * webkit_web_view_set_full_content_zoom:
 * @web_view: a #WebKitWebView
 * @full_content_zoom: %FALSE if only text should be scaled (the default),
 * %TRUE if the full content of the view should be scaled.
 *
 * Sets whether the zoom level affects only text or all elements.
 *
 * Since: 1.0.1
 */
void webkit_web_view_set_full_content_zoom(WebKitWebView* webView, gboolean zoomFullContent)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->zoomFullContent == zoomFullContent)
      return;

    priv->zoomFullContent = zoomFullContent;
    webkit_web_view_apply_zoom_level(webView, webkit_web_view_get_zoom_level(webView));

    g_object_notify(G_OBJECT(webView), "full-content-zoom");
}

/**
 * webkit_get_default_session:
 *
 * Retrieves the default #SoupSession used by all web views.
 * Note that the session features are added by WebKit on demand,
 * so if you insert your own #SoupCookieJar before any network
 * traffic occurs, WebKit will use it instead of the default.
 *
 * Return value: the default #SoupSession
 *
 * Since: 1.1.1
 */
SoupSession* webkit_get_default_session ()
{
    return ResourceHandle::defaultSession();
}

/**
 * webkit_web_view_get_load_status:
 * @web_view: a #WebKitWebView
 *
 * Determines the current status of the load.
 *
 * Since: 1.1.7
 */
WebKitLoadStatus webkit_web_view_get_load_status(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), WEBKIT_LOAD_FINISHED);

    WebKitWebViewPrivate* priv = webView->priv;
    return priv->loadStatus;
}

/**
 * webkit_web_view_get_progress:
 * @web_view: a #WebKitWebView
 *
 * Determines the current progress of the load.
 *
 * Since: 1.1.7
 */
gdouble webkit_web_view_get_progress(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 1.0);

    return core(webView)->progress()->estimatedProgress();
}

/**
 * webkit_web_view_get_encoding:
 * @web_view: a #WebKitWebView
 *
 * Returns the default encoding of the #WebKitWebView.
 *
 * Return value: the default encoding
 *
 * Since: 1.1.1
 */
const gchar* webkit_web_view_get_encoding(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    String encoding = core(webView)->mainFrame()->loader()->encoding();

    if (!encoding.isEmpty()) {
        WebKitWebViewPrivate* priv = webView->priv;
        g_free(priv->encoding);
        priv->encoding = g_strdup(encoding.utf8().data());
        return priv->encoding;
    } else
      return NULL;
}

/**
 * webkit_web_view_set_custom_encoding:
 * @web_view: a #WebKitWebView
 * @encoding: the new encoding, or %NULL to restore the default encoding
 *
 * Sets the current #WebKitWebView encoding, without modifying the default one,
 * and reloads the page.
 *
 * Since: 1.1.1
 */
void webkit_web_view_set_custom_encoding(WebKitWebView* webView, const char* encoding)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    core(webView)->mainFrame()->loader()->reloadWithOverrideEncoding(String::fromUTF8(encoding));
}

/**
 * webkit_web_view_get_custom_encoding:
 * @web_view: a #WebKitWebView
 *
 * Returns the current encoding of the #WebKitWebView, not the default-encoding
 * of WebKitWebSettings.
 *
 * Return value: a string containing the current custom encoding for @web_view, or %NULL if there's none set.
 *
 * Since: 1.1.1
 */
const char* webkit_web_view_get_custom_encoding(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);

    String overrideEncoding = core(webView)->mainFrame()->loader()->documentLoader()->overrideEncoding();

    if (!overrideEncoding.isEmpty()) {
        WebKitWebViewPrivate* priv = webView->priv;
        g_free (priv->customEncoding);
        priv->customEncoding = g_strdup(overrideEncoding.utf8().data());
        return priv->customEncoding;
    } else
      return NULL;
}

/**
 * webkit_web_view_move_cursor:
 * @web_view: a #WebKitWebView
 * @step: a #GtkMovementStep
 * @count: integer describing the direction of the movement. 1 for forward, -1 for backwards.
 *
 * Move the cursor in @view as described by @step and @count.
 *
 * Since: 1.1.4
 */
void webkit_web_view_move_cursor(WebKitWebView* webView, GtkMovementStep step, gint count)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(step == GTK_MOVEMENT_VISUAL_POSITIONS ||
                     step == GTK_MOVEMENT_DISPLAY_LINES ||
                     step == GTK_MOVEMENT_PAGES ||
                     step == GTK_MOVEMENT_BUFFER_ENDS);
    g_return_if_fail(count == 1 || count == -1);

    gboolean handled;
    g_signal_emit(webView, webkit_web_view_signals[MOVE_CURSOR], 0, step, count, &handled);
}

void webkit_web_view_set_group_name(WebKitWebView* webView, const gchar* groupName)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    WebKitWebViewPrivate* priv = webView->priv;

    if (!priv->corePage)
        return;

    priv->corePage->setGroupName(String::fromUTF8(groupName));
}

/**
 * webkit_web_view_can_undo:
 * @web_view: a #WebKitWebView
 *
 * Determines whether or not it is currently possible to undo the last
 * editing command in the view.
 *
 * Return value: %TRUE if a undo can be done, %FALSE if not
 *
 * Since: 1.1.14
 */
gboolean webkit_web_view_can_undo(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    return frame->editor()->canUndo();
}

/**
 * webkit_web_view_undo:
 * @web_view: a #WebKitWebView
 *
 * Undoes the last editing command in the view, if possible.
 *
 * Since: 1.1.14
 */
void webkit_web_view_undo(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (webkit_web_view_can_undo(webView))
        g_signal_emit(webView, webkit_web_view_signals[UNDO], 0);
}

/**
 * webkit_web_view_can_redo:
 * @web_view: a #WebKitWebView
 *
 * Determines whether or not it is currently possible to redo the last
 * editing command in the view.
 *
 * Return value: %TRUE if a redo can be done, %FALSE if not
 *
 * Since: 1.1.14
 */
gboolean webkit_web_view_can_redo(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    return frame->editor()->canRedo();
}

/**
 * webkit_web_view_redo:
 * @web_view: a #WebKitWebView
 *
 * Redoes the last editing command in the view, if possible.
 *
 * Since: 1.1.14
 */
void webkit_web_view_redo(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (webkit_web_view_can_redo(webView))
        g_signal_emit(webView, webkit_web_view_signals[REDO], 0);
}


/**
 * webkit_web_view_set_view_source_mode:
 * @web_view: a #WebKitWebView
 * @view_source_mode: the mode to turn on or off view source mode
 *
 * Set whether the view should be in view source mode. Setting this mode to
 * %TRUE before loading a URI will display the source of the web page in a
 * nice and readable format.
 *
 * Since: 1.1.14
 */
void webkit_web_view_set_view_source_mode (WebKitWebView* webView, gboolean mode)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (Frame* mainFrame = core(webView)->mainFrame())
        mainFrame->setInViewSourceMode(mode);
}

/**
 * webkit_web_view_get_view_source_mode:
 * @web_view: a #WebKitWebView
 *
 * Return value: %TRUE if @web_view is in view source mode, %FALSE otherwise.
 *
 * Since: 1.1.14
 */
gboolean webkit_web_view_get_view_source_mode (WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    if (Frame* mainFrame = core(webView)->mainFrame())
        return mainFrame->inViewSourceMode();

    return FALSE;
}

// Internal subresource management
void webkit_web_view_add_resource(WebKitWebView* webView, char* identifier, WebKitWebResource* webResource)
{
    WebKitWebViewPrivate* priv = webView->priv;

    if (!priv->mainResource) {
        priv->mainResource = webResource;
        priv->mainResourceIdentifier = g_strdup(identifier);
        return;
    }

    g_hash_table_insert(priv->subResources, identifier, webResource);
}

WebKitWebResource* webkit_web_view_get_resource(WebKitWebView* webView, char* identifier)
{
    WebKitWebViewPrivate* priv = webView->priv;
    gpointer webResource = NULL;

    gboolean resourceFound = g_hash_table_lookup_extended(priv->subResources, identifier, NULL, &webResource);

    // The only resource we do not store in this hash table is the
    // main!  If we did not find a request, it probably means the load
    // has been interrupted while while a resource was still being
    // loaded.
    if (!resourceFound && !g_str_equal(identifier, priv->mainResourceIdentifier))
        return NULL;

    if (!webResource)
        return webkit_web_view_get_main_resource(webView);

    return WEBKIT_WEB_RESOURCE(webResource);
}

WebKitWebResource* webkit_web_view_get_main_resource(WebKitWebView* webView)
{
    return webView->priv->mainResource;
}

void webkit_web_view_clear_resources(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = webView->priv;

    g_free(priv->mainResourceIdentifier);
    priv->mainResourceIdentifier = NULL;

    if (priv->mainResource) {
        g_object_unref(priv->mainResource);
        priv->mainResource = NULL;
    }

    if (priv->subResources)
        g_hash_table_remove_all(priv->subResources);
}

GList* webkit_web_view_get_subresources(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = webView->priv;
    GList* subResources = g_hash_table_get_values(priv->subResources);
    return g_list_remove(subResources, priv->mainResource);
}

/* From EventHandler.cpp */
static IntPoint documentPointForWindowPoint(Frame* frame, const IntPoint& windowPoint)
{
    FrameView* view = frame->view();
    // FIXME: Is it really OK to use the wrong coordinates here when view is 0?
    // Historically the code would just crash; this is clearly no worse than that.
    return view ? view->windowToContents(windowPoint) : windowPoint;
}

/**
 * webkit_web_view_get_hit_test_result:
 * @webView: a #WebKitWebView
 * @event: a #GdkEventButton
 * 
 * Does a 'hit test' in the coordinates specified by @event to figure
 * out context information about that position in the @webView.
 * 
 * Returns: a newly created #WebKitHitTestResult with the context of the
 * specified position.
 *
 * Since: 1.1.15
 **/
WebKitHitTestResult* webkit_web_view_get_hit_test_result(WebKitWebView* webView, GdkEventButton* event)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), NULL);
    g_return_val_if_fail(event, NULL);

    PlatformMouseEvent mouseEvent = PlatformMouseEvent(event);
    Frame* frame = core(webView)->focusController()->focusedOrMainFrame();
    HitTestRequest request(HitTestRequest::Active);
    IntPoint documentPoint = documentPointForWindowPoint(frame, mouseEvent.pos());
    MouseEventWithHitTestResults mev = frame->document()->prepareMouseEvent(request, documentPoint, mouseEvent);

    return kit(mev.hitTestResult());
}
