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

#include "CustomElementReactionQueue.h"
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

    value = toJS(exec, globalObject(), *item);
    return true;
}

void JSHTMLOptionsCollection::setLength(ExecState& state, JSValue value)
{
    CustomElementReactionStack customElementReactionStack;
    VM& vm = state.vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    double number = value.toNumber(&state);
    RETURN_IF_EXCEPTION(throwScope, void());
    unsigned length;
    if (!std::isfinite(number))
        length = 0;
    else if (number < 0)
        return setDOMException(&state, throwScope, INDEX_SIZE_ERR);
    else
        length = static_cast<unsigned>(std::min<double>(number, UINT_MAX));
    propagateException(state, throwScope, wrapped().setLength(length));
}

void JSHTMLOptionsCollection::indexSetter(ExecState* state, unsigned index, JSValue value)
{
    CustomElementReactionStack customElementReactionStack;
    selectElementIndexSetter(*state, wrapped().selectElement(), index, value);
}

}
