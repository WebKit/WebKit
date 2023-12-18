/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
#include "PaintWorkletGlobalScope.h"

#if ENABLE(CSS_PAINTING_API)

#include "Document.h"
#include "JSCSSPaintCallback.h"
#include "JSDOMConvert.h"
#include "LocalDOMWindow.h"
#include "RenderView.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>

namespace WebCore {
using namespace JSC;

WTF_MAKE_ISO_ALLOCATED_IMPL(PaintWorkletGlobalScope);

RefPtr<PaintWorkletGlobalScope> PaintWorkletGlobalScope::tryCreate(Document& document, ScriptSourceCode&& code)
{
    RefPtr<VM> vm = VM::tryCreate();
    if (!vm)
        return nullptr;
    auto scope = adoptRef(*new PaintWorkletGlobalScope(document, vm.releaseNonNull(), WTFMove(code)));
    scope->addToContextsMap();
    return scope;
}

PaintWorkletGlobalScope::PaintWorkletGlobalScope(Document& document, Ref<VM>&& vm, ScriptSourceCode&& code)
    : WorkletGlobalScope(document, WTFMove(vm), WTFMove(code))
{
}

double PaintWorkletGlobalScope::devicePixelRatio() const
{
    if (!responsibleDocument() || !responsibleDocument()->domWindow())
        return 1.0;
    return responsibleDocument()->domWindow()->devicePixelRatio();
}

PaintWorkletGlobalScope::PaintDefinition::PaintDefinition(const AtomString& name, JSC::JSObject* paintConstructor, Ref<CSSPaintCallback>&& paintCallback, Vector<AtomString>&& inputProperties, Vector<String>&& inputArguments)
    : name(name)
    , paintConstructor(paintConstructor)
    , paintCallback(WTFMove(paintCallback))
    , inputProperties(WTFMove(inputProperties))
    , inputArguments(WTFMove(inputArguments))
{
}

// https://drafts.css-houdini.org/css-paint-api/#registering-custom-paint
ExceptionOr<void> PaintWorkletGlobalScope::registerPaint(JSC::JSGlobalObject& globalObject, const AtomString& name, Strong<JSObject> paintConstructor)
{
    auto& vm = paintConstructor->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Validate that paintConstructor is a VoidFunction
    if (!paintConstructor->isCallable())
        return Exception { ExceptionCode::TypeError, "paintConstructor must be callable"_s };

    if (name.isEmpty())
        return Exception { ExceptionCode::TypeError, "The first argument must not be the empty string"_s };

    {
        Locker locker { paintDefinitionLock() };

        if (paintDefinitionMap().contains(name))
            return Exception { ExceptionCode::InvalidModificationError, "This name has already been registered"_s };

        Vector<AtomString> inputProperties;

        JSValue inputPropertiesIterableValue = paintConstructor->get(&globalObject, Identifier::fromString(vm, "inputProperties"_s));
        RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

        if (!inputPropertiesIterableValue.isUndefined())
            inputProperties = convert<IDLSequence<IDLAtomStringAdaptor<IDLDOMString>>>(globalObject, inputPropertiesIterableValue);
        RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

        // FIXME: Validate input properties here (step 7).

        Vector<String> inputArguments;

        JSValue inputArgumentsIterableValue = paintConstructor->get(&globalObject, Identifier::fromString(vm, "inputArguments"_s));
        RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

        if (!inputArgumentsIterableValue.isUndefined())
            inputArguments = convert<IDLSequence<IDLDOMString>>(globalObject, inputArgumentsIterableValue);
        RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

        // FIXME: Parse syntax for inputArguments here (steps 11 and 12).

        JSValue contextOptionsValue = paintConstructor->get(&globalObject, Identifier::fromString(vm, "contextOptions"_s));
        RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });
        UNUSED_PARAM(contextOptionsValue);

        // FIXME: Convert to PaintRenderingContext2DSettings here (step 14).

        if (!paintConstructor->isConstructor())
            return Exception { ExceptionCode::TypeError, "The second argument must be a constructor"_s };

        JSValue prototypeValue = paintConstructor->get(&globalObject, vm.propertyNames->prototype);
        RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

        if (!prototypeValue.isObject())
            return Exception { ExceptionCode::TypeError, "The second argument must have a prototype that is an object"_s };

        JSValue paintValue = prototypeValue.get(&globalObject, Identifier::fromString(vm, "paint"_s));
        RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

        if (paintValue.isUndefined())
            return Exception { ExceptionCode::TypeError, "The class must have a paint method"_s };

        RefPtr<JSCSSPaintCallback> paint = convert<IDLCallbackFunction<JSCSSPaintCallback>>(globalObject, paintValue, *jsCast<JSDOMGlobalObject*>(&globalObject));
        RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

        auto paintDefinition = makeUnique<PaintDefinition>(name, paintConstructor.get(), paint.releaseNonNull(), WTFMove(inputProperties), WTFMove(inputArguments));
        paintDefinitionMap().add(name, WTFMove(paintDefinition));
    }

    // This is for the case when we have already visited the paint definition map, and the GC is currently running in the background.
    vm.writeBarrier(&globalObject);

    // FIXME: construct documentDefinition (step 22).

    // FIXME: we should only repaint affected custom paint images <https://bugs.webkit.org/show_bug.cgi?id=192322>.
    if (responsibleDocument() && responsibleDocument()->renderView())
        responsibleDocument()->renderView()->repaintRootContents();

    return { };
}

} // namespace WebCore

#endif
