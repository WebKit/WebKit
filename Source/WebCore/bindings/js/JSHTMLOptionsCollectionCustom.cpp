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

void JSHTMLOptionsCollection::setLength(ExecState* exec, JSValue value)
{
    ExceptionCode ec = 0;
    unsigned newLength = 0;
    double lengthValue = value.toNumber(exec);
    if (!std::isnan(lengthValue) && !std::isinf(lengthValue)) {
        if (lengthValue < 0.0)
            ec = INDEX_SIZE_ERR;
        else if (lengthValue > static_cast<double>(UINT_MAX))
            newLength = UINT_MAX;
        else
            newLength = static_cast<unsigned>(lengthValue);
    }
    if (!ec)
        impl().setLength(newLength, ec);
    setDOMException(exec, ec);
}

void JSHTMLOptionsCollection::indexSetter(ExecState* exec, unsigned index, JSValue value)
{
    selectIndexSetter(&impl().selectElement(), exec, index, value);
}

JSValue JSHTMLOptionsCollection::add(ExecState* exec)
{
    HTMLOptionsCollection& imp = impl();
    HTMLOptionElement* option = toHTMLOptionElement(exec->argument(0));
    ExceptionCode ec = 0;
    if (exec->argumentCount() < 2)
        imp.add(option, ec);
    else {
        int index = exec->argument(1).toInt32(exec);
        if (exec->hadException())
            return jsUndefined();
        imp.add(option, index, ec);
    }
    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue JSHTMLOptionsCollection::remove(ExecState* exec)
{
    // The argument can be an HTMLOptionElement or an index.
    JSValue argument = exec->argument(0);
    if (HTMLOptionElement* option = toHTMLOptionElement(argument))
        impl().remove(option);
    else
        impl().remove(argument.toInt32(exec));
    return jsUndefined();
}

}
