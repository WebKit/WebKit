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

#ifndef JSHTMLCollectionFunctions_h
#define JSHTMLCollectionFunctions_h

#include "JSNode.h"
#include "JSNodeList.h"
#include "Node.h"
#include "PlatformString.h"
#include "StaticNodeList.h"
#include <wtf/Vector.h>

namespace WebCore {

template<typename JSCollection>
JSC::JSValue getCollectionNamedItems(JSC::ExecState* exec, JSCollection* jsCollection, const JSC::Identifier& propertyName)
{
    Vector<RefPtr<Node> > namedItems;
    jsCollection->impl()->namedItems(propertyName, namedItems);

    if (namedItems.isEmpty())
        return JSC::jsUndefined();
    if (namedItems.size() == 1)
        return toJS(exec, jsCollection->globalObject(), namedItems[0].get());

    // FIXME: HTML5 specifies that this should be a DynamicNodeList.
    // FIXME: HTML5 specifies that non-HTMLOptionsCollection collections should return
    // the first matching item instead of a NodeList.
    return toJS(exec, jsCollection->globalObject(), StaticNodeList::adopt(namedItems).get());
}

template<typename JSCollection>
JSC::JSValue getCollectionItems(JSC::ExecState* exec, JSCollection* jsCollection, JSC::JSValue value)
{
    bool ok;
    JSC::UString string = value.toString(exec);
    unsigned index = string.toUInt32(&ok, false);
    if (ok)
        return toJS(exec, jsCollection->globalObject(), jsCollection->impl()->item(index));
    return getCollectionNamedItems<JSCollection>(exec, jsCollection, JSC::Identifier(exec, string));
}

template<typename Collection, typename JSCollection>
JSC::JSValue callHTMLCollectionGeneric(JSC::ExecState* exec, JSC::JSObject* function, const JSC::ArgList& args)
{
    if (args.size() < 1)
        return JSC::jsUndefined();

    JSCollection* jsCollection = static_cast<JSCollection*>(function);
    Collection* collection = jsCollection->impl();

    // Do we need the TypeError test here?

    if (args.size() == 1)
        return getCollectionItems<JSCollection>(exec, jsCollection, args.at(0));

    // The second arg, if set, is the index of the item we want
    bool ok;
    JSC::UString string = args.at(0).toString(exec);
    unsigned index = args.at(1).toString(exec).toUInt32(&ok, false);
    if (ok) {
        String pstr = string;
        Node* node = collection->namedItem(pstr);
        while (node) {
            if (!index)
                return toJS(exec, jsCollection->globalObject(), node);
            node = collection->nextNamedItem(pstr);
            --index;
        }
    }

    return JSC::jsUndefined();
}

} // namespace WebCore

#endif // JSHTMLCollectionFunctions_h
