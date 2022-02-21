/*
 * Copyright (C) 2021 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ShadowRealmPrototype.h"

#include "AuxiliaryBarrierInlines.h"
#include "IndirectEvalExecutable.h"
#include "Interpreter.h"
#include "JSGlobalObject.h"
#include "JSInternalPromise.h"
#include "JSModuleLoader.h"
#include "ShadowRealmObject.h"
#include "StructureInlines.h"

#include "ShadowRealmPrototype.lut.h"

namespace JSC {

/* Source for ShadowRealmPrototype.lut.h
@begin shadowRealmPrototypeTable
  evaluate    JSBuiltin     DontEnum|Function  1
  importValue JSBuiltin     DontEnum|Function  2
@end
*/

const ClassInfo ShadowRealmPrototype::s_info = { "ShadowRealm", &Base::s_info, &shadowRealmPrototypeTable, nullptr, CREATE_METHOD_TABLE(ShadowRealmPrototype) };

ShadowRealmPrototype::ShadowRealmPrototype(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void ShadowRealmPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

JSC_DEFINE_HOST_FUNCTION(importInRealm, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->uncheckedArgument(0);
    ShadowRealmObject* thisRealm = jsDynamicCast<ShadowRealmObject*>(vm, thisValue);
    ASSERT(thisRealm);

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());

    auto sourceOrigin = callFrame->callerSourceOrigin(vm);
    auto* specifier = callFrame->uncheckedArgument(1).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    JSGlobalObject* realmGlobalObject = thisRealm->globalObject();
    auto* internalPromise = realmGlobalObject->moduleLoader()->importModule(realmGlobalObject, specifier, jsUndefined(), sourceOrigin);
    RETURN_IF_EXCEPTION(scope, JSValue::encode(promise->rejectWithCaughtException(realmGlobalObject, scope)));

    scope.release();
    promise->resolve(globalObject, internalPromise);
    return JSValue::encode(promise);
}

JSC_DEFINE_HOST_FUNCTION(evalInRealm, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->argument(0);
    ShadowRealmObject* thisRealm = jsDynamicCast<ShadowRealmObject*>(vm, thisValue);
    ASSERT(thisRealm);
    JSGlobalObject* realmGlobalObject = thisRealm->globalObject();

    JSValue evalArg = callFrame->argument(1);
    // eval code adapted from JSGlobalObjecFunctions::globalFuncEval
    String script = asString(evalArg)->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    NakedPtr<JSObject> executableError;
    SourceCode source = makeSource(script, callFrame->callerSourceOrigin(vm));
    EvalExecutable* eval = IndirectEvalExecutable::create(realmGlobalObject, source, DerivedContextType::None, false, EvalContextType::None, executableError);
    if (executableError) {
        ErrorInstance* error = jsDynamicCast<ErrorInstance*>(vm, JSValue(executableError.get()));
        if (error != nullptr && error->errorType() == ErrorType::SyntaxError) {
            scope.clearException();
            const String syntaxErrorMessage = error->sanitizedMessageString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            throwVMError(globalObject, scope, createSyntaxError(globalObject, syntaxErrorMessage));
        } else
            throwVMError(globalObject, scope, createTypeError(globalObject, "Error encountered during evaluation"_s));
        return JSValue::encode(jsUndefined());
    }
    RETURN_IF_EXCEPTION(scope, { });

    JSValue result = vm.interpreter->execute(eval, realmGlobalObject, realmGlobalObject->globalThis(), realmGlobalObject->globalScope());
    if (UNLIKELY(scope.exception())) {
        scope.clearException();
        return throwVMError(globalObject, scope, createTypeError(globalObject, "Error encountered during evaluation"_s));
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(result));
}

JSC_DEFINE_HOST_FUNCTION(moveFunctionToRealm, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue wrappedFnArg = callFrame->argument(0);
    JSFunction* wrappedFn = jsDynamicCast<JSFunction*>(vm, wrappedFnArg);
    JSValue targetRealmArg = callFrame->argument(1);
    ShadowRealmObject* targetRealm = jsDynamicCast<ShadowRealmObject*>(vm, targetRealmArg);
    ASSERT(targetRealm);
    RETURN_IF_EXCEPTION(scope, { });

    bool isBuiltin = false;
    JSGlobalObject* targetGlobalObj = targetRealm->globalObject();
    wrappedFn->setPrototype(vm, targetGlobalObj, targetGlobalObj->strictFunctionStructure(isBuiltin)->storedPrototype());
    RELEASE_AND_RETURN(scope, JSValue::encode(jsUndefined()));
}

} // namespace JSC
