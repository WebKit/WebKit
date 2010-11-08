/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Kenneth Rohde Christiansen
 * Copyright (C) 2008 Diego Gonzalez
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ChromeClientEfl.h"

#if ENABLE(DATABASE)
#include "DatabaseDetails.h"
#include "DatabaseTracker.h"
#endif
#include "EWebKit.h"
#include "FileChooser.h"
#include "FloatRect.h"
#include "FrameLoader.h"
#include "FrameLoaderClientEfl.h"
#include "HitTestResult.h"
#include "IntRect.h"
#include "KURL.h"
#include "NavigationAction.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "PopupMenuEfl.h"
#include "SearchPopupMenuEfl.h"
#include "ViewportArguments.h"
#include "WindowFeatures.h"
#include "ewk_private.h"
#include <Ecore_Evas.h>
#include <Evas.h>
#include <wtf/text/CString.h>

using namespace WebCore;

static inline Evas_Object* kit(Frame* frame)
{
    if (!frame)
        return 0;

    FrameLoaderClientEfl* client = static_cast<FrameLoaderClientEfl*>(frame->loader()->client());
    return client ? client->webFrame() : 0;
}

namespace WebCore {

ChromeClientEfl::ChromeClientEfl(Evas_Object* view)
    : m_view(view)
{
}

ChromeClientEfl::~ChromeClientEfl()
{
}

void ChromeClientEfl::chromeDestroyed()
{
    delete this;
}

void ChromeClientEfl::focusedNodeChanged(Node*)
{
    notImplemented();
}

void ChromeClientEfl::focusedFrameChanged(Frame*)
{
}

FloatRect ChromeClientEfl::windowRect()
{
    Ecore_Evas* ee = 0;
    int x, y, w, h;

    if (!m_view)
        return FloatRect();

    ee = ecore_evas_ecore_evas_get(evas_object_evas_get(m_view));
    ecore_evas_geometry_get(ee, &x, &y, &w, &h);
    return FloatRect(x, y, w, h);
}

void ChromeClientEfl::setWindowRect(const FloatRect& rect)
{
    Ecore_Evas* ee = 0;
    IntRect intrect = IntRect(rect);

    if (!m_view)
        return;

    if (!ewk_view_setting_enable_auto_resize_window_get(m_view))
        return;

    ee = ecore_evas_ecore_evas_get(evas_object_evas_get(m_view));
    ecore_evas_move(ee, intrect.x(), intrect.y());
    ecore_evas_resize(ee, intrect.width(), intrect.height());
}

FloatRect ChromeClientEfl::pageRect()
{
    if (!m_view)
        return FloatRect();

    return ewk_view_page_rect_get(m_view);
}

float ChromeClientEfl::scaleFactor()
{
    notImplemented();
    return 1.0;
}

void ChromeClientEfl::focus()
{
    evas_object_focus_set(m_view, EINA_TRUE);
}

void ChromeClientEfl::unfocus()
{
    evas_object_focus_set(m_view, EINA_FALSE);
}

Page* ChromeClientEfl::createWindow(Frame*, const FrameLoadRequest& frameLoadRequest, const WindowFeatures& features, const NavigationAction&)
{
    Evas_Object* newView = ewk_view_window_create(m_view, EINA_TRUE, &features);
    if (!newView)
        return 0;

    if (!frameLoadRequest.isEmpty())
        ewk_view_uri_set(newView, frameLoadRequest.resourceRequest().url().string().utf8().data());

    return ewk_view_core_page_get(newView);
}

void ChromeClientEfl::show()
{
    ewk_view_ready(m_view);
}

bool ChromeClientEfl::canRunModal()
{
    notImplemented();
    return false;
}

void ChromeClientEfl::runModal()
{
    notImplemented();
}

void ChromeClientEfl::setToolbarsVisible(bool visible)
{
    ewk_view_toolbars_visible_set(m_view, visible);
}

bool ChromeClientEfl::toolbarsVisible()
{
    Eina_Bool visible;

    ewk_view_toolbars_visible_get(m_view, &visible);
    return visible;
}

void ChromeClientEfl::setStatusbarVisible(bool visible)
{
    ewk_view_statusbar_visible_set(m_view, visible);
}

bool ChromeClientEfl::statusbarVisible()
{
    Eina_Bool visible;

    ewk_view_statusbar_visible_get(m_view, &visible);
    return visible;
}

void ChromeClientEfl::setScrollbarsVisible(bool visible)
{
    ewk_view_scrollbars_visible_set(m_view, visible);
}

bool ChromeClientEfl::scrollbarsVisible()
{
    Eina_Bool visible;

    ewk_view_scrollbars_visible_get(m_view, &visible);
    return visible;
}

void ChromeClientEfl::setMenubarVisible(bool visible)
{
    ewk_view_menubar_visible_set(m_view, visible);
}

bool ChromeClientEfl::menubarVisible()
{
    Eina_Bool visible;

    ewk_view_menubar_visible_get(m_view, &visible);
    return visible;
}

void ChromeClientEfl::createSelectPopup(PopupMenuClient* client, int selected, const IntRect& rect)
{
    ewk_view_popup_new(m_view, client, selected, rect);
}

bool ChromeClientEfl::destroySelectPopup()
{
    return ewk_view_popup_destroy(m_view);
}

void ChromeClientEfl::setResizable(bool)
{
    notImplemented();
}

void ChromeClientEfl::closeWindowSoon()
{
    ewk_view_window_close(m_view);
}

bool ChromeClientEfl::canTakeFocus(FocusDirection)
{
    // This is called when cycling through links/focusable objects and we
    // reach the last focusable object.
    return false;
}

void ChromeClientEfl::takeFocus(FocusDirection)
{
    unfocus();
}

bool ChromeClientEfl::canRunBeforeUnloadConfirmPanel()
{
    return true;
}

bool ChromeClientEfl::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    return runJavaScriptConfirm(frame, message);
}

void ChromeClientEfl::addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message,
                                          unsigned int lineNumber, const String& sourceID)
{
    ewk_view_add_console_message(m_view, message.utf8().data(), lineNumber, sourceID.utf8().data());
}

void ChromeClientEfl::runJavaScriptAlert(Frame* frame, const String& message)
{
    ewk_view_run_javascript_alert(m_view, kit(frame), message.utf8().data());
}

bool ChromeClientEfl::runJavaScriptConfirm(Frame* frame, const String& message)
{
    return ewk_view_run_javascript_confirm(m_view, kit(frame), message.utf8().data());
}

bool ChromeClientEfl::runJavaScriptPrompt(Frame* frame, const String& message, const String& defaultValue, String& result)
{
    char* value = 0;
    ewk_view_run_javascript_prompt(m_view, kit(frame), message.utf8().data(), defaultValue.utf8().data(), &value);
    if (value) {
        result = String::fromUTF8(value);
        free(value);
        return true;
    }
    return false;
}

void ChromeClientEfl::setStatusbarText(const String& string)
{
    ewk_view_statusbar_text_set(m_view, string.utf8().data());
}

bool ChromeClientEfl::shouldInterruptJavaScript()
{
    return ewk_view_should_interrupt_javascript(m_view);
}

bool ChromeClientEfl::tabsToLinks() const
{
    return true;
}

IntRect ChromeClientEfl::windowResizerRect() const
{
    notImplemented();
    // Implementing this function will make repaint being
    // called during resize, but as this will be done with
    // a minor delay it adds a weird "filling" effect due
    // to us using an evas image for showing the cairo
    // context. So instead of implementing this function
    // we call paint directly during resize with
    // the new object size as its argument.
    return IntRect();
}

void ChromeClientEfl::contentsSizeChanged(Frame* frame, const IntSize& size) const
{
    ewk_frame_contents_size_changed(kit(frame), size.width(), size.height());
    if (ewk_view_frame_main_get(m_view) == kit(frame))
        ewk_view_contents_size_changed(m_view, size.width(), size.height());
}

IntRect ChromeClientEfl::windowToScreen(const IntRect& rect) const
{
    notImplemented();
    return rect;
}

IntPoint ChromeClientEfl::screenToWindow(const IntPoint& point) const
{
    notImplemented();
    return point;
}

PlatformPageClient ChromeClientEfl::platformPageClient() const
{
    return m_view;
}

void ChromeClientEfl::scrollbarsModeDidChange() const
{
}

void ChromeClientEfl::mouseDidMoveOverElement(const HitTestResult& hit, unsigned modifierFlags)
{
    // FIXME, compare with old link, look at Qt impl.
    bool isLink = hit.isLiveLink();
    if (isLink) {
        KURL url = hit.absoluteLinkURL();
        if (!url.isEmpty() && url != m_hoveredLinkURL) {
            const char* link[2];
            TextDirection dir;
            CString urlStr = url.prettyURL().utf8();
            CString titleStr = hit.title(dir).utf8();
            link[0] = urlStr.data();
            link[1] = titleStr.data();
            ewk_view_mouse_link_hover_in(m_view, link);
            m_hoveredLinkURL = url;
        }
    } else if (!isLink && !m_hoveredLinkURL.isEmpty()) {
        ewk_view_mouse_link_hover_out(m_view);
        m_hoveredLinkURL = KURL();
    }
}

void ChromeClientEfl::setToolTip(const String& toolTip, TextDirection)
{
    ewk_view_tooltip_text_set(m_view, toolTip.utf8().data());
}

void ChromeClientEfl::print(Frame* frame)
{
    notImplemented();
}

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
void ChromeClientEfl::reachedMaxAppCacheSize(int64_t spaceNeeded)
{
    // FIXME: Free some space.
    notImplemented();
}

void ChromeClientEfl::reachedApplicationCacheOriginQuota(SecurityOrigin*)
{
    notImplemented();
}
#endif

#if ENABLE(DATABASE)
void ChromeClientEfl::exceededDatabaseQuota(Frame* frame, const String& databaseName)
{
    uint64_t quota;
    SecurityOrigin* origin = frame->document()->securityOrigin();

    DatabaseDetails details = DatabaseTracker::tracker().detailsForNameAndOrigin(databaseName, origin);
    quota = ewk_view_exceeded_database_quota(m_view,
            kit(frame), databaseName.utf8().data(),
            details.currentUsage(), details.expectedUsage());

    /* if client did not set quota, and database is being created now, the
     * default quota is applied
     */
    if (!quota && !DatabaseTracker::tracker().hasEntryForOrigin(origin))
        quota = ewk_settings_web_database_default_quota_get();

    DatabaseTracker::tracker().setQuota(origin, quota);
}
#endif

void ChromeClientEfl::runOpenPanel(Frame* frame, PassRefPtr<FileChooser> prpFileChooser)
{
    RefPtr<FileChooser> chooser = prpFileChooser;
    bool confirm;
    Eina_List* selectedFilenames = 0;
    Eina_List* suggestedFilenames = 0;
    void* filename;
    Vector<String> filenames;

    for (unsigned i = 0; i < chooser->filenames().size(); i++) {
        CString str = chooser->filenames()[i].utf8();
        filename = strdup(str.data());
        suggestedFilenames = eina_list_append(suggestedFilenames, filename);
    }

    confirm = ewk_view_run_open_panel(m_view, kit(frame), chooser->allowsMultipleFiles(), suggestedFilenames, &selectedFilenames);
    EINA_LIST_FREE(suggestedFilenames, filename)
        free(filename);

    if (!confirm)
        return;

    EINA_LIST_FREE(selectedFilenames, filename) {
        filenames.append((char *)filename);
        free(filename);
    }

    if (chooser->allowsMultipleFiles())
        chooser->chooseFiles(filenames);
    else
        chooser->chooseFile(filenames[0]);
}

void ChromeClientEfl::formStateDidChange(const Node*)
{
    notImplemented();
}

void ChromeClientEfl::setCursor(const Cursor&)
{
    notImplemented();
}

void ChromeClientEfl::requestGeolocationPermissionForFrame(Frame*, Geolocation*)
{
    // See the comment in WebCore/page/ChromeClient.h
    notImplemented();
}

void ChromeClientEfl::cancelGeolocationPermissionRequestForFrame(Frame*, Geolocation*)
{
    notImplemented();
}

void ChromeClientEfl::cancelGeolocationPermissionForFrame(Frame*, Geolocation*)
{
    notImplemented();
}

void ChromeClientEfl::invalidateContents(const IntRect& updateRect, bool immediate)
{
    notImplemented();
}

void ChromeClientEfl::invalidateWindow(const IntRect& updateRect, bool immediate)
{
    notImplemented();
}

void ChromeClientEfl::invalidateContentsAndWindow(const IntRect& updateRect, bool immediate)
{
    Evas_Coord x, y, w, h;

    x = updateRect.x();
    y = updateRect.y();
    w = updateRect.width();
    h = updateRect.height();
    ewk_view_repaint(m_view, x, y, w, h);
}

void ChromeClientEfl::invalidateContentsForSlowScroll(const IntRect& updateRect, bool immediate)
{
    invalidateContentsAndWindow(updateRect, immediate);
}

void ChromeClientEfl::scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    ewk_view_scroll(m_view, scrollDelta.width(), scrollDelta.height(), rectToScroll.x(), rectToScroll.y(), rectToScroll.width(), rectToScroll.height(), clipRect.x(), clipRect.y(), clipRect.width(), clipRect.height(), EINA_TRUE);
}

void ChromeClientEfl::cancelGeolocationPermissionRequestForFrame(Frame*)
{
    notImplemented();
}

void ChromeClientEfl::iconForFiles(const Vector<String, 0u>&, PassRefPtr<FileChooser>)
{
    notImplemented();
}

void ChromeClientEfl::chooseIconForFiles(const Vector<String>&, FileChooser*)
{
    notImplemented();
}

void ChromeClientEfl::dispatchViewportDataDidChange(const ViewportArguments& arguments) const
{
    ewk_view_viewport_attributes_set(m_view, arguments);
}

bool ChromeClientEfl::selectItemWritingDirectionIsNatural()
{
    return true;
}

PassRefPtr<PopupMenu> ChromeClientEfl::createPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new PopupMenuEfl(client));
}

PassRefPtr<SearchPopupMenu> ChromeClientEfl::createSearchPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new SearchPopupMenuEfl(client));
}

}
