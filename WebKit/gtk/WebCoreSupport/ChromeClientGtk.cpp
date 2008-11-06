/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007, 2008 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
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

#include "FileSystem.h"
#include "FileChooser.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "CString.h"
#include "HitTestResult.h"
#include "KURL.h"
#include "webkitwebview.h"
#include "webkitprivate.h"
#include "NotImplemented.h"
#include "WindowFeatures.h"
#if ENABLE(DATABASE)
#include "DatabaseTracker.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

using namespace WebCore;

namespace WebKit {

ChromeClient::ChromeClient(WebKitWebView* webView)
    : m_webView(webView)
{
}

void ChromeClient::chromeDestroyed()
{
    delete this;
}

FloatRect ChromeClient::windowRect()
{
    if (!m_webView)
        return FloatRect();
    GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(m_webView));
    if (window) {
        gint left, top, width, height;
        gtk_window_get_position(GTK_WINDOW(window), &left, &top);
        gtk_window_get_size(GTK_WINDOW(window), &width, &height);
        return IntRect(left, top, width, height);
    }
    return FloatRect();
}

void ChromeClient::setWindowRect(const FloatRect& r)
{
    notImplemented();
}

FloatRect ChromeClient::pageRect()
{
    if (!m_webView)
        return FloatRect();
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
    if (!m_webView)
        return;
    gtk_widget_grab_focus(GTK_WIDGET(m_webView));
}

void ChromeClient::unfocus()
{
    if (!m_webView)
        return;
    GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(m_webView));
    if (window)
        gtk_window_set_focus(GTK_WINDOW(window), NULL);
}

Page* ChromeClient::createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures& features)
{
    if (features.dialog) {
        notImplemented();
        return 0;
    } else {
        /* TODO: FrameLoadRequest is not used */
        WebKitWebView* webView = WEBKIT_WEB_VIEW_GET_CLASS(m_webView)->create_web_view(m_webView);
        if (!webView)
            return 0;

        WebKitWebViewPrivate* privateData = WEBKIT_WEB_VIEW_GET_PRIVATE(webView);
        return privateData->corePage;
    }
}

void ChromeClient::show()
{
    notImplemented();
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

void ChromeClient::setToolbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClient::toolbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClient::setStatusbarVisible(bool)
{
    notImplemented();
}

bool ChromeClient::statusbarVisible()
{
    notImplemented();
    return false;
}

void ChromeClient::setScrollbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClient::scrollbarsVisible() {
    notImplemented();
    return false;
}

void ChromeClient::setMenubarVisible(bool)
{
    notImplemented();
}

bool ChromeClient::menubarVisible()
{
    notImplemented();
    return false;
}

void ChromeClient::setResizable(bool)
{
    notImplemented();
}

void ChromeClient::closeWindowSoon()
{
    notImplemented();
}

bool ChromeClient::canTakeFocus(FocusDirection)
{
    if (!m_webView)
        return false;
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

void ChromeClient::addMessageToConsole(const WebCore::String& message, unsigned int lineNumber, const WebCore::String& sourceId)
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
    if (!m_webView)
        return;

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
    if (!m_webView)
        return;

    GdkWindow* window = GTK_WIDGET(m_webView)->window;
    if (!window)
        return;

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

IntRect ChromeClient::windowToScreen(const IntRect& rect) const
{
    notImplemented();
    return rect;
}

IntPoint ChromeClient::screenToWindow(const IntPoint& point) const
{
    notImplemented();
    return point;
}

PlatformWidget ChromeClient::platformWindow() const
{
    return m_webView ? GTK_WIDGET(m_webView) : 0;
}

void ChromeClient::mouseDidMoveOverElement(const HitTestResult& hit, unsigned modifierFlags)
{
    // check if the element is a link...
    bool isLink = hit.isLiveLink();
    if (isLink) {
        KURL url = hit.absoluteLinkURL();
        if (!url.isEmpty() && url != m_hoveredLinkURL) {
            CString titleString = hit.title().utf8();
            CString urlString = url.prettyURL().utf8();
            g_signal_emit_by_name(m_webView, "hovering-over-link", titleString.data(), urlString.data());
            m_hoveredLinkURL = url;
        }
    } else if (!isLink && !m_hoveredLinkURL.isEmpty()) {
        g_signal_emit_by_name(m_webView, "hovering-over-link", 0, 0);
        m_hoveredLinkURL = KURL();
    }
}

void ChromeClient::setToolTip(const String& toolTip)
{
#if GTK_CHECK_VERSION(2,12,0)
    if (toolTip.isEmpty())
        g_object_set(G_OBJECT(m_webView), "has-tooltip", FALSE, NULL);
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
    webkit_web_frame_print(kit(frame));
}

void ChromeClient::exceededDatabaseQuota(Frame* frame, const String&)
{
#if ENABLE(DATABASE)
    // Set to 5M for testing
    // FIXME: Make this configurable
    notImplemented();
    const unsigned long long defaultQuota = 5 * 1024 * 1024;
    DatabaseTracker::tracker().setQuota(frame->document()->securityOrigin(), defaultQuota);
#endif
}

void ChromeClient::runOpenPanel(Frame*, PassRefPtr<FileChooser> prpFileChooser)
{
    RefPtr<FileChooser> chooser = prpFileChooser;

    GtkWidget* dialog = gtk_file_chooser_dialog_new(_("Upload File"),
                                                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(platformWindow()))),
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

}
