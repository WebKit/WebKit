/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2008, 2010, 2016 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSImageConstructor.h"

#include "HTMLNames.h"
#include "JSDOMConstructor.h"
#include "JSHTMLImageElement.h"
#include "JSNode.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

typedef JSDOMNamedConstructor<JSHTMLImageElement> JSImageConstructor;

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSImageConstructor);

template<> void JSImageConstructor::initializeProperties(VM& vm, JSDOMGlobalObject& globalObject)
{
    putDirect(vm, vm.propertyNames->prototype, JSHTMLImageElement::prototype(vm, &globalObject), None);
}

template<> JSValue JSImageConstructor::prototypeForStructure(VM& vm, const JSDOMGlobalObject& globalObject)
{
    return JSHTMLElement::getConstructor(vm, &globalObject);
}

template<> EncodedJSValue JSImageConstructor::construct(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSImageConstructor* jsConstructor = jsCast<JSImageConstructor*>(state->callee());
    Document* document = jsConstructor->document();
    if (!document)
        return throwVMError(state, scope, createReferenceError(state, "Image constructor associated document is unavailable"));

    // Calling toJS on the document causes the JS document wrapper to be
    // added to the window object. This is done to ensure that JSDocument::visit
    // will be called, which will cause the image element to be marked if necessary.
    toJS(state, jsConstructor->globalObject(), *document);
    int width;
    int height;
    int* optionalWidth = 0;
    int* optionalHeight = 0;
    if (state->argumentCount() > 0) {
        width = state->argument(0).toInt32(state);
        optionalWidth = &width;
    }
    if (state->argumentCount() > 1) {
        height = state->argument(1).toInt32(state);
        optionalHeight = &height;
    }

    return JSValue::encode(asObject(toJS(state, jsConstructor->globalObject(),
        HTMLImageElement::createForJSConstructor(*document, optionalWidth, optionalHeight))));
}

template<> const ClassInfo JSImageConstructor::s_info = { "ImageConstructor", &Base::s_info, 0, CREATE_METHOD_TABLE(JSImageConstructor) };

JSC::JSValue createImageConstructor(JSC::VM& vm, const JSDOMGlobalObject& globalObject)
{
    return getDOMConstructor<JSImageConstructor>(vm, globalObject);
}

} // namespace WebCore
