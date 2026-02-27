/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2013 Michael Pruett <michael@68k.org>
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
#include "JSDOMConstructor.h"

#include "WebCoreJSClientData.h"
#include <JavaScriptCore/JSCInlines.h>

namespace WebCore {
using namespace JSC;

EncodedJSValue constructJSHTMLElement(JSGlobalObject*, CallFrame&);

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSDOMConstructorBase);

JSC_DEFINE_HOST_FUNCTION(callThrowTypeErrorForJSDOMConstructor, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwTypeError(globalObject, scope, "Constructor requires 'new' operator"_s);
    return JSValue::encode(jsNull());
}

JSC_DEFINE_HOST_FUNCTION(callThrowTypeErrorForJSDOMConstructorNotConstructable, (JSC::JSGlobalObject* globalObject, JSC::CallFrame* callFrame))
{
    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Support customized built-in elements: when a non-constructable HTML element
    // interface (e.g. HTMLDivElement, HTMLParagraphElement) is subclassed via
    // `class MyDiv extends HTMLDivElement {}`, delegate to the HTMLElement
    // constructor which handles custom element registration and upgrade.
    if (callFrame && !callFrame->newTarget().isUndefined()) {
        auto result = constructJSHTMLElement(globalObject, *callFrame);
        if (!scope.exception())
            return result;
        // constructJSHTMLElement failed (e.g., new.target is not a registered
        // custom element). Fall through to throw "Illegal constructor".
        if (!scope.tryClearException())
            return JSC::JSValue::encode(JSC::jsNull());
    }

    JSC::throwTypeError(globalObject, scope, "Illegal constructor"_s);
    return JSC::JSValue::encode(JSC::jsNull());
}

JSC::GCClient::IsoSubspace* JSDOMConstructorBase::subspaceForImpl(JSC::VM& vm)
{
    return &downcast<JSVMClientData>(vm.clientData)->domConstructorSpace();
}

} // namespace WebCore
