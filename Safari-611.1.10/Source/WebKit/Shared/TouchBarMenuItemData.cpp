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

#if HAVE(TOUCH_BAR)

#include "TouchBarMenuItemData.h"

#include "Decoder.h"
#include "Encoder.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/HTMLMenuItemElement.h>
#include <WebCore/HTMLNames.h>

namespace WebKit {

static ItemType itemType(const String&)
{
    return ItemType::Button;
}

TouchBarMenuItemData::TouchBarMenuItemData(const WebCore::HTMLMenuItemElement& element)
{
    type = itemType(element.attributeWithoutSynchronization(WebCore::HTMLNames::typeAttr));
    identifier = element.attributeWithoutSynchronization(WebCore::HTMLNames::idAttr);
    priority = element.attributeWithoutSynchronization(WebCore::HTMLNames::valueAttr).toFloat();
}

void TouchBarMenuItemData::encode(IPC::Encoder& encoder) const
{
    encoder << type;
    
    encoder << identifier;
    encoder << priority;
}

Optional<TouchBarMenuItemData> TouchBarMenuItemData::decode(IPC::Decoder& decoder)
{
    TouchBarMenuItemData result;
    if (!decoder.decode(result.type))
        return WTF::nullopt;
    
    if (!decoder.decode(result.identifier))
        return WTF::nullopt;
    
    if (!decoder.decode(result.priority))
        return WTF::nullopt;
    
    return makeOptional(WTFMove(result));
}

}

#endif // HAVE(TOUCH_BAR)
