/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "JSHTMLCollection.h"

#include "AtomicString.h"
#include "HTMLAllCollection.h"
#include "HTMLCollection.h"
#include "HTMLOptionsCollection.h"
#include "JSDOMBinding.h"
#include "JSHTMLAllCollection.h"
#include "JSHTMLCollectionFunctions.h"
#include "JSHTMLOptionsCollection.h"
#include "JSNode.h"
#include "JSNodeList.h"
#include "Node.h"
#include "StaticNodeList.h"
#include <wtf/Vector.h>

using namespace JSC;

namespace WebCore {

static JSValue JSC_HOST_CALL callHTMLCollection(ExecState* exec, JSObject* function, JSValue, const ArgList& args)
{
    return callHTMLCollectionGeneric<HTMLCollection, JSHTMLCollection>(exec, function, args);
}

CallType JSHTMLCollection::getCallData(CallData& callData)
{
    callData.native.function = callHTMLCollection;
    return CallTypeHost;
}

bool JSHTMLCollection::canGetItemsForName(ExecState*, HTMLCollection* collection, const Identifier& propertyName)
{
    Vector<RefPtr<Node> > namedItems;
    collection->namedItems(propertyName, namedItems);
    return !namedItems.isEmpty();
}

JSValue JSHTMLCollection::nameGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLCollection* thisObj = static_cast<JSHTMLCollection*>(asObject(slot.slotBase()));
    return getCollectionNamedItems<JSHTMLCollection>(exec, thisObj, propertyName);
}

JSValue JSHTMLCollection::item(ExecState* exec, const ArgList& args)
{
    return getCollectionItems<JSHTMLCollection>(exec, this, args.at(0));
}

JSValue JSHTMLCollection::namedItem(ExecState* exec, const ArgList& args)
{
    return getCollectionNamedItems<JSHTMLCollection>(exec, this, Identifier(exec, args.at(0).toString(exec)));
}

} // namespace WebCore
