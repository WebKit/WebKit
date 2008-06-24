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
#include "JSHTMLOptionElementConstructor.h"

#include "Document.h"
#include "HTMLOptionElement.h"
#include "JSHTMLOptionElement.h"
#include "Text.h"

using namespace KJS;

namespace WebCore {

const ClassInfo JSHTMLOptionElementConstructor::s_info = { "OptionConstructor", 0, 0, 0 };

JSHTMLOptionElementConstructor::JSHTMLOptionElementConstructor(ExecState* exec, Document* document)
    : DOMObject(exec->lexicalGlobalObject()->objectPrototype())
    , m_document(document)
{
    putDirect(exec->propertyNames().length, jsNumber(exec, 4), ReadOnly|DontDelete|DontEnum);
}

static JSObject* constructHTMLOptionElement(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    Document* document = static_cast<JSHTMLOptionElementConstructor*>(constructor)->document();

    ExceptionCode ec = 0;

    RefPtr<HTMLOptionElement> element = static_pointer_cast<HTMLOptionElement>(document->createElement("option", ec));
    RefPtr<Text> text;
    if (ec == 0)
        text = document->createTextNode("");
    if (ec == 0 && !args[0]->isUndefined())
        text->setData(args[0]->toString(exec), ec);
    if (ec == 0)
        element->appendChild(text.release(), ec);
    if (ec == 0 && !args[1]->isUndefined())
        element->setValue(args[1]->toString(exec));
    if (ec == 0)
        element->setDefaultSelected(args[2]->toBoolean(exec));
    if (ec == 0)
        element->setSelected(args[3]->toBoolean(exec));

    if (ec) {
        setDOMException(exec, ec);
        return 0;
    }

    return static_cast<JSObject*>(toJS(exec, element.release()));
}

ConstructType JSHTMLOptionElementConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructHTMLOptionElement;
    return ConstructTypeNative;
}

} // namespace WebCore
