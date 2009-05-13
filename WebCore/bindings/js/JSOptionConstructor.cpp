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
#include "JSOptionConstructor.h"

#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "JSHTMLOptionElement.h"
#include "ScriptExecutionContext.h"
#include "Text.h"

using namespace JSC;

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSOptionConstructor);

const ClassInfo JSOptionConstructor::s_info = { "OptionConstructor", 0, 0, 0 };

JSOptionConstructor::JSOptionConstructor(ExecState* exec, JSDOMGlobalObject* globalObject)
    : DOMObject(JSOptionConstructor::createStructure(exec->lexicalGlobalObject()->objectPrototype()))
    , m_globalObject(globalObject)
{
    ASSERT(globalObject->scriptExecutionContext());
    ASSERT(globalObject->scriptExecutionContext()->isDocument());

    putDirect(exec->propertyNames().prototype, JSHTMLOptionElementPrototype::self(exec, exec->lexicalGlobalObject()), None);
    putDirect(exec->propertyNames().length, jsNumber(exec, 4), ReadOnly|DontDelete|DontEnum);
}

Document* JSOptionConstructor::document() const
{
    return static_cast<Document*>(m_globalObject->scriptExecutionContext());
}

static JSObject* constructHTMLOptionElement(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    Document* document = static_cast<JSOptionConstructor*>(constructor)->document();
    if (!document)
        return throwError(exec, ReferenceError, "Option constructor associated document is unavailable");

    RefPtr<HTMLOptionElement> element = static_pointer_cast<HTMLOptionElement>(document->createElement(HTMLNames::optionTag, false));

    ExceptionCode ec = 0;
    RefPtr<Text> text = document->createTextNode("");
    if (!args.at(0).isUndefined())
        text->setData(args.at(0).toString(exec), ec);
    if (ec == 0)
        element->appendChild(text.release(), ec);
    if (ec == 0 && !args.at(1).isUndefined())
        element->setValue(args.at(1).toString(exec));
    if (ec == 0)
        element->setDefaultSelected(args.at(2).toBoolean(exec));
    if (ec == 0)
        element->setSelected(args.at(3).toBoolean(exec));

    if (ec) {
        setDOMException(exec, ec);
        return 0;
    }

    return asObject(toJS(exec, element.release()));
}

ConstructType JSOptionConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructHTMLOptionElement;
    return ConstructTypeHost;
}

void JSOptionConstructor::mark()
{
    DOMObject::mark();
    if (!m_globalObject->marked())
        m_globalObject->mark();
}

} // namespace WebCore
