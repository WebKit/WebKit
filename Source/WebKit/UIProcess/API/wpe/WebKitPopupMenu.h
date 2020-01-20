/*
 * Copyright (C) 2020 Igalia S.L.
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
#include <wtf/glib/GRefPtr.h>

typedef struct _WebKitOptionMenu WebKitOptionMenu;

namespace WKWPE {
class View;
}

namespace WebKit {

class WebKitPopupMenu final : public WebPopupMenuProxy {
public:
    static Ref<WebKitPopupMenu> create(WKWPE::View&, WebPopupMenuProxy::Client&);
    ~WebKitPopupMenu() = default;

    void selectItem(unsigned itemIndex);
    void activateItem(Optional<unsigned> itemIndex);

private:
    WebKitPopupMenu(WKWPE::View&, WebPopupMenuProxy::Client&);

    void showPopupMenu(const WebCore::IntRect&, WebCore::TextDirection, double pageScaleFactor, const Vector<WebPopupItem>&, const PlatformPopupMenuData&, int32_t selectedIndex) override;
    void hidePopupMenu() override;
    void cancelTracking() override;

    WKWPE::View& m_view;
    GRefPtr<WebKitOptionMenu> m_menu;
    Optional<unsigned> m_selectedItem;
};

} // namespace WebKit
