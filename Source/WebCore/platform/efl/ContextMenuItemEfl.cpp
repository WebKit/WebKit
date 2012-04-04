/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2007 Staikos Computing Services Inc. <info@staikos.net>
 * Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2011 Samsung Electronics
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

#if ENABLE(CONTEXT_MENUS)

#include "ContextMenuItem.h"

#include "NotImplemented.h"

namespace WebCore {

#if USE(CROSS_PLATFORM_CONTEXT_MENUS)
void* ContextMenuItem::nativeMenuItem() const
{
    notImplemented();
    return 0;
}
#else
ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu*)
{
    m_platformDescription.type = type;
    m_platformDescription.action = action;
    m_platformDescription.title = title;
}

ContextMenuItem::ContextMenuItem(ContextMenuAction, const String&, bool enabled, bool checked, Vector<ContextMenuItem>& subMenuItems)
{
}

ContextMenuItem::ContextMenuItem(ContextMenu*)
{
}

ContextMenuItem::ContextMenuItem(PlatformMenuItemDescription)
{
}

ContextMenuItem::ContextMenuItem(WebCore::ContextMenuItemType, WebCore::ContextMenuAction, WTF::String const&, bool, bool)
{
}

ContextMenuItem::~ContextMenuItem()
{
}

void ContextMenuItem::setType(ContextMenuItemType type)
{
    m_platformDescription.type = type;
}

ContextMenuItemType ContextMenuItem::type() const
{
    return m_platformDescription.type;
}

void ContextMenuItem::setAction(ContextMenuAction action)
{
    m_platformDescription.action = action;
}

ContextMenuAction ContextMenuItem::action() const
{
    return m_platformDescription.action;
}

void ContextMenuItem::setTitle(const String& title)
{
    m_platformDescription.title = title;
}

String ContextMenuItem::title() const
{
    return m_platformDescription.title;
}

void ContextMenuItem::setChecked(bool checked)
{
    m_platformDescription.checked = checked;
}

bool ContextMenuItem::checked() const
{
    return m_platformDescription.checked;
}

void ContextMenuItem::setEnabled(bool enabled)
{
    m_platformDescription.enabled = enabled;
}

bool ContextMenuItem::enabled() const
{
    return m_platformDescription.enabled;
}

void ContextMenuItem::setSubMenu(ContextMenu*)
{
    notImplemented();
}

PlatformMenuDescription ContextMenuItem::platformSubMenu() const
{                                                                                       
    return 0;
}  
#endif
}
#endif // ENABLE(CONTEXT_MENUS)
