/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSHTMLFrameSetElement.h"

#include "Document.h"
#include "HTMLCollection.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLNames.h"
#include "JSDOMWindow.h"
#include "JSDOMWindowShell.h"
#include "JSDOMBinding.h"

using namespace JSC;

namespace WebCore {

using namespace HTMLNames;

bool JSHTMLFrameSetElement::canGetItemsForName(ExecState*, HTMLFrameSetElement* frameSet, const Identifier& propertyName)
{
    Node* frame = frameSet->children()->namedItem(propertyName);
    return frame && frame->hasTagName(frameTag);
}

JSValue JSHTMLFrameSetElement::nameGetter(ExecState*, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(asObject(slot.slotBase()));
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());

    Node* frame = element->children()->namedItem(propertyName);
    if (Document* doc = static_cast<HTMLFrameElement*>(frame)->contentDocument()) {
        if (JSDOMWindowShell* window = toJSDOMWindowShell(doc->frame()))
            return window;
    }

    return jsUndefined();
}

} // namespace WebCore
