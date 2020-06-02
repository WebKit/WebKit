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

#pragma once

#include "WebPopupMenuProxy.h"
#include <WebCore/GUniquePtrGtk.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/WTFString.h>

typedef struct _GdkDevice GdkDevice;
typedef struct _GtkTreePath GtkTreePath;
typedef struct _GtkTreeView GtkTreeView;
typedef struct _GtkTreeViewColumn GtkTreeViewColumn;
#if USE(GTK4)
typedef struct _GdkEvent GdkEvent;
#else
typedef union _GdkEvent GdkEvent;
#endif

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

    virtual void selectItem(unsigned itemIndex);
    virtual void activateItem(Optional<unsigned> itemIndex);

    bool handleKeyPress(unsigned keyval, uint32_t timestamp);
    void activateSelectedItem();

protected:
    WebPopupMenuProxyGtk(GtkWidget*, WebPopupMenuProxy::Client&);

    GtkWidget* m_webView { nullptr };

private:
    void createPopupMenu(const Vector<WebPopupItem>&, int32_t selectedIndex);
    void show();
    bool activateItemAtPath(GtkTreePath*);
    Optional<unsigned> typeAheadFindIndex(unsigned keyval, uint32_t timestamp);
    bool typeAheadFind(unsigned keyval, uint32_t timestamp);

#if !USE(GTK4)
    static gboolean buttonPressEventCallback(GtkWidget*, GdkEventButton*, WebPopupMenuProxyGtk*);
    static gboolean keyPressEventCallback(GtkWidget*, GdkEvent*, WebPopupMenuProxyGtk*);
    static gboolean treeViewButtonReleaseEventCallback(GtkWidget*, GdkEvent*, WebPopupMenuProxyGtk*);
#endif

    GtkWidget* m_popup { nullptr };
    GtkWidget* m_treeView { nullptr };
#if !USE(GTK4)
    GdkDevice* m_device { nullptr };
#endif

    Vector<GUniquePtr<GtkTreePath>> m_paths;
    Optional<unsigned> m_selectedItem;

    // Typeahead find.
    gunichar m_repeatingCharacter { '\0' };
    uint32_t m_previousKeyEventTime { 0 };
    GString* m_currentSearchString { nullptr };
};

} // namespace WebKit
