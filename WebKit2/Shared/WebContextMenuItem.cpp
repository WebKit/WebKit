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

#include "WebContextMenuItem.h"

#include "ArgumentCoders.h"
#include "Arguments.h"
#include <wtf/text/CString.h>
#include <WebCore/ContextMenu.h>

namespace WebKit {

WebContextMenuItem::WebContextMenuItem()
    : m_type(WebCore::ActionType)
    , m_action(WebCore::ContextMenuItemTagNoAction)
    , m_enabled(true)
    , m_checked(false)
{
}

WebContextMenuItem::WebContextMenuItem(WebCore::ContextMenuItemType type, WebCore::ContextMenuAction action, const String& title, bool enabled, bool checked)
    : m_type(type)
    , m_action(action)
    , m_title(title)
    , m_enabled(enabled)
    , m_checked(checked)
{
    ASSERT(type == WebCore::ActionType || type == WebCore::CheckableActionType || type == WebCore::SeparatorType);
}

WebContextMenuItem::WebContextMenuItem(WebCore::ContextMenuAction action, const String& title, bool enabled, const Vector<WebContextMenuItem>& submenu)
    : m_type(WebCore::SubmenuType)
    , m_action(action)
    , m_title(title)
    , m_enabled(enabled)
    , m_checked(false)
    , m_submenu(submenu)
{
}

WebContextMenuItem::WebContextMenuItem(WebCore::ContextMenuItem& item, WebCore::ContextMenu* menu)
    : m_type(item.type())
    , m_action(item.action())
    , m_title(item.title())
{
    if (m_type == WebCore::SubmenuType) {
        Vector<WebCore::ContextMenuItem> coreSubmenu = WebCore::contextMenuItemVector(item.platformSubMenu());
        m_submenu = kitItems(coreSubmenu, menu);
    }
    
    menu->checkOrEnableIfNeeded(item);
    
    m_enabled = item.enabled();
    m_checked = item.checked();
}

void WebContextMenuItem::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(CoreIPC::In(static_cast<uint32_t>(m_type), static_cast<uint32_t>(m_action), m_title, m_checked, m_enabled, m_submenu));
}

bool WebContextMenuItem::decode(CoreIPC::ArgumentDecoder* decoder, WebContextMenuItem& item)
{
    uint32_t type;
    uint32_t action;
    String title;
    bool checked;
    bool enabled;
    Vector<WebContextMenuItem> submenu;

    if (!decoder->decode(CoreIPC::Out(type, action, title, checked, enabled, submenu)))
        return false;

    switch (type) {
    case WebCore::ActionType:
    case WebCore::SeparatorType:
    case WebCore::CheckableActionType:
        item = WebContextMenuItem(static_cast<WebCore::ContextMenuItemType>(type), static_cast<WebCore::ContextMenuAction>(action), title, enabled, checked);
        break;
    case WebCore::SubmenuType:
        item = WebContextMenuItem(static_cast<WebCore::ContextMenuAction>(action), title, enabled, submenu);
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

Vector<WebContextMenuItem> kitItems(Vector<WebCore::ContextMenuItem>& coreItems, WebCore::ContextMenu* menu)
{
    Vector<WebContextMenuItem> result;
    result.reserveCapacity(coreItems.size());
    for (unsigned i = 0; i < coreItems.size(); ++i)
        result.append(WebContextMenuItem(coreItems[i], menu));
    
    return result;
}

} // namespace WebKit
