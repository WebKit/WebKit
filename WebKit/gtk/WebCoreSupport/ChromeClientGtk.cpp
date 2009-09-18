/*
 * Copyright (C) 2007, 2008 Holger Hans Peter Freyther
 * Copyright (C) 2007, 2008 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Gustavo Noronha Silva <gns@gnome.org>
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
#include "ChromeClientGtk.h"

#include "Console.h"
#include "FileSystem.h"
#include "FileChooser.h"
#include "FloatRect.h"
#include "FrameLoadRequest.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "CString.h"
#include "HitTestResult.h"
#include "KURL.h"
#include "webkitwebview.h"
#include "webkitnetworkrequest.h"
#include "webkitprivate.h"
#include "NotImplemented.h"
#include "WindowFeatures.h"
#if ENABLE(DATABASE)
#include "DatabaseTracker.h"
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

using namespace WebCore;

namespace WebKit {

ChromeClient::ChromeClient(WebKitWebView* webView)
    : m_webView(webView)
{
    ASSERT(m_webView);
}

void ChromeClient::chromeDestroyed()
{
    delete this;
}

FloatRect ChromeClient::windowRect()
{
    GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(m_webView));
    if (GTK_WIDGET_TOPLEVEL(window)) {
        gint left, top, width, height;
        gtk_window_get_position(GTK_WINDOW(window), &left, &top);
        gtk_window_get_size(GTK_WINDOW(window), &width, &height);
        return IntRect(left, top, width, height);
    }
    return FloatRect();
}

void ChromeClient::setWindowRect(const FloatRect& rect)
{
    IntRect intrect = IntRect(rect);
    WebKitWebWindowFeatures* webWindowFeatures = webkit_web_view_get_window_features(m_webView);

    g_object_set(webWindowFeatures,
                 "x", intrect.x(),
                 "y", intrect.y(),
                 "width", intrect.width(),
                 "height", intrect.height(),
                 NULL);
}

FloatRect ChromeClient::pageRect()
{
    GtkAllocation allocation = GTK_WIDGET(m_webView)->allocation;
    return IntRect(allocation.x, allocation.y, allocation.width, allocation.height);
}

float ChromeClient::scaleFactor()
{
    // Not implementable
    return 1.0;
}

void ChromeClient::focus()
{
    gtk_widget_grab_focus(GTK_WIDGET(m_webView));
}

void ChromeClient::unfocus()
{
    GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(m_webView));
    if (GTK_WIDGET_TOPLEVEL(window))
        gtk_window_set_focus(GTK_WINDOW(window), NULL);
}

Page* ChromeClient::createWindow(Frame* frame, const FrameLoadRequest& frameLoadRequest, const WindowFeatures& coreFeatures)
{
    WebKitWebView* webView = 0;

    g_signal_emit_by_name(m_webView, "create-web-view", kit(frame), &webView);

    if (!webView)
        return 0;

    WebKitWebWindowFeatures* webWindowFeatures = webkit_web_window_features_new_from_core_features(coreFeatures);
    g_object_set(webView, "window-features", webWindowFeatures, NULL);
    g_object_unref(webWindowFeatures);

    if (!frameLoadRequest.isEmpty())
        webkit_web_view_open(webView, frameLoadRequest.resourceRequest().url().string().utf8().data());

    return core(webView);
}

void ChromeClient::show()
{
    webkit_web_view_notify_ready(m_webView);
}

bool ChromeClient::canRunModal()
{
    notImplemented();
    return false;
}

void ChromeClient::runModal()
{
    notImplemented();
}

void ChromeClient::setToolbarsVisible(bool visible)
{
    WebKitWebWindowFeatures* webWindowFeatures = webkit_web_view_get_window_features(m_webView);

    g_object_set(webWindowFeatures, "toolbar-visible", visible, NULL);
}

bool ChromeClient::toolbarsVisible()
{
    WebKitWebWindowFeatures* webWindowFeatures = webkit_web_view_get_window_features(m_webView);
    gboolean visible;

    g_object_get(webWindowFeatures, "toolbar-visible", &visible, NULL);
    return visible;
}

void ChromeClient::setStatusbarVisible(bool visible)
{
    WebKitWebWindowFeatures* webWindowFeatures = webkit_web_view_get_window_features(m_webView);

    g_object_set(webWindowFeatures, "statusbar-visible", visible, NULL);
}

bool ChromeClient::statusbarVisible()
{
    WebKitWebWindowFeatures* webWindowFeatures = webkit_web_view_get_window_features(m_webView);
    gboolean visible;

    g_object_get(webWindowFeatures, "statusbar-visible", &visible, NULL);
    return visible;
}

void ChromeClient::setScrollbarsVisible(bool visible)
{
    WebKitWebWindowFeatures* webWindowFeatures = webkit_web_view_get_window_features(m_webView);

    g_object_set(webWindowFeatures, "scrollbar-visible", visible, NULL);
}

bool ChromeClient::scrollbarsVisible() {
    WebKitWebWindowFeatures* webWindowFeatures = webkit_web_view_get_window_features(m_webView);
    gboolean visible;

    g_object_get(webWindowFeatures, "scrollbar-visible", &visible, NULL);
    return visible;
}

void ChromeClient::setMenubarVisible(bool visible)
{
    WebKitWebWindowFeatures* webWindowFeatures = webkit_web_view_get_window_features(m_webView);

    g_object_set(webWindowFeatures, "menubar-visible", visible, NULL);
}

bool ChromeClient::menubarVisible()
{
    WebKitWebWindowFeatures* webWindowFeatures = webkit_web_view_get_window_features(m_webView);
    gboolean visible;

    g_object_get(webWindowFeatures, "menubar-visible", &visible, NULL);
    return visible;
}

void ChromeClient::setResizable(bool)
{
    // Ignored for now
}

void ChromeClient::closeWindowSoon()
{
    // We may not have a WebView as create-web-view can return NULL.
    if (!m_webView)
        return;

    webkit_web_view_stop_loading(m_webView);

    gboolean isHandled = false;
    g_signal_emit_by_name(m_webView, "close-web-view", &isHandled);

    if (isHandled)
        return;

    // FIXME: should we clear the frame group name here explicitly? Mac does it.
    // But this gets cleared in Page's destructor anyway.
    // webkit_web_view_set_group_name(m_webView, "");
}

bool ChromeClient::canTakeFocus(FocusDirection)
{
    return GTK_WIDGET_CAN_FOCUS(m_webView);
}

void ChromeClient::takeFocus(FocusDirection)
{
    unfocus();
}

bool ChromeClient::canRunBeforeUnloadConfirmPanel()
{
    return true;
}

bool ChromeClient::runBeforeUnloadConfirmPanel(const WebCore::String& message, WebCore::Frame* frame)
{
    return runJavaScriptConfirm(frame, message);
}

void ChromeClient::addMessageToConsole(WebCore::MessageSource source, WebCore::MessageType type, WebCore::MessageLevel level, const WebCore::String& message, unsigned int lineNumber, const WebCore::String& sourceId)
{
    gboolean retval;
    g_signal_emit_by_name(m_webView, "console-message", message.utf8().data(), lineNumber, sourceId.utf8().data(), &retval);
}

void ChromeClient::runJavaScriptAlert(Frame* frame, const String& message)
{
    gboolean retval;
    g_signal_emit_by_name(m_webView, "script-alert", kit(frame), message.utf8().data(), &retval);
}

bool ChromeClient::runJavaScriptConfirm(Frame* frame, const String& message)
{
    gboolean retval;
    gboolean didConfirm;
    g_signal_emit_by_name(m_webView, "script-confirm", kit(frame), message.utf8().data(), &didConfirm, &retval);
    return didConfirm == TRUE;
}

bool ChromeClient::runJavaScriptPrompt(Frame* frame, const String& message, const String& defaultValue, String& result)
{
    gboolean retval;
    gchar* value = 0;
    g_signal_emit_by_name(m_webView, "script-prompt", kit(frame), message.utf8().data(), defaultValue.utf8().data(), &value, &retval);
    if (value) {
        result = String::fromUTF8(value);
        g_free(value);
        return true;
    }
    return false;
}

void ChromeClient::setStatusbarText(const String& string)
{
    CString stringMessage = string.utf8();
    g_signal_emit_by_name(m_webView, "status-bar-text-changed", stringMessage.data());
}

bool ChromeClient::shouldInterruptJavaScript()
{
    notImplemented();
    return false;
}

bool ChromeClient::tabsToLinks() const
{
    return true;
}

IntRect ChromeClient::windowResizerRect() const
{
    notImplemented();
    return IntRect();
}

void ChromeClient::repaint(const IntRect& windowRect, bool contentChanged, bool immediate, bool repaintContentOnly)
{
    GdkRectangle rect = windowRect;
    GdkWindow* window = GTK_WIDGET(m_webView)->window;

    if (window) {
        if (contentChanged)
            gdk_window_invalidate_rect(window, &rect, FALSE);
        // We don't currently do immediate updates since they delay other UI elements.
        //if (immediate)
        //    gdk_window_process_updates(window, FALSE);
    }
}

void ChromeClient::scroll(const IntSize& delta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    GdkWindow* window = GTK_WIDGET(m_webView)->window;
    if (!window)
        return;

    // We cannot use gdk_window_scroll here because it is only able to
    // scroll the whole window at once, and we often need to scroll
    // portions of the window only (think frames).
    GdkRectangle area = clipRect;
    GdkRectangle moveRect;

    GdkRectangle sourceRect = area;
    sourceRect.x -= delta.width();
    sourceRect.y -= delta.height();

    GdkRegion* invalidRegion = gdk_region_rectangle(&area);

    if (gdk_rectangle_intersect(&area, &sourceRect, &moveRect)) {
        GdkRegion* moveRegion = gdk_region_rectangle(&moveRect);
        gdk_window_move_region(window, moveRegion, delta.width(), delta.height());
        gdk_region_offset(moveRegion, delta.width(), delta.height());
        gdk_region_subtract(invalidRegion, moveRegion);
        gdk_region_destroy(moveRegion);
    }

    gdk_window_invalidate_region(window, invalidRegion, FALSE);
    gdk_region_destroy(invalidRegion);
}

// FIXME: this does not take into account the WM decorations
static IntPoint widgetScreenPosition(GtkWidget* widget)
{
    GtkWidget* window = gtk_widget_get_toplevel(widget);
    int widgetX = 0, widgetY = 0;

    gtk_widget_translate_coordinates(widget, window, 0, 0, &widgetX, &widgetY);

    IntPoint result(widgetX, widgetY);
    int originX, originY;
    gdk_window_get_origin(window->window, &originX, &originY);
    result.move(originX, originY);

    return result;
}

IntRect ChromeClient::windowToScreen(const IntRect& rect) const
{
    IntRect result(rect);
    IntPoint screenPosition = widgetScreenPosition(GTK_WIDGET(m_webView));
    result.move(screenPosition.x(), screenPosition.y());

    return result;
}

IntPoint ChromeClient::screenToWindow(const IntPoint& point) const
{
    IntPoint result(point);
    IntPoint screenPosition = widgetScreenPosition(GTK_WIDGET(m_webView));
    result.move(-screenPosition.x(), -screenPosition.y());

    return result;
}

PlatformPageClietn ChromeClient::platformPageClient() const
{
    return GTK_WIDGET(m_webView);
}

void ChromeClient::contentsSizeChanged(Frame* frame, const IntSize& size) const
{
    // We need to queue a resize request only if the size changed,
    // otherwise we get into an infinite loop!
    GtkWidget* widget = GTK_WIDGET(m_webView);
    if (GTK_WIDGET_REALIZED(widget) &&
        (widget->requisition.height != size.height()) &&
        (widget->requisition.width != size.width()))
        gtk_widget_queue_resize_no_redraw(widget);
}

void ChromeClient::scrollbarsModeDidChange() const
{
    WebKitWebFrame* webFrame = webkit_web_view_get_main_frame(m_webView);

    g_object_notify(G_OBJECT(webFrame), "horizontal-scrollbar-policy");
    g_object_notify(G_OBJECT(webFrame), "vertical-scrollbar-policy");

    gboolean isHandled;
    g_signal_emit_by_name(webFrame, "scrollbars-policy-changed", &isHandled);

    if (isHandled)
        return;

    GtkWidget* parent = gtk_widget_get_parent(GTK_WIDGET(m_webView));
    if (!parent || !GTK_IS_SCROLLED_WINDOW(parent))
        return;

    GtkPolicyType horizontalPolicy = webkit_web_frame_get_horizontal_scrollbar_policy(webFrame);
    GtkPolicyType verticalPolicy = webkit_web_frame_get_vertical_scrollbar_policy(webFrame);

    // ScrolledWindow doesn't like to display only part of a widget if
    // the scrollbars are completely disabled; We have a disparity
    // here on what the policy requested by the web app is and what we
    // can represent; the idea is not to show scrollbars, only.
    if (horizontalPolicy == GTK_POLICY_NEVER)
        horizontalPolicy = GTK_POLICY_AUTOMATIC;

    if (verticalPolicy == GTK_POLICY_NEVER)
        verticalPolicy = GTK_POLICY_AUTOMATIC;

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(parent),
                                   horizontalPolicy, verticalPolicy);
}

void ChromeClient::mouseDidMoveOverElement(const HitTestResult& hit, unsigned modifierFlags)
{
    // If a tooltip must be displayed it will be, afterwards, when
    // setToolTip is called; this is just a work-around to make sure
    // it updates its location correctly; see
    // https://bugs.webkit.org/show_bug.cgi?id=15793.
    g_object_set(m_webView, "has-tooltip", FALSE, NULL);

    GdkDisplay* gdkDisplay;
    GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(m_webView));
    if (GTK_WIDGET_TOPLEVEL(window))
        gdkDisplay = gtk_widget_get_display(window);
    else
        gdkDisplay = gdk_display_get_default();
    gtk_tooltip_trigger_tooltip_query(gdkDisplay);

    // check if the element is a link...
    bool isLink = hit.isLiveLink();
    if (isLink) {
        KURL url = hit.absoluteLinkURL();
        if (!url.isEmpty() && url != m_hoveredLinkURL) {
            TextDirection dir;
            CString titleString = hit.title(dir).utf8();
            CString urlString = url.prettyURL().utf8();
            g_signal_emit_by_name(m_webView, "hovering-over-link", titleString.data(), urlString.data());
            m_hoveredLinkURL = url;
        }
    } else if (!isLink && !m_hoveredLinkURL.isEmpty()) {
        g_signal_emit_by_name(m_webView, "hovering-over-link", 0, 0);
        m_hoveredLinkURL = KURL();
    }
}

void ChromeClient::setToolTip(const String& toolTip, TextDirection)
{
#if GTK_CHECK_VERSION(2,12,0)
    if (toolTip.isEmpty())
        g_object_set(m_webView, "has-tooltip", FALSE, NULL);
    else
        gtk_widget_set_tooltip_text(GTK_WIDGET(m_webView), toolTip.utf8().data());
#else
    // TODO: Support older GTK+ versions
    // See http://bugs.webkit.org/show_bug.cgi?id=15793
    notImplemented();
#endif
}

void ChromeClient::print(Frame* frame)
{
    WebKitWebFrame* webFrame = kit(frame);
    gboolean isHandled = false;
    g_signal_emit_by_name(m_webView, "print-requested", webFrame, &isHandled);

    if (isHandled)
        return;

    webkit_web_frame_print(webFrame);
}

#if ENABLE(DATABASE)
void ChromeClient::exceededDatabaseQuota(Frame* frame, const String& databaseName)
{
    guint64 defaultQuota = webkit_get_default_web_database_quota();
    DatabaseTracker::tracker().setQuota(frame->document()->securityOrigin(), defaultQuota);

    WebKitWebFrame* webFrame = kit(frame);
    WebKitWebView* webView = getViewFromFrame(webFrame);

    WebKitSecurityOrigin* origin = webkit_web_frame_get_security_origin(webFrame);
    WebKitWebDatabase* webDatabase = webkit_security_origin_get_web_database(origin, databaseName.utf8().data());
    g_signal_emit_by_name(webView, "database-quota-exceeded", webFrame, webDatabase);
}
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
void ChromeClient::reachedMaxAppCacheSize(int64_t spaceNeeded)
{
    // FIXME: Free some space.
    notImplemented();
}
#endif

void ChromeClient::runOpenPanel(Frame*, PassRefPtr<FileChooser> prpFileChooser)
{
    RefPtr<FileChooser> chooser = prpFileChooser;

    GtkWidget* dialog = gtk_file_chooser_dialog_new(_("Upload File"),
                                                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(platformPageClient()))),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                                    NULL);

    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), chooser->allowsMultipleFiles());

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        if (gtk_file_chooser_get_select_multiple(GTK_FILE_CHOOSER(dialog))) {
            GSList* filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
            Vector<String> names;
            for (GSList* item = filenames ; item ; item = item->next) {
                if (!item->data)
                    continue;
                names.append(filenameToString(static_cast<char*>(item->data)));
                g_free(item->data);
            }
            g_slist_free(filenames);
            chooser->chooseFiles(names);
        } else {
            gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            if (filename)
                chooser->chooseFile(filenameToString(filename));
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
}

bool ChromeClient::setCursor(PlatformCursorHandle)
{
    notImplemented();
    return false;
}

void ChromeClient::requestGeolocationPermissionForFrame(Frame*, Geolocation*)
{
    // See the comment in WebCore/page/ChromeClient.h
    notImplemented();
}

}
