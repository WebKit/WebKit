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
#include "ContextMenu.h"

namespace WebCore {

// This is a stub implementation of WebKit's ContextMenu class that does
// nothing.

ContextMenu::ContextMenu()
{
}

ContextMenu::ContextMenu(const PlatformMenuDescription menu)
{
}

ContextMenu::~ContextMenu()
{
}

unsigned ContextMenu::itemCount() const
{
    return m_items.size();
}

void ContextMenu::insertItem(unsigned position, ContextMenuItem& item)
{
    m_items.insert(position, item);
}

void ContextMenu::appendItem(ContextMenuItem& item)
{
    m_items.append(item);
}

ContextMenuItem* ContextMenu::itemWithAction(unsigned action)
{
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].action() == static_cast<ContextMenuAction>(action))
            return &m_items[i];
    }
    return 0;
}

ContextMenuItem* ContextMenu::itemAtIndex(unsigned index, const PlatformMenuDescription platformDescription)
{
    return &m_items[index];
}

void ContextMenu::setPlatformDescription(PlatformMenuDescription menu)
{
}

PlatformMenuDescription ContextMenu::platformDescription() const
{
    return 0;
}

PlatformMenuDescription ContextMenu::releasePlatformDescription()
{
    return 0;
}

} // namespace WebCore
