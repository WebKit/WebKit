/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2010 Company 100, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ContextMenuItem.h"

#include "ContextMenu.h"
#include "NotImplemented.h"

#include <wtf/text/CString.h>

namespace WebCore {

ContextMenuItem::ContextMenuItem(PlatformMenuDescription item)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

ContextMenuItem::ContextMenuItem(ContextMenu* subMenu)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* subMenu)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

ContextMenuItem::~ContextMenuItem()
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

PlatformMenuDescription ContextMenuItem::releasePlatformDescription()
{
    ASSERT_NOT_REACHED();
    notImplemented();
    return 0;
}

ContextMenuItemType ContextMenuItem::type() const
{
    ASSERT_NOT_REACHED();
    notImplemented();
    return ActionType;
}

ContextMenuAction ContextMenuItem::action() const
{ 
    ASSERT_NOT_REACHED();
    notImplemented();
    return ContextMenuItemTagNoAction;
}

String ContextMenuItem::title() const 
{
    ASSERT_NOT_REACHED();
    notImplemented();
    return String();
}

PlatformMenuDescription ContextMenuItem::platformSubMenu() const
{
    ASSERT_NOT_REACHED();
    notImplemented();
    return 0;
}

void ContextMenuItem::setType(ContextMenuItemType type)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

void ContextMenuItem::setAction(ContextMenuAction action)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

void ContextMenuItem::setTitle(const String& title)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

void ContextMenuItem::setSubMenu(ContextMenu* subMenu)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

void ContextMenuItem::setChecked(bool checked)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

void ContextMenuItem::setEnabled(bool enabled)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

bool ContextMenuItem::enabled() const
{
    ASSERT_NOT_REACHED();
    notImplemented();
    return false;
}

}
