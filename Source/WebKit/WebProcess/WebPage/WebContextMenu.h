/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#if ENABLE(CONTEXT_MENUS)

#include "WebContextMenuItemData.h"

#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class Image;
}

namespace WebKit {

class ShareableBitmap;
class WebPage;

class WebContextMenu : public RefCounted<WebContextMenu> {
public:
    static Ref<WebContextMenu> create(WebPage& page)
    {
        return adoptRef(*new WebContextMenu(page));
    }
    
    ~WebContextMenu();

    void show();
    void itemSelected(const WebContextMenuItemData&);
    Vector<WebContextMenuItemData> items() const;

private:
    WebContextMenu(WebPage&);
    void menuItemsWithUserData(Vector<WebContextMenuItemData>&, RefPtr<API::Object>&) const;

    WeakPtr<WebPage> m_page;
};

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
