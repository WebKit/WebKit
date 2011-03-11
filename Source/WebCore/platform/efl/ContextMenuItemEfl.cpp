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
ContextMenuItem::ContextMenuItem(ContextMenuItemType, ContextMenuAction, const String&, ContextMenu*)
{
    notImplemented();
}

ContextMenuItem::~ContextMenuItem()
{
    notImplemented();
}

ContextMenuItemType ContextMenuItem::type() const
{
    notImplemented();
    return ActionType;
}

void ContextMenuItem::setAction(ContextMenuAction)
{
    notImplemented();
}

ContextMenuAction ContextMenuItem::action() const
{
    notImplemented();
    return ContextMenuItemTagNoAction;
}

void ContextMenuItem::setChecked(bool)
{
    notImplemented();
}

bool ContextMenuItem::checked() const
{
    notImplemented();
    return 0;
}

void ContextMenuItem::setEnabled(bool)
{
    notImplemented();
}

bool ContextMenuItem::enabled() const
{
    notImplemented();
    return false;
}

void ContextMenuItem::setSubMenu(ContextMenu*)
{
    notImplemented();
}
#endif
}
