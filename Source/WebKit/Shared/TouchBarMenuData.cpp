/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "TouchBarMenuData.h"

#include "Decoder.h"
#include "Encoder.h"
#include "TouchBarMenuItemData.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/HTMLMenuElement.h>
#include <WebCore/HTMLNames.h>

namespace WebKit {
    
TouchBarMenuData::TouchBarMenuData(const WebCore::HTMLMenuElement& element)
{
    if (!element.isTouchBarMenu())
        return;

    // FIXME: We can't rely on using the 'id' attribute of the element here to distinguish
    // between different menu items. For instance, a menuitem may have the same 'id' as
    // another, or have no 'id' at all.
    m_id = element.attributeWithoutSynchronization(WebCore::HTMLNames::idAttr);
    m_isPageCustomized = true;
}
    
void TouchBarMenuData::addMenuItem(const TouchBarMenuItemData& data)
{
    m_items.append(data);
}
    
void TouchBarMenuData::removeMenuItem(const TouchBarMenuItemData& data)
{
    m_items.removeFirst(data);
}
    
void TouchBarMenuData::encode(IPC::Encoder& encoder) const
{
    encoder << m_items;
}

bool TouchBarMenuData::decode(IPC::Decoder& decoder, TouchBarMenuData& data)
{
    return decoder.decode(data.m_items);
}
    
}
