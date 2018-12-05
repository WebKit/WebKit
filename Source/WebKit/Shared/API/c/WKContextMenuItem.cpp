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
#include "WKContextMenuItem.h"

#include "APIArray.h"
#include "WebContextMenuItem.h"
#include "WebContextMenuItemData.h"
#include "WKAPICast.h"
#include "WKContextMenuItemTypes.h"

WKTypeID WKContextMenuItemGetTypeID()
{
#if ENABLE(CONTEXT_MENUS)
    return WebKit::toAPI(WebKit::WebContextMenuItem::APIType);
#else
    return WebKit::toAPI(API::Object::Type::Null);
#endif
}

WKContextMenuItemRef WKContextMenuItemCreateAsAction(WKContextMenuItemTag tag, WKStringRef title, bool enabled)
{
#if ENABLE(CONTEXT_MENUS)
    return WebKit::toAPI(&WebKit::WebContextMenuItem::create(WebKit::WebContextMenuItemData(WebCore::ActionType, WebKit::toImpl(tag), WebKit::toImpl(title)->string(), enabled, false)).leakRef());
#else
    UNUSED_PARAM(tag);
    UNUSED_PARAM(title);
    UNUSED_PARAM(enabled);
    return nullptr;
#endif
}

WKContextMenuItemRef WKContextMenuItemCreateAsCheckableAction(WKContextMenuItemTag tag, WKStringRef title, bool enabled, bool checked)
{
#if ENABLE(CONTEXT_MENUS)
    return WebKit::toAPI(&WebKit::WebContextMenuItem::create(WebKit::WebContextMenuItemData(WebCore::CheckableActionType, WebKit::toImpl(tag), WebKit::toImpl(title)->string(), enabled, checked)).leakRef());
#else
    UNUSED_PARAM(tag);
    UNUSED_PARAM(title);
    UNUSED_PARAM(enabled);
    UNUSED_PARAM(checked);
    return nullptr;
#endif
}

WKContextMenuItemRef WKContextMenuItemCreateAsSubmenu(WKStringRef title, bool enabled, WKArrayRef submenuItems)
{
#if ENABLE(CONTEXT_MENUS)
    return WebKit::toAPI(&WebKit::WebContextMenuItem::create(WebKit::toImpl(title)->string(), enabled, WebKit::toImpl(submenuItems)).leakRef());
#else
    UNUSED_PARAM(title);
    UNUSED_PARAM(enabled);
    UNUSED_PARAM(submenuItems);
    return nullptr;
#endif
}

WKContextMenuItemRef WKContextMenuItemSeparatorItem()
{
#if ENABLE(CONTEXT_MENUS)
    return WebKit::toAPI(WebKit::WebContextMenuItem::separatorItem());
#else
    return nullptr;
#endif
}

WKContextMenuItemTag WKContextMenuItemGetTag(WKContextMenuItemRef itemRef)
{
#if ENABLE(CONTEXT_MENUS)
    return WebKit::toAPI(WebKit::toImpl(itemRef)->data().action());
#else
    UNUSED_PARAM(itemRef);
    return WebKit::toAPI(WebCore::ContextMenuItemTagNoAction);
#endif
}

WKContextMenuItemType WKContextMenuItemGetType(WKContextMenuItemRef itemRef)
{
#if ENABLE(CONTEXT_MENUS)
    return WebKit::toAPI(WebKit::toImpl(itemRef)->data().type());
#else
    UNUSED_PARAM(itemRef);
    return WebKit::toAPI(WebCore::ActionType);
#endif
}

WKStringRef WKContextMenuItemCopyTitle(WKContextMenuItemRef itemRef)
{
#if ENABLE(CONTEXT_MENUS)
    return WebKit::toCopiedAPI(WebKit::toImpl(itemRef)->data().title().impl());
#else
    UNUSED_PARAM(itemRef);
    return 0;
#endif
}

bool WKContextMenuItemGetEnabled(WKContextMenuItemRef itemRef)
{
#if ENABLE(CONTEXT_MENUS)
    return WebKit::toImpl(itemRef)->data().enabled();
#else
    UNUSED_PARAM(itemRef);
    return false;
#endif
}

bool WKContextMenuItemGetChecked(WKContextMenuItemRef itemRef)
{
#if ENABLE(CONTEXT_MENUS)
    return WebKit::toImpl(itemRef)->data().checked();
#else
    UNUSED_PARAM(itemRef);
    return false;
#endif
}

WKArrayRef WKContextMenuCopySubmenuItems(WKContextMenuItemRef itemRef)
{
#if ENABLE(CONTEXT_MENUS)
    return WebKit::toAPI(&WebKit::toImpl(itemRef)->submenuItemsAsAPIArray().leakRef());
#else
    UNUSED_PARAM(itemRef);
    return 0;
#endif
}

WKTypeRef WKContextMenuItemGetUserData(WKContextMenuItemRef itemRef)
{
#if ENABLE(CONTEXT_MENUS)
    return WebKit::toAPI(WebKit::toImpl(itemRef)->userData());
#else
    UNUSED_PARAM(itemRef);
    return 0;
#endif
}

void WKContextMenuItemSetUserData(WKContextMenuItemRef itemRef, WKTypeRef userDataRef)
{
#if ENABLE(CONTEXT_MENUS)
    WebKit::toImpl(itemRef)->setUserData(WebKit::toImpl(userDataRef));
#else
    UNUSED_PARAM(itemRef);
    UNUSED_PARAM(userDataRef);
#endif
}
