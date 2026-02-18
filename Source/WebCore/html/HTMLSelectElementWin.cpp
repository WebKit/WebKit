/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "HTMLSelectElement.h"

#if OS(WINDOWS)

#include "Element.h"
#include "KeyboardEvent.h"
#include "WindowsKeyboardCodes.h"

namespace WebCore {

bool HTMLSelectElement::platformHandleKeydownEvent(KeyboardEvent* event)
{
    // Allow (Shift) F4 and (Ctrl/Shift) Alt/AltGr + Up/Down arrow to pop the menu, matching Firefox.
    switch (event->keyCode()) {
    case VK_F4:
        if (event->altKey() || event->ctrlKey())
            return false;
        break;
    case VK_DOWN:
    case VK_UP:
        if (!event->altKey())
            return false;
        break;
    default:
        return false;
    }

    // Save the selection so it can be compared to the new selection when dispatching change events during setSelectedIndex,
    // which gets called from RenderMenuList::valueChanged, which gets called after the user makes a selection from the menu.
    saveLastSelection();
    showPopup();

    int index = selectedIndex();
    ASSERT(index >= 0);
    ASSERT_WITH_SECURITY_IMPLICATION(index < static_cast<int>(listItems().size()));
    setSelectedIndex(index);
    event->setDefaultHandled();
    return true;
}

}

#endif
