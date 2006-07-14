/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "JSHTMLOptionsCollection.h"

#include "ExceptionCode.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptionsCollection.h"
#include "HTMLSelectElement.h"
#include "JSHTMLOptionElement.h"

#include <kjs/operations.h>

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
    double lengthValue = value->getNumber();
    if (!isNaN(lengthValue) && !isInf(lengthValue)) {
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

void JSHTMLOptionsCollection::indexSetter(KJS::ExecState* exec, const KJS::Identifier &propertyName, KJS::JSValue* value, int attr)
{
    bool ok;
    unsigned index = propertyName.toUInt32(&ok);
    if (!ok)
        return;

    HTMLOptionsCollection* imp = static_cast<HTMLOptionsCollection*>(impl());
    HTMLSelectElement* base = static_cast<HTMLSelectElement*>(imp->base());
    if (value->isUndefinedOrNull())
        base->remove(index);
    else {
        ExceptionCode ec = 0;
        HTMLOptionElement* option = toHTMLOptionElement(value);
        if (!option)
            ec = TYPE_MISMATCH_ERR;
        else
            base->setOption(index, option, ec);
        setDOMException(exec, ec);
    }
}

}
