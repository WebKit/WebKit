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
#include "ChromeClientGdk.h"
#include "ContextMenuClientGdk.h"
#include "EditorClientGdk.h"
#include "EventHandler.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "GraphicsContext.h"
#include "InspectorClientGdk.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "SubstituteData.h"


using namespace WebCore;
using namespace WebKitGtk;

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

G_DEFINE_TYPE(WebKitGtkPage, webkit_gtk_page, GTK_TYPE_LAYOUT)

struct FrameGdkExposeData {
    GtkContainer* container;
    GdkEventExpose* expose;
};

static void frame_gdk_expose_child(GtkWidget* widget, gpointer _data)
{
    FrameGdkExposeData* data = (FrameGdkExposeData*)_data;
    gtk_container_propagate_expose(data->container, widget, data->expose);
} 

static gboolean webkit_gtk_page_rendering_area_handle_gdk_event(GtkWidget* widget, GdkEvent* event)
{
    Frame* frame = core(getFrameFromPage(WEBKIT_GTK_PAGE(widget)));

    switch (event->type) {
    case GDK_EXPOSE: {
        GdkRectangle clip;
        gdk_region_get_clipbox(event->expose.region, &clip);
        gdk_window_begin_paint_region(event->any.window, event->expose.region);
        cairo_t* cr = gdk_cairo_create(event->any.window);
        GraphicsContext ctx(cr);
        ctx.setGdkDrawable(event->any.window);
        if (frame->renderer()) {
            if (frame->view()->needsLayout())
                frame->view()->layout();
            IntRect rect(clip.x, clip.y, clip.width, clip.height);
            frame->paint(&ctx, rect);
        }
        cairo_destroy(cr);

        /*
         * Make sure children of the view get redrawn
         */
        FrameGdkExposeData data = { GTK_CONTAINER(frame->view()->gtkWidget()), &event->expose };
        gtk_container_forall(GTK_CONTAINER(frame->view()->gtkWidget()), frame_gdk_expose_child, &data);
        gdk_window_end_paint(event->any.window);
        break;
    }

    case GDK_CONFIGURE: {
        frame->view()->updateGeometry(event->configure.width, event->configure.height);
        frame->forceLayout();
        break;
    }

    case GDK_SCROLL: {
        PlatformWheelEvent wheelEvent(&event->scroll);
        frame->view()->wheelEvent(wheelEvent);
        if (wheelEvent.isAccepted())
            return TRUE;

        HitTestRequest hitTestRequest(true, true);
        HitTestResult hitTestResult(wheelEvent.pos());
        frame->renderer()->layer()->hitTest(hitTestRequest, hitTestResult);
        Node* node = hitTestResult.innerNode();
        if (!node)
            return TRUE;
        /*
         * FIXME: Does this belong here?
         * Default to scrolling the page
         * not sure why its null
         * broke anyway when its not null
         * doScroll(renderer(), wheelEvent.deltaX(), wheelEvent.deltaY());
         */
        break;
    }
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED: {
        //bool updateDragAndDrop(const PlatformMouseEvent&, Clipboard*);
        //void cancelDragAndDrop(const PlatformMouseEvent&, Clipboard*);
        //bool performDragAndDrop(const PlatformMouseEvent&, Clipboard*);
        break;
    }
    case GDK_MOTION_NOTIFY:
        frame->eventHandler()->mouseMoved(PlatformMouseEvent(&event->motion));
        break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
        frame->eventHandler()->handleMousePressEvent(PlatformMouseEvent(&event->button));
        break;
    case GDK_BUTTON_RELEASE:
        frame->eventHandler()->handleMouseReleaseEvent(PlatformMouseEvent(&event->button));
        break;
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
        frame->eventHandler()->keyEvent(PlatformKeyboardEvent(&event->key));
        break;
    default:
        break;
    }

    return FALSE;
}

static void webkit_gtk_page_rendering_area_resize_callback(GtkWidget*, GtkAllocation* allocation, WebKitGtkPage* page)
{
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(page);
    WebKitGtkFramePrivate* frameData = WEBKIT_GTK_FRAME_GET_PRIVATE(pageData->mainFrame);

    frameData->frame->view()->updateGeometry(allocation->width, allocation->height);
    frameData->frame->forceLayout();
    frameData->frame->view()->adjustViewSize();
    frameData->frame->sendResizeEvent();
}

static void webkit_gtk_page_register_rendering_area_events(GtkWidget* win, WebKitGtkPage* page)
{
    gdk_window_set_events(GTK_IS_LAYOUT(win) ? GTK_LAYOUT(win)->bin_window : win->window,
                          (GdkEventMask)(GDK_EXPOSURE_MASK
                            | GDK_BUTTON_PRESS_MASK
                            | GDK_BUTTON_RELEASE_MASK
                            | GDK_POINTER_MOTION_MASK
                            | GDK_KEY_PRESS_MASK
                            | GDK_KEY_RELEASE_MASK
                            | GDK_BUTTON_MOTION_MASK
                            | GDK_BUTTON1_MOTION_MASK
                            | GDK_BUTTON2_MOTION_MASK
                            | GDK_BUTTON3_MOTION_MASK));
}

static WebKitGtkFrame* webkit_gtk_page_real_create_frame(WebKitGtkPage*, WebKitGtkFrame* parent, WebKitGtkFrameData*)
{
    notImplemented();
    return 0;
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
    pageClass->create_frame = webkit_gtk_page_real_create_frame;
    pageClass->create_page = webkit_gtk_page_real_create_page;
    pageClass->navigation_requested = webkit_gtk_page_real_navigation_requested;
    pageClass->choose_file = webkit_gtk_page_real_choose_file;
    pageClass->java_script_alert = webkit_gtk_page_real_java_script_alert;
    pageClass->java_script_confirm = webkit_gtk_page_real_java_script_confirm;
    pageClass->java_script_prompt = webkit_gtk_page_real_java_script_prompt;
    pageClass->java_script_console_message = webkit_gtk_page_real_java_script_console_message;

    G_OBJECT_CLASS(pageClass)->finalize = webkit_gtk_page_finalize;
    GTK_WIDGET_CLASS(pageClass)->event = webkit_gtk_page_rendering_area_handle_gdk_event;
}

static void webkit_gtk_page_init(WebKitGtkPage* page)
{
    WebKitGtkPagePrivate* pageData = WEBKIT_GTK_PAGE_GET_PRIVATE(WEBKIT_GTK_PAGE(page));
    pageData->page = new Page(new ChromeClientGdk(page), new ContextMenuClientGdk, new EditorClientGdk(page), 0, new InspectorClientGdk);


    GTK_WIDGET_SET_FLAGS(page, GTK_CAN_FOCUS);

    g_signal_connect_after(G_OBJECT(page), "realize", G_CALLBACK(webkit_gtk_page_register_rendering_area_events), page);
    g_signal_connect_after(G_OBJECT(page), "size-allocate", G_CALLBACK(webkit_gtk_page_rendering_area_resize_callback), page);

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
