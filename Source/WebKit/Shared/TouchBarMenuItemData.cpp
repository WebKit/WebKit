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
#include "TouchBarMenuItemData.h"

#include "Decoder.h"
#include "Encoder.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/HTMLElement.h>
#include <WebCore/HTMLMenuItemElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/Node.h>
#include <vector>

namespace WebKit {

ItemType TouchBarMenuItemData::getItemType(String value)
{
    return ItemType::Button;
}

TouchBarMenuItemData::TouchBarMenuItemData()
{
}

TouchBarMenuItemData::TouchBarMenuItemData(WebCore::HTMLMenuItemElement& element)
{
    itemType = getItemType(element.attributeWithoutSynchronization(WebCore::HTMLNames::typeAttr));
    identifier = element.attributeWithoutSynchronization(WebCore::HTMLNames::idAttr);
    commandName = element.attributeWithoutSynchronization(WebCore::HTMLNames::onclickAttr);
    priority = element.attributeWithoutSynchronization(WebCore::HTMLNames::valueAttr).toFloat();
}
    
TouchBarMenuItemData::TouchBarMenuItemData(const TouchBarMenuItemData& other)
    : itemType(other.itemType)
    , identifier(other.identifier)
    , commandName(other.commandName)
    , priority(other.priority)
{
}

void TouchBarMenuItemData::encode(IPC::Encoder& encoder) const
{
    encoder.encodeEnum(itemType);
    
    encoder << identifier;
    encoder << commandName;
    encoder << priority;
}

std::optional<TouchBarMenuItemData> TouchBarMenuItemData::decode(IPC::Decoder& decoder)
{
    TouchBarMenuItemData result;
    if (!decoder.decodeEnum(result.itemType))
        return std::nullopt;
    
    if (!decoder.decode(result.identifier))
        return std::nullopt;
    
    if (!decoder.decode(result.commandName))
        return std::nullopt;
    
    if (!decoder.decode(result.priority))
        return std::nullopt;
    
    return WTFMove(result);
}

}
