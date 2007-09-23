/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Holger Hans Peter Freyther
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ContextMenuItem.h"

#include "NotImplemented.h"

namespace WebCore {

ContextMenuItem::ContextMenuItem(PlatformMenuItemDescription)
{
    notImplemented();
}

ContextMenuItem::ContextMenuItem(ContextMenu*)
{
    notImplemented();
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType, ContextMenuAction, const String&, ContextMenu*)
{
    notImplemented();
}

ContextMenuItem::~ContextMenuItem()
{
    notImplemented();
}

PlatformMenuItemDescription ContextMenuItem::releasePlatformDescription()
{
    notImplemented();
    return m_platformDescription;
}

ContextMenuItemType ContextMenuItem::type() const
{
    notImplemented();
    return ActionType; 
}

void ContextMenuItem::setType(ContextMenuItemType)
{
    notImplemented();
}

ContextMenuAction ContextMenuItem::action() const
{
    notImplemented();
    return ContextMenuItemTagNoAction;
}

void ContextMenuItem::setAction(ContextMenuAction)
{
    notImplemented();
}

String ContextMenuItem::title() const
{
    notImplemented();
    return String();
}

void ContextMenuItem::setTitle(const String&)
{
    notImplemented();
}

PlatformMenuDescription ContextMenuItem::platformSubMenu() const
{
    notImplemented();
    return 0;
}

void ContextMenuItem::setSubMenu(ContextMenu*)
{
    notImplemented();
}

void ContextMenuItem::setChecked(bool)
{
    notImplemented();
}

void ContextMenuItem::setEnabled(bool)
{
    notImplemented();
}

}
