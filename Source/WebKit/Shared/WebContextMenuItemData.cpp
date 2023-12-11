/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(CONTEXT_MENUS)

#include "WebContextMenuItemData.h"

#include "APIObject.h"
#include "ArgumentCoders.h"
#include <wtf/text/CString.h>
#include <WebCore/ContextMenu.h>

namespace WebKit {
using namespace WebCore;

WebContextMenuItemData::WebContextMenuItemData()
    : m_type(WebCore::ContextMenuItemType::Action)
    , m_action(WebCore::ContextMenuItemTagNoAction)
    , m_enabled(true)
    , m_checked(false)
    , m_indentationLevel(0)
{
}

WebContextMenuItemData::WebContextMenuItemData(WebCore::ContextMenuItemType type, WebCore::ContextMenuAction action, String&& title, bool enabled, bool checked, unsigned indentationLevel, Vector<WebContextMenuItemData>&& submenu)
    : m_type(type)
    , m_action(action)
    , m_title(WTFMove(title))
    , m_enabled(enabled)
    , m_checked(checked)
    , m_indentationLevel(indentationLevel)
    , m_submenu(WTFMove(submenu))
{
}

WebContextMenuItemData::WebContextMenuItemData(const WebCore::ContextMenuItem& item)
    : m_type(item.type())
    , m_action(item.action())
    , m_title(item.title())
{
    if (m_type == WebCore::ContextMenuItemType::Submenu) {
        const Vector<WebCore::ContextMenuItem>& coreSubmenu = item.subMenuItems();
        m_submenu = kitItems(coreSubmenu);
    }
    
    m_enabled = item.enabled();
    m_checked = item.checked();
    m_indentationLevel = item.indentationLevel();
}

ContextMenuItem WebContextMenuItemData::core() const
{
    if (m_type != ContextMenuItemType::Submenu)
        return ContextMenuItem(m_type, m_action, m_title, m_enabled, m_checked, m_indentationLevel);
    
    Vector<ContextMenuItem> subMenuItems = coreItems(m_submenu);
    return ContextMenuItem(m_action, m_title, m_enabled, m_checked, subMenuItems, m_indentationLevel);
}

API::Object* WebContextMenuItemData::userData() const
{
    return m_userData.get();
}

void WebContextMenuItemData::setUserData(API::Object* userData)
{
    m_userData = userData;
}

Vector<WebContextMenuItemData> kitItems(const Vector<WebCore::ContextMenuItem>& coreItemVector)
{
    return coreItemVector.map([](auto& item) {
        return WebContextMenuItemData { item };
    });
}

Vector<ContextMenuItem> coreItems(const Vector<WebContextMenuItemData>& kitItemVector)
{
    return kitItemVector.map([](auto& item) {
        return item.core();
    });
}

} // namespace WebKit
#endif // ENABLE(CONTEXT_MENUS)
