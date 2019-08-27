/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "DOMWindow.h"
#include "Document.h"
#include "JSCSSPaintCallback.h"
#include "JSDOMConvertCallbacks.h"
#include "JSDOMConvertSequences.h"
#include "RenderView.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>

namespace WebCore {
using namespace JSC;

WTF_MAKE_ISO_ALLOCATED_IMPL(PaintWorkletGlobalScope);

Ref<PaintWorkletGlobalScope> PaintWorkletGlobalScope::create(Document& document, ScriptSourceCode&& code)
{
    return adoptRef(*new PaintWorkletGlobalScope(document, WTFMove(code)));
}

PaintWorkletGlobalScope::PaintWorkletGlobalScope(Document& document, ScriptSourceCode&& code)
    : WorkletGlobalScope(document, WTFMove(code))
{
}

double PaintWorkletGlobalScope::devicePixelRatio() const
{
    if (!responsibleDocument() || !responsibleDocument()->domWindow())
        return 1.0;
    return responsibleDocument()->domWindow()->devicePixelRatio();
}

PaintWorkletGlobalScope::PaintDefinition::PaintDefinition(const AtomString& name, JSC::JSObject* paintConstructor, Ref<CSSPaintCallback>&& paintCallback, Vector<String>&& inputProperties, Vector<String>&& inputArguments)
    : name(name)
    , paintConstructor(paintConstructor)
    , paintCallback(WTFMove(paintCallback))
    , inputProperties(WTFMove(inputProperties))
    , inputArguments(WTFMove(inputArguments))
{
}

// https://drafts.css-houdini.org/css-paint-api/#registering-custom-paint
ExceptionOr<void> PaintWorkletGlobalScope::registerPaint(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const String& name, Strong<JSObject> paintConstructor)
{
    auto& vm = paintConstructor->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Validate that paintConstructor is a VoidFunction
    CallData callData;
    if (JSC::getCallData(vm, paintConstructor.get(), callData) == JSC::CallType::None)
        return Exception { TypeError, "paintConstructor must be callable" };

    if (name.isEmpty())
        return Exception { TypeError, "The first argument must not be the empty string" };

    {
        auto locker = holdLock(paintDefinitionLock());

        if (paintDefinitionMap().contains(name))
            return Exception { InvalidModificationError, "This name has already been registered" };

        Vector<String> inputProperties;

        JSValue inputPropertiesIterableValue = paintConstructor->get(&state, Identifier::fromString(vm, "inputProperties"));
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        if (!inputPropertiesIterableValue.isUndefined())
            inputProperties = convert<IDLSequence<IDLDOMString>>(state, inputPropertiesIterableValue);
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        // FIXME: Validate input properties here (step 7).

        Vector<String> inputArguments;

        JSValue inputArgumentsIterableValue = paintConstructor->get(&state, Identifier::fromString(vm, "inputArguments"));
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        if (!inputArgumentsIterableValue.isUndefined())
            inputArguments = convert<IDLSequence<IDLDOMString>>(state, inputArgumentsIterableValue);
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        // FIXME: Parse syntax for inputArguments here (steps 11 and 12).

        JSValue contextOptionsValue = paintConstructor->get(&state, Identifier::fromString(vm, "contextOptions"));
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });
        UNUSED_PARAM(contextOptionsValue);

        // FIXME: Convert to PaintRenderingContext2DSettings here (step 14).

        if (!paintConstructor->isConstructor(vm))
            return Exception { TypeError, "The second argument must be a constructor" };

        JSValue prototypeValue = paintConstructor->get(&state, vm.propertyNames->prototype);
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        if (!prototypeValue.isObject())
            return Exception { TypeError, "The second argument must have a prototype that is an object" };

        JSValue paintValue = prototypeValue.get(&state, Identifier::fromString(vm, "paint"));
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        if (paintValue.isUndefined())
            return Exception { TypeError, "The class must have a paint method" };

        RefPtr<JSCSSPaintCallback> paint = convert<IDLCallbackFunction<JSCSSPaintCallback>>(state, paintValue, globalObject);
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

        auto paintDefinition = makeUnique<PaintDefinition>(name, paintConstructor.get(), paint.releaseNonNull(), WTFMove(inputProperties), WTFMove(inputArguments));
        paintDefinitionMap().add(name, WTFMove(paintDefinition));
    }

    // This is for the case when we have already visited the paint definition map, and the GC is currently running in the background.
    vm.heap.writeBarrier(&globalObject);

    // FIXME: construct documentDefinition (step 22).

    // FIXME: we should only repaint affected custom paint images <https://bugs.webkit.org/show_bug.cgi?id=192322>.
    if (responsibleDocument() && responsibleDocument()->renderView())
        responsibleDocument()->renderView()->repaintRootContents();

    return { };
}

} // namespace WebCore

#endif
