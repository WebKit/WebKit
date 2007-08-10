/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "webkitgtkpage.h"
#include "webkitgtk-marshal.h"
#include "webkitgtkprivate.h"

#include "NotImplemented.h"
#include "ChromeClientGtk.h"
#include "ContextMenuClientGtk.h"
#include "EditorClientGtk.h"
#include "EventHandler.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "GraphicsContext.h"
#include "InspectorClientGtk.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "SubstituteData.h"


using namespace WebKit;
using namespace WebCore;

extern "C" {

enum {
    /* normal signals */
    LOAD_STARTED,
    LOAD_PROGRESS_CHANGED,
    LOAD_FINISHED,
    TITLE_CHANGED,
    HOVERING_OVER_LINK,
    STATUS_BAR_TEXT_CHANGED,
    ICOND_LOADED,
    SELECTION_CHANGED,
    LAST_SIGNAL
};

static guint webkit_gtk_page_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(WebKitGtkPage, webkit_gtk_page, GTK_TYPE_CONTAINER)

static gboolean webkit_gtk_page_expose_event(GtkWidget* widget, GdkEventExpose* event)
{
    Frame* frame = core(getFrameFromPage(WEBKIT_GTK_PAGE(widget)));
    GdkRectangle clip;
    gdk_region_get_clipbox(event->region, &clip);
    cairo_t* cr = gdk_cairo_create(event->window);
    GraphicsContext ctx(cr);
    ctx.setGdkExposeEvent(event);
    if (frame->renderer()) {
        if (frame->view()->needsLayout())
            frame->view()->layout();
        IntRect rect(clip.x, clip.y, clip.width, clip.height);
        frame->view()->paint(&ctx, rect);
    }
    cairo_destroy(cr);

    return FALSE;
}

static gboolean webkit_gtk_page_key_event(GtkWidget* widget, GdkEventKey* event)
{
    Frame* frame = core(getFrameFromPage(WEBKIT_GTK_PAGE(widget)));
    frame->eventHandler()->keyEvent(PlatformKeyboardEvent(event));
    return FALSE;
}

static gboolean webkit_gtk_page_button_event(GtkWidget* widget, GdkEventButton* event)
{
    Frame* frame = core(getFrameFromPage(WEBKIT_GTK_PAGE(widget)));

    if (event->type == GDK_BUTTON_RELEASE)
        frame->eventHandler()->handleMouseReleaseEvent(PlatformMouseEvent(event));
    else
        frame->eventHandler()->handleMousePressEvent(PlatformMouseEvent(event));

    return FALSE;
}

static gboolean webkit_gtk_page_motion_event(GtkWidget* widget, GdkEventMotion* event)
{
    Frame* frame = core(getFrameFromPage(WEBKIT_GTK_PAGE(widget)));
    frame->eventHandler()->mouseMoved(PlatformMouseEvent(event));
    return FALSE;
}

static gboolean webkit_gtk_page_scroll_event(GtkWidget* widget, GdkEventScroll* event)
{
    Frame* frame = core(getFrameFromPage(WEBKIT_GTK_PAGE(widget)));

    PlatformWheelEvent wheelEvent(event);
    frame->eventHandler()->handleWheelEvent(wheelEvent);
    return FALSE;
}

static void webkit_gtk_page_size_allocate(GtkWidget* widget, GtkAllocation* allocation)
{
    GTK_WIDGET_CLASS(webkit_gtk_page_parent_class)->size_allocate(widget,allocation);

    Frame* frame = core(getFrameFromPage(WEBKIT_GTK_PAGE(widget)));
    frame->view()->resize(allocation->width, allocation->height);
    frame->forceLayout();
    frame->view()->adjustViewSize();
    frame->sendResizeEvent();
}

static void webkit_gtk_page_realize(GtkWidget* widget)
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
}

static void webkit_gtk_page_map(GtkWidget* widget)
{
    GTK_WIDGET_SET_FLAGS(widget, GTK_MAPPED);
    WebKitGtkPagePrivate* private_data = WEBKIT_GTK_PAGE_GET_PRIVATE(WEBKIT_GTK_PAGE(widget));

    HashSet<GtkWidget*>::const_iterator end = private_data->children.end();
    for (HashSet<GtkWidget*>::const_iterator current = private_data->children.begin(); current != end; ++current)
        if (GTK_WIDGET_VISIBLE(*current) && !GTK_WIDGET_MAPPED(*current))
            gtk_widget_map((*current));

    gdk_window_show(widget->window);
}

static void webkit_gtk_page_set_scroll_adjustments(WebKitGtkPage* page, GtkAdjustment* hadj, GtkAdjustment* vadj)
{
    FrameView* view = core(getFrameFromPage(page))->view();
    view->setGtkAdjustments(hadj, vadj);
}

static void webkit_gtk_page_container_add(GtkContainer* container, GtkWidget* widget)
{
    WebKitGtkPage* page = WEBKIT_GTK_PAGE(container);
    WebKitGtkPagePrivate* private_data = WEBKIT_GTK_PAGE_GET_PRIVATE(page);

    private_data->children.add(widget);
    if (GTK_WIDGET_REALIZED(container))
        gtk_widget_set_parent_window(widget, GTK_WIDGET(page)->window);
    gtk_widget_set_parent(widget, GTK_WIDGET(container));
}

static void webkit_gtk_page_container_remove(GtkContainer* container, GtkWidget* widget)
{
    WebKitGtkPagePrivate* private_data = WEBKIT_GTK_PAGE_GET_PRIVATE(WEBKIT_GTK_PAGE(container));

    if (private_data->children.contains(widget)) {
        gtk_widget_unparent(widget);
        private_data->children.remove(widget);
    }
}

static void webkit_gtk_page_container_forall(GtkContainer* container, gboolean, GtkCallback callback, gpointer callbackData)
{
    WebKitGtkPagePrivate* privateData = WEBKIT_GTK_PAGE_GET_PRIVATE(WEBKIT_GTK_PAGE(container));

    HashSet<GtkWidget*> children = privateData->children;
    HashSet<GtkWidget*>::const_iterator end = children.end();
    for (HashSet<GtkWidget*>::const_iterator current = children.begin(); current != end; ++current)
        (*callback)(*current, callbackData);
}

static WebKitGtkPage* webkit_gtk_page_real_create_page(WebKitGtkPage*)
{
    notImplemented();
    return 0;
}

static WEBKIT_GTK_NAVIGATION_REQUEST_RESPONSE webkit_gtk_page_real_navigation_requested(WebKitGtkPage*, WebKitGtkFrame* frame, WebKitGtkNetworkRequest*)
{
    notImplemented();
    return WEBKIT_GTK_ACCEPT_NAVIGATION_REQUEST;
}

static gchar* webkit_gtk_page_real_choose_file(WebKitGtkPage*, WebKitGtkFrame*, const gchar* old_name)
{
    notImplemented();
    return g_strdup(old_name);
}

static void webkit_gtk_page_real_java_script_alert(WebKitGtkPage*, WebKitGtkFrame*, const gchar*)
{
    notImplemented();
}

static gboolean webkit_gtk_page_real_java_script_confirm(WebKitGtkPage*, WebKitGtkFrame*, const gchar*)
{
    notImplemented();
    return FALSE;
}

/**
 * WebKitGtkPage::java_script_prompt
 *
 * @return: NULL to cancel the prompt
 */
static gchar* webkit_gtk_page_real_java_script_prompt(WebKitGtkPage*, WebKitGtkFrame*, const gchar*, const gchar* defaultValue)
{
    notImplemented();
    return g_strdup(defaultValue);
}

static void webkit_gtk_page_real_java_script_console_message(WebKitGtkPage*, const gchar*, unsigned int, const gchar*)
{
    notImplemented();
}

static void webkit_gtk_page_finalize(GObject* object)
{
    webkit_gtk_page_stop_loading(WEBKIT_GTK_PAGE(object));

    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(WEBKIT_GTK_PAGE(object));
    //delete pageData->page; - we don't have a DragClient yet, so it is crashing here
    delete pageData->settings;
    g_object_unref(pageData->mainFrame);
    delete pageData->userAgent;

    G_OBJECT_CLASS(webkit_gtk_page_parent_class)->finalize(object);
}

static void webkit_gtk_page_class_init(WebKitGtkPageClass* pageClass)
{
    g_type_class_add_private(pageClass, sizeof(WebKitGtkPagePrivate));


    /*
     * signals
     */
    /**
     * WebKitGtkPage::load-started
     * @page: the object on which the signal is emitted
     * @frame: the frame going to do the load
     *
     * When a WebKitGtkFrame begins to load this signal is emitted.
     */
    webkit_gtk_page_signals[LOAD_STARTED] = g_signal_new("load_started",
            G_TYPE_FROM_CLASS(pageClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            WEBKIT_GTK_TYPE_FRAME);

    /**
     * WebKitGtkPage::load-progress-changed
     * @page: The WebKitGtkPage
     * @progress: Global progress
     */
    webkit_gtk_page_signals[LOAD_PROGRESS_CHANGED] = g_signal_new("load_progress_changed",
            G_TYPE_FROM_CLASS(pageClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__INT,
            G_TYPE_NONE, 1,
            G_TYPE_INT);
    
    webkit_gtk_page_signals[LOAD_FINISHED] = g_signal_new("load_finished",
            G_TYPE_FROM_CLASS(pageClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            WEBKIT_GTK_TYPE_FRAME);

    webkit_gtk_page_signals[TITLE_CHANGED] = g_signal_new("title_changed",
            G_TYPE_FROM_CLASS(pageClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            webkit_gtk_marshal_VOID__STRING_STRING,
            G_TYPE_NONE, 2,
            G_TYPE_STRING, G_TYPE_STRING);

    webkit_gtk_page_signals[HOVERING_OVER_LINK] = g_signal_new("hovering_over_link",
            G_TYPE_FROM_CLASS(pageClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            webkit_gtk_marshal_VOID__STRING_STRING,
            G_TYPE_NONE, 2,
            G_TYPE_STRING,
            G_TYPE_STRING);

    webkit_gtk_page_signals[STATUS_BAR_TEXT_CHANGED] = g_signal_new("status_bar_text_changed",
            G_TYPE_FROM_CLASS(pageClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__STRING,
            G_TYPE_NONE, 1,
            G_TYPE_STRING);

    webkit_gtk_page_signals[ICOND_LOADED] = g_signal_new("icon_loaded",
            G_TYPE_FROM_CLASS(pageClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    webkit_gtk_page_signals[SELECTION_CHANGED] = g_signal_new("selection_changed",
            G_TYPE_FROM_CLASS(pageClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);


    /*
     * implementations of virtual methods
     */
    pageClass->create_page = webkit_gtk_page_real_create_page;
    pageClass->navigation_requested = webkit_gtk_page_real_navigation_requested;
    pageClass->choose_file = webkit_gtk_page_real_choose_file;
    pageClass->java_script_alert = webkit_gtk_page_real_java_script_alert;
    pageClass->java_script_confirm = webkit_gtk_page_real_java_script_confirm;
    pageClass->java_script_prompt = webkit_gtk_page_real_java_script_prompt;
    pageClass->java_script_console_message = webkit_gtk_page_real_java_script_console_message;

    G_OBJECT_CLASS(pageClass)->finalize = webkit_gtk_page_finalize;

    GtkWidgetClass* widgetClass = GTK_WIDGET_CLASS(pageClass);
    widgetClass->realize = webkit_gtk_page_realize;
    widgetClass->map = webkit_gtk_page_map;
    widgetClass->expose_event = webkit_gtk_page_expose_event;
    widgetClass->key_press_event = webkit_gtk_page_key_event;
    widgetClass->key_release_event = webkit_gtk_page_key_event;
    widgetClass->button_press_event = webkit_gtk_page_button_event;
    widgetClass->button_release_event = webkit_gtk_page_button_event;
    widgetClass->motion_notify_event = webkit_gtk_page_motion_event;
    widgetClass->scroll_event = webkit_gtk_page_scroll_event;
    widgetClass->size_allocate = webkit_gtk_page_size_allocate;

    GtkContainerClass* containerClass = GTK_CONTAINER_CLASS(pageClass);
    containerClass->add = webkit_gtk_page_container_add;
    containerClass->remove = webkit_gtk_page_container_remove;
    containerClass->forall = webkit_gtk_page_container_forall;

    /*
     * make us scrollable (e.g. addable to a GtkScrolledWindow)
     */
    pageClass->set_scroll_adjustments = webkit_gtk_page_set_scroll_adjustments;
    GTK_WIDGET_CLASS(pageClass)->set_scroll_adjustments_signal = g_signal_new("set_scroll_adjustments",
            G_TYPE_FROM_CLASS(pageClass),
            (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
            G_STRUCT_OFFSET(WebKitGtkPageClass, set_scroll_adjustments),
            NULL, NULL,
            webkit_gtk_marshal_VOID__OBJECT_OBJECT,
            G_TYPE_NONE, 2,
            GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);
}

static void webkit_gtk_page_init(WebKitGtkPage* page)
{
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(WEBKIT_GTK_PAGE(page));
    pageData->page = new Page(new WebKit::ChromeClient(page), new WebKit::ContextMenuClient, new WebKit::EditorClient(page), 0, new WebKit::InspectorClient);

    Settings* settings = pageData->page->settings();
    settings->setLoadsImagesAutomatically(true);
    settings->setMinimumFontSize(5);
    settings->setMinimumLogicalFontSize(5);
    settings->setShouldPrintBackgrounds(true);
    settings->setJavaScriptEnabled(true);
    settings->setDefaultFixedFontSize(14);
    settings->setDefaultFontSize(14);
    settings->setSerifFontFamily("Times New Roman");
    settings->setSansSerifFontFamily("Arial");
    settings->setFixedFontFamily("Courier");
    settings->setStandardFontFamily("Arial");

    GTK_WIDGET_SET_FLAGS(page, GTK_CAN_FOCUS);
    pageData->mainFrame = WEBKIT_GTK_FRAME(webkit_gtk_frame_new(page));
}

GtkWidget* webkit_gtk_page_new(void)
{
    WebKitGtkPage* page = WEBKIT_GTK_PAGE(g_object_new(WEBKIT_GTK_TYPE_PAGE, NULL));

    return GTK_WIDGET(page);
}

void webkit_gtk_page_set_settings(WebKitGtkPage* page, WebKitGtkSettings* settings)
{
    notImplemented();
}

WebKitGtkSettings* webkit_gtk_page_get_settings(WebKitGtkPage* page)
{
    notImplemented();
    return 0;
}

void webkit_gtk_page_go_backward(WebKitGtkPage* page)
{
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(page);
    WebKitGtkFramePrivate* frameData = WEBKIT_GTK_FRAME_GET_PRIVATE(pageData->mainFrame);
    frameData->frame->loader()->goBackOrForward(-1);
}

void webkit_gtk_page_go_forward(WebKitGtkPage* page)
{
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(page);
    WebKitGtkFramePrivate* frameData = WEBKIT_GTK_FRAME_GET_PRIVATE(pageData->mainFrame);
    frameData->frame->loader()->goBackOrForward(1);
}

gboolean webkit_gtk_page_can_go_backward(WebKitGtkPage* page)
{
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(page);
    WebKitGtkFramePrivate* frameData = WEBKIT_GTK_FRAME_GET_PRIVATE(pageData->mainFrame);
    return frameData->frame->loader()->canGoBackOrForward(-1);
}

gboolean webkit_gtk_page_can_go_forward(WebKitGtkPage* page)
{
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(page);
    WebKitGtkFramePrivate* frameData = WEBKIT_GTK_FRAME_GET_PRIVATE(pageData->mainFrame);
    return frameData->frame->loader()->canGoBackOrForward(1);
}

void webkit_gtk_page_open(WebKitGtkPage* page, const gchar* url)
{
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(page);
    WebKitGtkFramePrivate* frameData = WEBKIT_GTK_FRAME_GET_PRIVATE(pageData->mainFrame);

    DeprecatedString string = DeprecatedString::fromUtf8(url);
    frameData->frame->loader()->load(ResourceRequest(KURL(string)));
}

void webkit_gtk_page_reload(WebKitGtkPage* page)
{
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(page);
    WebKitGtkFramePrivate* frameData = WEBKIT_GTK_FRAME_GET_PRIVATE(pageData->mainFrame);
    frameData->frame->loader()->reload();
}

void webkit_gtk_page_load_string(WebKitGtkPage* page, const gchar* content, const gchar* contentMimeType, const gchar* contentEncoding, const gchar* baseUrl)
{
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(page);
    WebKitGtkFramePrivate* frameData = WEBKIT_GTK_FRAME_GET_PRIVATE(pageData->mainFrame);
    
    KURL url(DeprecatedString::fromUtf8(baseUrl));
    RefPtr<SharedBuffer> sharedBuffer = new SharedBuffer(strdup(content), strlen(content));    
    SubstituteData substituteData(sharedBuffer.release(), String(contentMimeType), String(contentEncoding), KURL("about:blank"), url);

    frameData->frame->loader()->load(ResourceRequest(url), substituteData);
}

void webkit_gtk_page_load_html_string(WebKitGtkPage* page, const gchar* content, const gchar* baseUrl)
{
    webkit_gtk_page_load_string(page, content, "text/html", "UTF-8", baseUrl);
}

void webkit_gtk_page_stop_loading(WebKitGtkPage* page)
{
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(page);
    WebKitGtkFramePrivate* frameData = WEBKIT_GTK_FRAME_GET_PRIVATE(pageData->mainFrame);
    
    if (FrameLoader* loader = frameData->frame->loader())
        loader->stopAllLoaders();
        
}

WebKitGtkFrame* webkit_gtk_page_get_main_frame(WebKitGtkPage* page)
{
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(page);
    return pageData->mainFrame;
}
}
