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

#include "MutableArray.h"
#include "WebContextMenuItem.h"
#include "WebContextMenuItemData.h"
#include "WKAPICast.h"
#include "WKContextMenuItemTypes.h"

using namespace WebCore;
using namespace WebKit;

WKTypeID WKContextMenuItemGetTypeID()
{
    return toAPI(WebContextMenuItem::APIType);
}

WKContextMenuItemRef WKContextMenuItemCreateAsAction(WKContextMenuItemTag tag, WKStringRef title, bool enabled)
{
    return toAPI(WebContextMenuItem::create(WebContextMenuItemData(ActionType, toImpl(tag), toImpl(title)->string(), enabled, false)).leakRef());
}

WKContextMenuItemRef WKContextMenuItemCreateAsCheckableAction(WKContextMenuItemTag tag, WKStringRef title, bool enabled, bool checked)
{
    return toAPI(WebContextMenuItem::create(WebContextMenuItemData(CheckableActionType, toImpl(tag), toImpl(title)->string(), enabled, checked)).leakRef());
}

WKContextMenuItemRef WKContextMenuItemCreateAsSubmenu(WKStringRef title, bool enabled, WKArrayRef submenuItems)
{
    return toAPI(WebContextMenuItem::create(toImpl(title)->string(), enabled, toImpl(submenuItems)).leakRef());
}

WKContextMenuItemRef WKContextMenuItemSeparatorItem()
{
    return toAPI(WebContextMenuItem::separatorItem());
}

WKContextMenuItemTag WKContextMenuItemGetTag(WKContextMenuItemRef itemRef)
{
    return toAPI(toImpl(itemRef)->data()->action());
}

WKContextMenuItemType WKContextMenuItemGetType(WKContextMenuItemRef itemRef)
{
    return toAPI(toImpl(itemRef)->data()->type());
}

WKStringRef WKContextMenuItemCopyTitle(WKContextMenuItemRef itemRef)
{
    return toCopiedAPI(toImpl(itemRef)->data()->title().impl());
}

bool WKContextMenuItemGetEnabled(WKContextMenuItemRef itemRef)
{
    return toImpl(itemRef)->data()->enabled();
}

bool WKContextMenuItemGetChecked(WKContextMenuItemRef itemRef)
{
    return toImpl(itemRef)->data()->checked();
}

WKArrayRef WKContextMenuCopySubmenuItems(WKContextMenuItemRef itemRef)
{
    return toAPI(toImpl(itemRef)->submenuItemsAsImmutableArray().leakRef());
}

WKTypeRef WKContextMenuItemGetUserData(WKContextMenuItemRef itemRef)
{
    return toAPI(toImpl(itemRef)->userData());
}

void WKContextMenuItemSetUserData(WKContextMenuItemRef itemRef, WKTypeRef userDataRef)
{
    toImpl(itemRef)->setUserData(toImpl(userDataRef));
}
