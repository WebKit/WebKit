/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSHTMLOptionsCollection.h"

#include "ExceptionCode.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptionsCollection.h"
#include "HTMLSelectElement.h"
#include "JSHTMLOptionElement.h"
#include "JSHTMLSelectElement.h"
#include "JSHTMLSelectElementCustom.h"
#include "JSNodeList.h"
#include "StaticNodeList.h"

#include <wtf/MathExtras.h>

using namespace JSC;

namespace WebCore {

bool JSHTMLOptionsCollection::nameGetter(ExecState* exec, PropertyName propertyName, JSValue& value)
{
    auto item = wrapped().namedItem(propertyNameToAtomicString(propertyName));
    if (!item)
        return false;

    value = toJS(exec, globalObject(), item);
    return true;
}

void JSHTMLOptionsCollection::setLength(ExecState& state, JSValue value)
{
    ExceptionCode ec = 0;
    unsigned newLength = 0;
    double lengthValue = value.toNumber(&state);
    if (!std::isnan(lengthValue) && !std::isinf(lengthValue)) {
        if (lengthValue < 0.0)
            ec = INDEX_SIZE_ERR;
        else if (lengthValue > static_cast<double>(UINT_MAX))
            newLength = UINT_MAX;
        else
            newLength = static_cast<unsigned>(lengthValue);
    }
    if (!ec)
        wrapped().setLength(newLength, ec);
    setDOMException(&state, ec);
}

void JSHTMLOptionsCollection::indexSetter(ExecState* exec, unsigned index, JSValue value)
{
    selectIndexSetter(&wrapped().selectElement(), exec, index, value);
}

JSValue JSHTMLOptionsCollection::remove(ExecState& state)
{
    // The argument can be an HTMLOptionElement or an index.
    JSValue argument = state.argument(0);
    if (HTMLOptionElement* option = JSHTMLOptionElement::toWrapped(argument))
        wrapped().remove(*option);
    else
        wrapped().remove(argument.toInt32(&state));
    return jsUndefined();
}

}
