/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
#include "JSNodeList.h"

#include "AtomicString.h"
#include "JSNode.h"
#include "Node.h"
#include "NodeList.h"

namespace WebCore {

// Need to support both get and call, so that list[0] and list(0) work.
KJS::JSValue* JSNodeList::callAsFunction(KJS::ExecState* exec, KJS::JSObject* thisObj, const KJS::List& args)
{
    // Do not use thisObj here. See JSHTMLCollection.
    KJS::UString s = args[0]->toString(exec);
    bool ok;
    unsigned u = s.toUInt32(&ok);
    if (ok)
        return toJS(exec, impl()->item(u));

    return KJS::jsUndefined();
}

bool JSNodeList::implementsCall() const
{
    return true;
}

bool JSNodeList::canGetItemsForName(KJS::ExecState*, NodeList* impl, const KJS::Identifier& propertyName)
{
    return impl->itemWithName(propertyName);
}

KJS::JSValue* JSNodeList::nameGetter(KJS::ExecState* exec, KJS::JSObject* originalObject, const KJS::Identifier& propertyName, const KJS::PropertySlot& slot)
{
    JSNodeList* thisObj = static_cast<JSNodeList*>(slot.slotBase());
    return toJS(exec, thisObj->impl()->itemWithName(propertyName));
}

} // namespace WebCore
