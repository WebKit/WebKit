/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSHTMLAllCollection.h"

#include "AtomicString.h"
#include "HTMLAllCollection.h"
#include "JSDOMBinding.h"
#include "JSHTMLAllCollection.h"
#include "JSHTMLCollectionFunctions.h"
#include "JSNode.h"
#include "JSNodeList.h"
#include "Node.h"

using namespace JSC;

namespace WebCore {

static JSValue JSC_HOST_CALL callHTMLAllCollection(ExecState* exec, JSObject* function, JSValue, const ArgList& args)
{
    return callHTMLCollectionGeneric<HTMLAllCollection, JSHTMLAllCollection>(exec, function, args);
}

CallType JSHTMLAllCollection::getCallData(CallData& callData)
{
    callData.native.function = callHTMLAllCollection;
    return CallTypeHost;
}

bool JSHTMLAllCollection::canGetItemsForName(ExecState*, HTMLAllCollection* collection, const Identifier& propertyName)
{
    Vector<RefPtr<Node> > namedItems;
    collection->namedItems(propertyName, namedItems);
    return !namedItems.isEmpty();
}

JSValue JSHTMLAllCollection::nameGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLAllCollection* thisObj = static_cast<JSHTMLAllCollection*>(asObject(slot.slotBase()));
    return getCollectionNamedItems<JSHTMLAllCollection>(exec, thisObj, propertyName);
}

JSValue JSHTMLAllCollection::item(ExecState* exec, const ArgList& args)
{
    return getCollectionItems<JSHTMLAllCollection>(exec, this, args.at(0));
}

JSValue JSHTMLAllCollection::namedItem(ExecState* exec, const ArgList& args)
{
    return getCollectionNamedItems<JSHTMLAllCollection>(exec, this, Identifier(exec, args.at(0).toString(exec)));
}

} // namespace WebCore
