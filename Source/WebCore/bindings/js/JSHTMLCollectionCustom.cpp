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

#include "HTMLCollection.h"
#include "HTMLOptionsCollection.h"
#include "HTMLAllCollection.h"
#include "JSDOMBinding.h"
#include "JSHTMLAllCollection.h"
#include "JSHTMLOptionsCollection.h"
#include "JSNode.h"
#include "JSNodeList.h"
#include "Node.h"
#include "StaticNodeList.h"
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

#if ENABLE(MICRODATA)
#include "HTMLPropertiesCollection.h"
#include "JSHTMLPropertiesCollection.h"
#endif

using namespace JSC;

namespace WebCore {

static JSValue getNamedItems(ExecState* exec, JSHTMLCollection* collection, const Identifier& propertyName)
{
    Vector<RefPtr<Node> > namedItems;
    collection->impl()->namedItems(identifierToAtomicString(propertyName), namedItems);

    if (namedItems.isEmpty())
        return jsUndefined();
    if (namedItems.size() == 1)
        return toJS(exec, collection->globalObject(), namedItems[0].get());

    // FIXME: HTML5 specifies that this should be a DynamicNodeList.
    // FIXME: HTML5 specifies that non-HTMLOptionsCollection collections should return
    // the first matching item instead of a NodeList.
    return toJS(exec, collection->globalObject(), StaticNodeList::adopt(namedItems).get());
}

bool JSHTMLCollection::canGetItemsForName(ExecState*, HTMLCollection* collection, const Identifier& propertyName)
{
    return collection->hasNamedItem(identifierToAtomicString(propertyName));
}

JSValue JSHTMLCollection::nameGetter(ExecState* exec, JSValue slotBase, const Identifier& propertyName)
{
    JSHTMLCollection* thisObj = static_cast<JSHTMLCollection*>(asObject(slotBase));
    return getNamedItems(exec, thisObj, propertyName);
}

JSValue JSHTMLCollection::item(ExecState* exec)
{
    bool ok;
    uint32_t index = Identifier::toUInt32(exec->argument(0).toString(exec)->value(exec), ok);
    if (ok)
        return toJS(exec, globalObject(), impl()->item(index));
    return getNamedItems(exec, this, Identifier(exec, exec->argument(0).toString(exec)->value(exec)));
}

JSValue JSHTMLCollection::namedItem(ExecState* exec)
{
    return getNamedItems(exec, this, Identifier(exec, exec->argument(0).toString(exec)->value(exec)));
}

JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, HTMLCollection* collection)
{
    if (!collection)
        return jsNull();

    JSDOMWrapper* wrapper = getCachedWrapper(currentWorld(exec), collection);

    if (wrapper)
        return wrapper;

    switch (collection->type()) {
        case SelectOptions:
            wrapper = CREATE_DOM_WRAPPER(exec, globalObject, HTMLOptionsCollection, collection);
            break;
        case DocAll:
            wrapper = CREATE_DOM_WRAPPER(exec, globalObject, HTMLAllCollection, collection);
            break;
#if ENABLE(MICRODATA)
        case ItemProperties:
            wrapper = CREATE_DOM_WRAPPER(exec, globalObject, HTMLPropertiesCollection, collection);
            break;
#endif
        default:
            wrapper = CREATE_DOM_WRAPPER(exec, globalObject, HTMLCollection, collection);
            break;
    }

    return wrapper;
}

} // namespace WebCore
