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

#include <wtf/MathExtras.h>

using namespace KJS;

namespace WebCore {

JSValue* JSHTMLOptionsCollection::length(ExecState* exec) const
{
    HTMLOptionsCollection* imp = static_cast<HTMLOptionsCollection*>(impl());
    return jsNumber(imp->length());
}

void JSHTMLOptionsCollection::setLength(ExecState* exec, JSValue* value)
{
    HTMLOptionsCollection* imp = static_cast<HTMLOptionsCollection*>(impl());
    ExceptionCode ec = 0;
    unsigned newLength = 0;
    double lengthValue = value->toNumber(exec);
    if (!isnan(lengthValue) && !isinf(lengthValue)) {
        if (lengthValue < 0.0)
            ec = INDEX_SIZE_ERR;
        else if (lengthValue > static_cast<double>(UINT_MAX))
            newLength = UINT_MAX;
        else
            newLength = static_cast<unsigned>(lengthValue);
    }
    if (!ec)
        imp->setLength(newLength, ec);
    setDOMException(exec, ec);
}

void JSHTMLOptionsCollection::indexSetter(ExecState* exec, unsigned index, JSValue* value, int attr)
{
    HTMLOptionsCollection* imp = static_cast<HTMLOptionsCollection*>(impl());
    HTMLSelectElement* base = static_cast<HTMLSelectElement*>(imp->base());
    selectIndexSetter(base, exec, index, value);
}

JSValue* JSHTMLOptionsCollection::add(ExecState* exec, const List& args)
{
    HTMLOptionsCollection* imp = static_cast<HTMLOptionsCollection*>(impl());
    HTMLOptionElement* option = toHTMLOptionElement(args[0]);
    ExceptionCode ec = 0;
    if (args.size() < 2)
        imp->add(option, ec);
    else {
        bool ok;
        int index = args[1]->toInt32(exec, ok);
        if (exec->hadException())
            return jsUndefined();
        if (!ok)
            ec = TYPE_MISMATCH_ERR;
        else
            imp->add(option, index, ec);
    }
    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue* JSHTMLOptionsCollection::remove(ExecState* exec, const List& args)
{
    HTMLOptionsCollection* imp = static_cast<HTMLOptionsCollection*>(impl());
    JSHTMLSelectElement* base = static_cast<JSHTMLSelectElement*>(toJS(exec, imp->base()));
    return base->remove(exec, args);
}

}
