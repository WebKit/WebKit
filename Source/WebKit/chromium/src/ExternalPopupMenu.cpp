/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ExternalPopupMenu.h"

#include "FrameView.h"
#include "IntPoint.h"
#include "PopupMenuClient.h"
#include "TextDirection.h"
#include "WebExternalPopupMenu.h"
#include "WebMenuItemInfo.h"
#include "WebPopupMenuInfo.h"
#include "WebVector.h"
#include "WebViewClient.h"

using namespace WebCore;

namespace WebKit {

ExternalPopupMenu::ExternalPopupMenu(PopupMenuClient* popupMenuClient,
                                     WebViewClient* webViewClient)
    : m_popupMenuClient(popupMenuClient)
    , m_webViewClient(webViewClient)
    , m_webExternalPopupMenu(0)
{
}

ExternalPopupMenu::~ExternalPopupMenu()
{
}

void ExternalPopupMenu::show(const IntRect& rect, FrameView* v, int index)
{
    // WebCore reuses the PopupMenu of a page.
    // For simplicity, we do recreate the actual external popup everytime.
    hide();

    WebPopupMenuInfo info;
    getPopupMenuInfo(&info);
    if (info.items.isEmpty())
        return;
    m_webExternalPopupMenu =
        m_webViewClient->createExternalPopupMenu(info, this);
    m_webExternalPopupMenu->show(v->contentsToWindow(rect));
}

void ExternalPopupMenu::hide()
{
    if (m_popupMenuClient)
        m_popupMenuClient->popupDidHide();
    if (!m_webExternalPopupMenu)
        return;
    m_webExternalPopupMenu->close();
    m_webExternalPopupMenu = 0;
}

void ExternalPopupMenu::updateFromElement()
{
}

void ExternalPopupMenu::disconnectClient()
{
    hide();
    m_popupMenuClient = 0;
}

void ExternalPopupMenu::didChangeSelection(int index)
{
    if (m_popupMenuClient)
        m_popupMenuClient->selectionChanged(index);
}

void ExternalPopupMenu::didAcceptIndex(int index)
{
    // Calling methods on the PopupMenuClient might lead to this object being
    // derefed. This ensures it does not get deleted while we are running this
    // method.
    RefPtr<ExternalPopupMenu> guard(this);

    if (m_popupMenuClient) {
        m_popupMenuClient->valueChanged(index);
        // The call to valueChanged above might have lead to a call to
        // disconnectClient, so we might not have a PopupMenuClient anymore.
        if (m_popupMenuClient)
            m_popupMenuClient->popupDidHide();
    }
    m_webExternalPopupMenu = 0;
}

void ExternalPopupMenu::didCancel()
{
    // See comment in didAcceptIndex on why we need this.
    RefPtr<ExternalPopupMenu> guard(this);

    if (m_popupMenuClient)
        m_popupMenuClient->popupDidHide();
    m_webExternalPopupMenu = 0;
}

void ExternalPopupMenu::getPopupMenuInfo(WebPopupMenuInfo* info)
{
    int itemCount = m_popupMenuClient->listSize();
    WebVector<WebPopupMenuInfo::Item> items(
        static_cast<size_t>(itemCount));
    for (int i = 0; i < itemCount; ++i) {
        WebPopupMenuInfo::Item& popupItem = items[i];
        popupItem.label = m_popupMenuClient->itemText(i);
        if (m_popupMenuClient->itemIsSeparator(i))
            popupItem.type = WebMenuItemInfo::Separator;
        else if (m_popupMenuClient->itemIsLabel(i))
            popupItem.type = WebMenuItemInfo::Group;
        else
            popupItem.type = WebMenuItemInfo::Option;
        popupItem.enabled = m_popupMenuClient->itemIsEnabled(i);
    }

    info->itemHeight = m_popupMenuClient->menuStyle().font().height();
    info->itemFontSize =
        static_cast<int>(m_popupMenuClient->menuStyle().font().size());
    info->selectedIndex = m_popupMenuClient->selectedIndex();
    info->rightAligned =
        m_popupMenuClient->menuStyle().textDirection() == WebCore::RTL;
    info->items.swap(items);
}

} // namespace WebKit
