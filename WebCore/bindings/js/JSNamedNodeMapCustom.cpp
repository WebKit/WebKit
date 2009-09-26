/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "JSNamedNodeMap.h"

#include "JSNode.h"

#include "Element.h"
#include "NamedNodeMap.h"

using namespace JSC;

namespace WebCore {

bool JSNamedNodeMap::canGetItemsForName(ExecState*, NamedNodeMap* impl, const Identifier& propertyName)
{
    return impl->getNamedItem(propertyName);
}

JSValue JSNamedNodeMap::nameGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSNamedNodeMap* thisObj = static_cast<JSNamedNodeMap*>(asObject(slot.slotBase()));
    return toJS(exec, thisObj->impl()->getNamedItem(propertyName));
}

void JSNamedNodeMap::markChildren(MarkStack& markStack)
{
    Base::markChildren(markStack);

    // Mark the element so that this will work to access the attribute even if the last
    // other reference goes away.
    if (Element* element = impl()->element()) {
        if (JSNode* wrapper = getCachedDOMNodeWrapper(element->document(), element))
            markStack.append(wrapper);
    }
}

} // namespace WebCore
