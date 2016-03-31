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

bool JSHTMLCollection::nameGetter(ExecState* exec, PropertyName propertyName, JSValue& value)
{
    ASSERT_WITH_MESSAGE(wrapped().type() != FormControls, "Should call the subclass' nameGetter");
    ASSERT_WITH_MESSAGE(wrapped().type() != SelectOptions, "Should call the subclass' nameGetter");

    auto item = wrapped().namedItem(propertyNameToAtomicString(propertyName));
    if (!item)
        return false;

    value = toJS(exec, globalObject(), item);
    return true;
}

JSValue toJS(ExecState*, JSDOMGlobalObject* globalObject, HTMLCollection* collection)
{
    if (!collection)
        return jsNull();

    JSObject* wrapper = getCachedWrapper(globalObject->world(), collection);

    if (wrapper)
        return wrapper;

    switch (collection->type()) {
    case FormControls:
        return CREATE_DOM_WRAPPER(globalObject, HTMLFormControlsCollection, collection);
    case SelectOptions:
        return CREATE_DOM_WRAPPER(globalObject, HTMLOptionsCollection, collection);
    case DocAll:
        return CREATE_DOM_WRAPPER(globalObject, HTMLAllCollection, collection);
    default:
        break;
    }

    return CREATE_DOM_WRAPPER(globalObject, HTMLCollection, collection);
}

} // namespace WebCore
