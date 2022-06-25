/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef SearchPopupMenuIOS_h
#define SearchPopupMenuIOS_h

#include "PopupMenuIOS.h"
#include <WebCore/SearchPopupMenu.h>

namespace WebCore {
class PopupMenuClient;
}

class SearchPopupMenuIOS : public WebCore::SearchPopupMenu {
public:
    SearchPopupMenuIOS(WebCore::PopupMenuClient*);

    WebCore::PopupMenu* popupMenu() override;
    void saveRecentSearches(const AtomString& name, const Vector<WebCore::RecentSearch>&) override;
    void loadRecentSearches(const AtomString& name, Vector<WebCore::RecentSearch>&) override;
    bool enabled() override;

private:
    Ref<PopupMenuIOS> m_popup;
};

#endif // SearchPopupMenuIOS_h
