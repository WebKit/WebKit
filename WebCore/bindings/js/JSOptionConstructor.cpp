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
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSOptionConstructor);

const ClassInfo JSOptionConstructor::s_info = { "OptionConstructor", 0, 0, 0 };

JSOptionConstructor::JSOptionConstructor(ExecState* exec, JSDOMGlobalObject* globalObject)
    : DOMConstructorWithDocument(JSOptionConstructor::createStructure(globalObject->objectPrototype()), globalObject)
{
    putDirect(exec->propertyNames().prototype, JSHTMLOptionElementPrototype::self(exec, globalObject), None);
    putDirect(exec->propertyNames().length, jsNumber(exec, 4), ReadOnly|DontDelete|DontEnum);
}

static JSObject* constructHTMLOptionElement(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    JSOptionConstructor* jsConstructor = static_cast<JSOptionConstructor*>(constructor);
    Document* document = jsConstructor->document();
    if (!document)
        return throwError(exec, ReferenceError, "Option constructor associated document is unavailable");

    String data;
    if (!args.at(0).isUndefined())
        data = args.at(0).toString(exec);

    String value;
    if (!args.at(1).isUndefined())
        value = args.at(1).toString(exec);
    bool defaultSelected = args.at(2).toBoolean(exec);
    bool selected = args.at(3).toBoolean(exec);

    ExceptionCode ec = 0;
    RefPtr<HTMLOptionElement> element = HTMLOptionElement::createForJSConstructor(document, data, value, defaultSelected, selected, ec);
    if (ec) {
        setDOMException(exec, ec);
        return 0;
    }

    return asObject(toJS(exec, jsConstructor->globalObject(), element.release()));
}

ConstructType JSOptionConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructHTMLOptionElement;
    return ConstructTypeHost;
}


} // namespace WebCore
