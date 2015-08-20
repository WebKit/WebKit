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
#include "JSHTMLFormControlsCollection.h"

#include "HTMLFormControlsCollection.h"
#include "JSNode.h"
#include "JSRadioNodeList.h"
#include "RadioNodeList.h"
#include <runtime/IdentifierInlines.h>

using namespace JSC;

namespace WebCore {

static JSValue namedItems(ExecState* exec, JSHTMLFormControlsCollection* collection, PropertyName propertyName)
{
    const AtomicString& name = propertyNameToAtomicString(propertyName);
    Vector<Ref<Element>> namedItems = collection->impl().namedItems(name);

    if (namedItems.isEmpty())
        return jsUndefined();
    if (namedItems.size() == 1)
        return toJS(exec, collection->globalObject(), namedItems[0].ptr());

    ASSERT(collection->impl().type() == FormControls);
    return toJS(exec, collection->globalObject(), collection->impl().ownerNode().radioNodeList(name).get());
}

bool JSHTMLFormControlsCollection::nameGetter(ExecState* exec, PropertyName propertyName, JSValue& value)
{
    auto items = namedItems(exec, this, propertyName);
    if (items.isUndefined())
        return false;

    value = items;
    return true;
}

JSValue JSHTMLFormControlsCollection::namedItem(ExecState* exec)
{
    JSValue value = namedItems(exec, this, Identifier::fromString(exec, exec->argument(0).toString(exec)->value(exec)));
    return value.isUndefined() ? jsNull() : value;
}

} // namespace WebCore
