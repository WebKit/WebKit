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

#include "HTMLAllCollection.h"
#include "HTMLCollection.h"
#include "HTMLFormControlsCollection.h"
#include "HTMLOptionsCollection.h"
#include "JSDOMBinding.h"
#include "JSHTMLAllCollection.h"
#include "JSHTMLFormControlsCollection.h"
#include "JSHTMLOptionsCollection.h"
#include "JSNode.h"
#include "JSNodeList.h"
#include "JSRadioNodeList.h"
#include "Node.h"
#include "RadioNodeList.h"
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

using namespace JSC;

namespace WebCore {

bool JSHTMLCollection::canGetItemsForName(ExecState*, HTMLCollection* collection, PropertyName propertyName)
{
    return collection->hasNamedItem(propertyNameToAtomicString(propertyName));
}

EncodedJSValue JSHTMLCollection::nameGetter(ExecState* exec, JSObject* slotBase, EncodedJSValue, PropertyName propertyName)
{
    JSHTMLCollection* collection = jsCast<JSHTMLCollection*>(slotBase);
    const AtomicString& name = propertyNameToAtomicString(propertyName);
    return JSValue::encode(toJS(exec, collection->globalObject(), collection->impl().namedItem(name)));
}

JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, HTMLCollection* collection)
{
    if (!collection)
        return jsNull();

    JSObject* wrapper = getCachedWrapper(currentWorld(exec), collection);

    if (wrapper)
        return wrapper;

    switch (collection->type()) {
    case FormControls:
        return CREATE_DOM_WRAPPER(exec, globalObject, HTMLFormControlsCollection, collection);
    case SelectOptions:
        return CREATE_DOM_WRAPPER(exec, globalObject, HTMLOptionsCollection, collection);
    case DocAll:
        return CREATE_DOM_WRAPPER(exec, globalObject, HTMLAllCollection, collection);
    default:
        break;
    }

    return CREATE_DOM_WRAPPER(exec, globalObject, HTMLCollection, collection);
}

} // namespace WebCore
