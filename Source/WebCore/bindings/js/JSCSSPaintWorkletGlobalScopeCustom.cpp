/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSCSSPaintWorkletGlobalScope.h"

#if ENABLE(CSS_PAINTING_API)

#include "Document.h"
#include "JSCSSPaintCallback.h"
#include "JSDOMConvertCallbacks.h"
#include "JSDOMConvertSequences.h"
#include <wtf/SetForScope.h>

namespace WebCore {
using namespace JSC;

static JSValue throwInvalidModificationError(ExecState& state, ThrowScope& scope, ASCIILiteral message)
{
    scope.assertNoException();
    throwException(&state, scope, createDOMException(&state, InvalidModificationError, message));
    return jsUndefined();
}

// https://drafts.css-houdini.org/css-paint-api/#registering-custom-paint
JSValue JSCSSPaintWorkletGlobalScope::registerPaint(ExecState& state)
{
    using PaintDefinition = CSSPaintWorkletGlobalScope::PaintDefinition;

    CSSPaintWorkletGlobalScope& workletGlobalScope = wrapped();
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 2))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    AtomicString name(state.uncheckedArgument(0).toString(&state)->toAtomicString(&state));
    RETURN_IF_EXCEPTION(scope, JSValue());
    JSValue constructorValue = state.uncheckedArgument(1);
    RETURN_IF_EXCEPTION(scope, JSValue());

    if (name.isEmpty())
        return throwTypeError(&state, scope, "The first argument must not be the empty string"_s);

    auto& paintDefinitionMap = workletGlobalScope.paintDefinitionMap();

    if (paintDefinitionMap.contains(name))
        return throwInvalidModificationError(state, scope, "This name has already been registered"_s);

    Vector<String> inputProperties;

    JSValue inputPropertiesIterableValue = constructorValue.get(&state, Identifier::fromString(&vm, "inputProperties"));
    RETURN_IF_EXCEPTION(scope, JSValue());

    if (!inputPropertiesIterableValue.isUndefined())
        inputProperties = convert<IDLSequence<IDLDOMString>>(state, inputPropertiesIterableValue);
    RETURN_IF_EXCEPTION(scope, JSValue());

    // FIXME: Validate input properties here (step 7).

    Vector<String> inputArguments;

    JSValue inputArgumentsIterableValue = constructorValue.get(&state, Identifier::fromString(&vm, "inputArguments"));
    RETURN_IF_EXCEPTION(scope, JSValue());

    if (!inputArgumentsIterableValue.isUndefined())
        inputArguments = convert<IDLSequence<IDLDOMString>>(state, inputArgumentsIterableValue);
    RETURN_IF_EXCEPTION(scope, JSValue());

    // FIXME: Parse syntax for inputArguments here (steps 11 and 12).

    JSValue contextOptionsValue = constructorValue.get(&state, Identifier::fromString(&vm, "contextOptions"));
    RETURN_IF_EXCEPTION(scope, JSValue());
    UNUSED_PARAM(contextOptionsValue);

    // FIXME: Convert to PaintRenderingContext2DSettings here (step 14).

    if (!constructorValue.isConstructor(vm))
        return throwTypeError(&state, scope, "The second argument must be a constructor"_s);

    JSValue prototypeValue = constructorValue.get(&state, vm.propertyNames->prototype);
    RETURN_IF_EXCEPTION(scope, JSValue());

    if (!prototypeValue.isObject())
        return throwTypeError(&state, scope, "The second argument must have a prototype that is an object"_s);

    JSValue paintValue = prototypeValue.get(&state, Identifier::fromString(&vm, "paint"));
    RETURN_IF_EXCEPTION(scope, JSValue());

    if (paintValue.isUndefined())
        return throwTypeError(&state, scope, "The class must have a paint method"_s);

    RefPtr<JSCSSPaintCallback> paint = convert<IDLCallbackFunction<JSCSSPaintCallback>>(state, paintValue, *globalObject());
    RETURN_IF_EXCEPTION(scope, JSValue());

    auto paintDefinition = std::unique_ptr<PaintDefinition>(new PaintDefinition { name });
    paintDefinitionMap.add(name, WTFMove(paintDefinition));

    // FIXME: construct documentDefinition (step 22).

    // FIXME: This is for testing only.
    paint->handleEvent();
    RETURN_IF_EXCEPTION(scope, JSValue());

    UNUSED_PARAM(inputProperties);
    UNUSED_PARAM(inputArguments);

    return jsUndefined();
}

}
#endif
