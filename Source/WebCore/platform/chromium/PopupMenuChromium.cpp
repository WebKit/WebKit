/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
#include "PopupMenuChromium.h"

#include "Frame.h"
#include "FrameView.h"
#include "Page.h"
#include "PopupContainer.h"
#include "Settings.h"

namespace WebCore {

int PopupMenuChromium::s_minimumRowHeight = 0;
int PopupMenuChromium::s_optionPaddingForTouch = 30;

// The settings used for the drop down menu.
// This is the delegate used if none is provided.
static const PopupContainerSettings dropDownSettings = {
    true, // setTextOnIndexChange
    true, // acceptOnAbandon
    false, // loopSelectionNavigation
    false // restrictWidthOfListBox
};

PopupMenuChromium::PopupMenuChromium(PopupMenuClient* client)
    : m_popupClient(client)
{
}

PopupMenuChromium::~PopupMenuChromium()
{
    // When the PopupMenuChromium is destroyed, the client could already have been
    // deleted.
    if (p.popup)
        p.popup->listBox()->disconnectClient();
    hide();
}

void PopupMenuChromium::show(const IntRect& r, FrameView* v, int index)
{
    if (!p.popup) {
        PopupContainerSettings popupSettings = dropDownSettings;
        popupSettings.defaultDeviceScaleFactor =
            v->frame()->page()->settings()->defaultDeviceScaleFactor();
        if (!popupSettings.defaultDeviceScaleFactor)
            popupSettings.defaultDeviceScaleFactor = 1;
        p.popup = PopupContainer::create(client(), PopupContainer::Select, popupSettings);
    }
    p.popup->showInRect(r, v, index);
}

void PopupMenuChromium::hide()
{
    if (p.popup)
        p.popup->hide();
}

void PopupMenuChromium::updateFromElement()
{
    p.popup->listBox()->updateFromElement();
}


void PopupMenuChromium::disconnectClient()
{
    m_popupClient = 0;
}

} // namespace WebCore
