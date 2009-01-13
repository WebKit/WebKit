/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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
#include "ContextMenuItem.h"

namespace WebCore {

// This is a stub implementation of WebKit's ContextMenu class that does
// nothing.

ContextMenuItem::ContextMenuItem(PlatformMenuItemDescription item)
{
}

ContextMenuItem::ContextMenuItem(ContextMenu* subMenu)
{
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* subMenu)
{
}

ContextMenuItem::~ContextMenuItem()
{
}

PlatformMenuItemDescription ContextMenuItem::releasePlatformDescription()
{
    return PlatformMenuItemDescription();
}

ContextMenuItemType ContextMenuItem::type() const
{
    return ContextMenuItemType();
}

ContextMenuAction ContextMenuItem::action() const
{ 
    return ContextMenuAction();
}

String ContextMenuItem::title() const 
{
    return String();
}

PlatformMenuDescription ContextMenuItem::platformSubMenu() const
{
    return PlatformMenuDescription();
}

void ContextMenuItem::setType(ContextMenuItemType type)
{
}

void ContextMenuItem::setAction(ContextMenuAction action)
{
}

void ContextMenuItem::setTitle(const String& title)
{
}

void ContextMenuItem::setSubMenu(ContextMenu* subMenu)
{
}

void ContextMenuItem::setChecked(bool checked)
{
}

void ContextMenuItem::setEnabled(bool enabled)
{
}

bool ContextMenuItem::enabled() const
{
    return false;
}

} // namespace WebCore
