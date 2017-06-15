/*
 * Copyright (C) 2011 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef WebPopupMenuProxyGtk_h
#define WebPopupMenuProxyGtk_h

#include "WebPopupMenuProxy.h"
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/WTFString.h>

typedef struct _GMainLoop GMainLoop;
typedef struct _GdkEventKey GdkEventKey;

namespace WebCore {
class IntRect;
}

namespace WebKit {

class WebPageProxy;

class WebPopupMenuProxyGtk : public WebPopupMenuProxy {
public:
    static Ref<WebPopupMenuProxyGtk> create(GtkWidget* webView, WebPopupMenuProxy::Client& client)
    {
        return adoptRef(*new WebPopupMenuProxyGtk(webView, client));
    }
    ~WebPopupMenuProxyGtk();

    void showPopupMenu(const WebCore::IntRect&, WebCore::TextDirection, double pageScaleFactor, const Vector<WebPopupItem>&, const PlatformPopupMenuData&, int32_t selectedIndex) override;
    void hidePopupMenu() override;
    void cancelTracking() override;

protected:
    WebPopupMenuProxyGtk(GtkWidget*, WebPopupMenuProxy::Client&);

    GtkWidget* m_webView { nullptr };

private:
    void setCurrentlySelectedMenuItem(GtkWidget* item) { m_currentlySelectedMenuItem = item; }
    void populatePopupMenu(const Vector<WebPopupItem>&);
    void dismissMenuTimerFired();

    bool typeAheadFind(GdkEventKey*);
    void resetTypeAheadFindState();

    static void menuItemActivated(GtkMenuItem*, WebPopupMenuProxyGtk*);
    static void selectItemCallback(GtkWidget*, WebPopupMenuProxyGtk*);
    static gboolean keyPressEventCallback(GtkWidget*, GdkEventKey*, WebPopupMenuProxyGtk*);
    static void menuUnmappedCallback(GtkWidget*, WebPopupMenuProxyGtk*);

    GtkWidget* m_popup { nullptr };

    RunLoop::Timer<WebPopupMenuProxyGtk> m_dismissMenuTimer;

    // Typeahead find.
    unsigned m_previousKeyEventCharacter { 0 };
    uint32_t m_previousKeyEventTimestamp { 0 };
    GtkWidget* m_currentlySelectedMenuItem { nullptr };
    String m_currentSearchString;
};

} // namespace WebKit


#endif // WebPopupMenuProxyGtk_h
