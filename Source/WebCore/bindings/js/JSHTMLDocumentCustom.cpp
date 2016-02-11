/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "Frame.h"
#include "HTMLAllCollection.h"
#include "HTMLBodyElement.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "JSDOMWindow.h"
#include "JSDOMWindowCustom.h"
#include "JSDOMWindowShell.h"
#include "JSHTMLCollection.h"
#include "JSMainThreadExecState.h"
#include "SegmentedString.h"
#include "DocumentParser.h"
#include <interpreter/StackVisitor.h>
#include <runtime/Error.h>
#include <runtime/JSCell.h>
#include <wtf/unicode/CharacterNames.h>

using namespace JSC;

namespace WebCore {

using namespace HTMLNames;

bool JSHTMLDocument::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSHTMLDocument* thisObject = jsCast<JSHTMLDocument*>(object);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    if (propertyName == "open") {
        if (Base::getOwnPropertySlot(thisObject, exec, propertyName, slot))
            return true;

        slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsHTMLDocumentPrototypeFunctionOpen, 2>);
        return true;
    }

    JSValue value;
    if (thisObject->nameGetter(exec, propertyName, value)) {
        slot.setValue(thisObject, ReadOnly | DontDelete | DontEnum, value);
        return true;
    }

    static_assert(!hasStaticPropertyTable, "This method does not handle static instance properties");

    return Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

bool JSHTMLDocument::nameGetter(ExecState* exec, PropertyName propertyName, JSValue& value)
{
    auto& document = wrapped();

    AtomicStringImpl* atomicPropertyName = propertyName.publicName();
    if (!atomicPropertyName || !document.hasDocumentNamedItem(*atomicPropertyName))
        return false;

    if (UNLIKELY(document.documentNamedItemContainsMultipleElements(*atomicPropertyName))) {
        Ref<HTMLCollection> collection = document.documentNamedItems(atomicPropertyName);
        ASSERT(collection->length() > 1);
        value = toJS(exec, globalObject(), WTF::getPtr(collection));
        return true;
    }

    Element* element = document.documentNamedItem(*atomicPropertyName);
    if (UNLIKELY(is<HTMLIFrameElement>(*element))) {
        if (Frame* frame = downcast<HTMLIFrameElement>(*element).contentFrame()) {
            value = toJS(exec, frame);
            return true;
        }
    }

    value = toJS(exec, globalObject(), element);
    return true;
}

// Custom attributes

JSValue JSHTMLDocument::all(ExecState& state) const
{
    // If "all" has been overwritten, return the overwritten value
    JSValue v = getDirect(state.vm(), Identifier::fromString(&state, "all"));
    if (v)
        return v;

    return toJS(&state, globalObject(), wrapped().all());
}

void JSHTMLDocument::setAll(ExecState& state, JSValue value)
{
    // Add "all" to the property map.
    putDirect(state.vm(), Identifier::fromString(&state, "all"), value);
}

static Document* findCallingDocument(ExecState& state)
{
    CallerFunctor functor;
    state.iterate(functor);
    CallFrame* callerFrame = functor.callerFrame();
    if (!callerFrame)
        return nullptr;

    return asJSDOMWindow(functor.callerFrame()->lexicalGlobalObject())->wrapped().document();
}

// Custom functions

JSValue JSHTMLDocument::open(ExecState& state)
{
    // For compatibility with other browsers, pass open calls with more than 2 parameters to the window.
    if (state.argumentCount() > 2) {
        if (Frame* frame = wrapped().frame()) {
            JSDOMWindowShell* wrapper = toJSDOMWindowShell(frame, currentWorld(&state));
            if (wrapper) {
                JSValue function = wrapper->get(&state, Identifier::fromString(&state, "open"));
                CallData callData;
                CallType callType = ::getCallData(function, callData);
                if (callType == CallTypeNone)
                    return throwTypeError(&state);
                return JSC::call(&state, function, callType, callData, wrapper, ArgList(&state));
            }
        }
        return jsUndefined();
    }

    // document.open clobbers the security context of the document and
    // aliases it with the active security context.
    Document* activeDocument = asJSDOMWindow(state.lexicalGlobalObject())->wrapped().document();

    // In the case of two parameters or fewer, do a normal document open.
    wrapped().open(activeDocument);
    return this;
}

enum NewlineRequirement { DoNotAddNewline, DoAddNewline };

static inline void documentWrite(ExecState& state, JSHTMLDocument* thisDocument, NewlineRequirement addNewline)
{
    HTMLDocument* document = &thisDocument->wrapped();
    // DOM only specifies single string argument, but browsers allow multiple or no arguments.

    size_t size = state.argumentCount();

    String firstString = state.argument(0).toString(&state)->value(&state);
    SegmentedString segmentedString = firstString;
    if (size != 1) {
        if (!size)
            segmentedString.clear();
        else {
            for (size_t i = 1; i < size; ++i) {
                String subsequentString = state.uncheckedArgument(i).toString(&state)->value(&state);
                segmentedString.append(SegmentedString(subsequentString));
            }
        }
    }
    if (addNewline)
        segmentedString.append(SegmentedString(String(&newlineCharacter, 1)));

    Document* activeDocument = findCallingDocument(state);
    document->write(segmentedString, activeDocument);
}

JSValue JSHTMLDocument::write(ExecState& state)
{
    documentWrite(state, this, DoNotAddNewline);
    return jsUndefined();
}

JSValue JSHTMLDocument::writeln(ExecState& state)
{
    documentWrite(state, this, DoAddNewline);
    return jsUndefined();
}

} // namespace WebCore
