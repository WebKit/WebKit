/*
 * Copyright (C) 2007-2016 Apple Inc. All rights reserved.
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
#include "JSHTMLDocument.h"

#include "HTMLIFrameElement.h"
#include "JSDOMWindowCustom.h"
#include "JSHTMLCollection.h"
#include "SegmentedString.h"
#include <runtime/Lookup.h>

using namespace JSC;

namespace WebCore {

using namespace HTMLNames;

JSValue toJSNewlyCreated(ExecState* state, JSDOMGlobalObject* globalObject, Ref<HTMLDocument>&& passedDocument)
{
    auto& document = passedDocument.get();
    auto* wrapper = createWrapper<HTMLDocument>(globalObject, WTFMove(passedDocument));
    reportMemoryForDocumentIfFrameless(*state, document);
    return wrapper;
}

JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, HTMLDocument& document)
{
    if (auto* wrapper = cachedDocumentWrapper(*state, *globalObject, document))
        return wrapper;
    return toJSNewlyCreated(state, globalObject, Ref<HTMLDocument>(document));
}

bool JSHTMLDocument::getOwnPropertySlot(JSObject* object, ExecState* state, PropertyName propertyName, PropertySlot& slot)
{
    auto& thisObject = *jsCast<JSHTMLDocument*>(object);
    ASSERT_GC_OBJECT_INHERITS(object, info());

    if (propertyName == "open") {
        if (Base::getOwnPropertySlot(&thisObject, state, propertyName, slot))
            return true;
        slot.setCustom(&thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsHTMLDocumentPrototypeFunctionOpen, 2>);
        return true;
    }

    JSValue value;
    if (thisObject.nameGetter(state, propertyName, value)) {
        slot.setValue(&thisObject, ReadOnly | DontDelete | DontEnum, value);
        return true;
    }

    return Base::getOwnPropertySlot(&thisObject, state, propertyName, slot);
}

bool JSHTMLDocument::nameGetter(ExecState* state, PropertyName propertyName, JSValue& value)
{
    auto& document = wrapped();

    auto* atomicPropertyName = propertyName.publicName();
    if (!atomicPropertyName || !document.hasDocumentNamedItem(*atomicPropertyName))
        return false;

    if (UNLIKELY(document.documentNamedItemContainsMultipleElements(*atomicPropertyName))) {
        auto collection = document.documentNamedItems(atomicPropertyName);
        ASSERT(collection->length() > 1);
        value = toJS(state, globalObject(), collection);
        return true;
    }

    auto& element = *document.documentNamedItem(*atomicPropertyName);
    if (UNLIKELY(is<HTMLIFrameElement>(element))) {
        if (auto* frame = downcast<HTMLIFrameElement>(element).contentFrame()) {
            value = toJS(state, frame);
            return true;
        }
    }

    value = toJS(state, globalObject(), element);
    return true;
}

// Custom attributes

JSValue JSHTMLDocument::all(ExecState& state) const
{
    // If "all" has been overwritten, return the overwritten value
    if (auto overwrittenValue = getDirect(state.vm(), Identifier::fromString(&state, "all")))
        return overwrittenValue;

    return toJS(&state, globalObject(), wrapped().all());
}

void JSHTMLDocument::setAll(ExecState& state, JSValue value)
{
    // Add "all" to the property map.
    putDirect(state.vm(), Identifier::fromString(&state, "all"), value);
}

static inline Document* findCallingDocument(ExecState& state)
{
    CallerFunctor functor;
    state.iterate(functor);
    auto* callerFrame = functor.callerFrame();
    if (!callerFrame)
        return nullptr;
    return asJSDOMWindow(callerFrame->lexicalGlobalObject())->wrapped().document();
}

// Custom functions

JSValue JSHTMLDocument::open(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // For compatibility with other browsers, pass open calls with more than 2 parameters to the window.
    if (state.argumentCount() > 2) {
        if (auto* frame = wrapped().frame()) {
            if (auto* wrapper = toJSDOMWindowShell(frame, currentWorld(&state))) {
                auto function = wrapper->get(&state, Identifier::fromString(&state, "open"));
                CallData callData;
                auto callType = ::getCallData(function, callData);
                if (callType == CallType::None)
                    return throwTypeError(&state, scope);
                return JSC::call(&state, function, callType, callData, wrapper, ArgList(&state));
            }
        }
        return jsUndefined();
    }

    // Calling document.open clobbers the security context of the document and aliases it with the active security context.
    // FIXME: Is it correct that this does not use findCallingDocument as the write function below does?
    wrapped().open(asJSDOMWindow(state.lexicalGlobalObject())->wrapped().document());
    // FIXME: Why do we return the document instead of returning undefined?
    return this;
}

enum NewlineRequirement { DoNotAddNewline, DoAddNewline };

static inline JSValue documentWrite(ExecState& state, JSHTMLDocument& document, NewlineRequirement addNewline)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    SegmentedString segmentedString;
    size_t argumentCount = state.argumentCount();
    for (size_t i = 0; i < argumentCount; ++i) {
        segmentedString.append(state.uncheckedArgument(i).toWTFString(&state));
        RETURN_IF_EXCEPTION(scope, { });
    }
    if (addNewline)
        segmentedString.append(String { "\n" });

    document.wrapped().write(WTFMove(segmentedString), findCallingDocument(state));
    return jsUndefined();
}

JSValue JSHTMLDocument::write(ExecState& state)
{
    return documentWrite(state, *this, DoNotAddNewline);
}

JSValue JSHTMLDocument::writeln(ExecState& state)
{
    return documentWrite(state, *this, DoAddNewline);
}

} // namespace WebCore
