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
    : m_type(WebCore::ActionType)
    , m_action(WebCore::ContextMenuItemTagNoAction)
    , m_enabled(true)
    , m_checked(false)
{
}

WebContextMenuItemData::WebContextMenuItemData(WebCore::ContextMenuItemType type, WebCore::ContextMenuAction action, const String& title, bool enabled, bool checked)
    : m_type(type)
    , m_action(action)
    , m_title(title)
    , m_enabled(enabled)
    , m_checked(checked)
{
    ASSERT(type == WebCore::ActionType || type == WebCore::CheckableActionType || type == WebCore::SeparatorType);
}

WebContextMenuItemData::WebContextMenuItemData(WebCore::ContextMenuAction action, const String& title, bool enabled, const Vector<WebContextMenuItemData>& submenu)
    : m_type(WebCore::SubmenuType)
    , m_action(action)
    , m_title(title)
    , m_enabled(enabled)
    , m_checked(false)
    , m_submenu(submenu)
{
}

WebContextMenuItemData::WebContextMenuItemData(const WebCore::ContextMenuItem& item)
    : m_type(item.type())
    , m_action(item.action())
    , m_title(item.title())
{
    if (m_type == WebCore::SubmenuType) {
        const Vector<WebCore::ContextMenuItem>& coreSubmenu = item.subMenuItems();
        m_submenu = kitItems(coreSubmenu);
    }
    
    m_enabled = item.enabled();
    m_checked = item.checked();
}

ContextMenuItem WebContextMenuItemData::core() const
{
    if (m_type != SubmenuType)
        return ContextMenuItem(m_type, m_action, m_title, m_enabled, m_checked);
    
    Vector<ContextMenuItem> subMenuItems = coreItems(m_submenu);
    return ContextMenuItem(m_action, m_title, m_enabled, m_checked, subMenuItems);
}

API::Object* WebContextMenuItemData::userData() const
{
    return m_userData.get();
}

void WebContextMenuItemData::setUserData(API::Object* userData)
{
    m_userData = userData;
}
    
void WebContextMenuItemData::encode(IPC::Encoder& encoder) const
{
    encoder << m_type;
    encoder << m_action;
    encoder << m_title;
    encoder << m_checked;
    encoder << m_enabled;
    encoder << m_submenu;
}

Optional<WebContextMenuItemData> WebContextMenuItemData::decode(IPC::Decoder& decoder)
{
    WebCore::ContextMenuItemType type;
    if (!decoder.decode(type))
        return WTF::nullopt;

    WebCore::ContextMenuAction action;
    if (!decoder.decode(action))
        return WTF::nullopt;

    String title;
    if (!decoder.decode(title))
        return WTF::nullopt;

    bool checked;
    if (!decoder.decode(checked))
        return WTF::nullopt;

    bool enabled;
    if (!decoder.decode(enabled))
        return WTF::nullopt;

    Optional<Vector<WebContextMenuItemData>> submenu;
    decoder >> submenu;
    if (!submenu)
        return WTF::nullopt;

    switch (type) {
    case WebCore::ActionType:
    case WebCore::SeparatorType:
    case WebCore::CheckableActionType:
        return {{ type, action, title, enabled, checked }};
        break;
    case WebCore::SubmenuType:
        return {{ action, title, enabled, WTFMove(*submenu) }};
        break;
    }
    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

Vector<WebContextMenuItemData> kitItems(const Vector<WebCore::ContextMenuItem>& coreItemVector)
{
    Vector<WebContextMenuItemData> result;
    result.reserveCapacity(coreItemVector.size());
    for (unsigned i = 0; i < coreItemVector.size(); ++i)
        result.append(WebContextMenuItemData(coreItemVector[i]));
    
    return result;
}

Vector<ContextMenuItem> coreItems(const Vector<WebContextMenuItemData>& kitItemVector)
{
    Vector<ContextMenuItem> result;
    result.reserveCapacity(kitItemVector.size());
    for (unsigned i = 0; i < kitItemVector.size(); ++i)
        result.append(kitItemVector[i].core());
    
    return result;
}

} // namespace WebKit
#endif // ENABLE(CONTEXT_MENUS)
