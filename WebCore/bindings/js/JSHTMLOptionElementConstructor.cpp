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
#include "JSHTMLOptionElementConstructor.h"

#include "Document.h"
#include "HTMLOptionElement.h"
#include "JSHTMLOptionElement.h"
#include "Text.h"

using namespace KJS;

namespace WebCore {

JSHTMLOptionElementConstructor::JSHTMLOptionElementConstructor(ExecState* exec, Document* d)
    : m_doc(d)
{
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
    put(exec, lengthPropertyName, jsNumber(4), ReadOnly|DontDelete|DontEnum);
}

bool JSHTMLOptionElementConstructor::implementsConstruct() const
{
    return true;
}

JSObject* JSHTMLOptionElementConstructor::construct(ExecState* exec, const List& args)
{
    int exception = 0;
    RefPtr<Element> el(m_doc->createElement("option", exception));
    HTMLOptionElement* opt = 0;
    if (el) {
        opt = static_cast<HTMLOptionElement*>(el.get());
        int sz = args.size();
        RefPtr<Text> text = m_doc->createTextNode("");
        opt->appendChild(text, exception);
        if (exception == 0 && sz > 0)
            text->setData(args[0]->toString(exec), exception);
        if (exception == 0 && sz > 1)
            opt->setValue(args[1]->toString(exec));
        if (exception == 0 && sz > 2)
            opt->setDefaultSelected(args[2]->toBoolean(exec));
        if (exception == 0 && sz > 3)
            opt->setSelected(args[3]->toBoolean(exec));
    }

    setDOMException(exec, exception);
    return static_cast<JSObject*>(toJS(exec, opt));
}

}
