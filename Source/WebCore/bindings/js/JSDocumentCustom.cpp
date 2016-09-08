/*
 * Copyright (C) 2007-2009, 2011, 2016 Apple Inc. All rights reserved.
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
#include "JSDocument.h"

#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "JSCanvasRenderingContext2D.h"
#include "JSDOMConvert.h"
#include "JSDOMWindowCustom.h"
#include "JSHTMLDocument.h"
#include "JSLocation.h"
#include "JSNodeOrString.h"
#include "JSXMLDocument.h"
#include "Location.h"
#include "NodeTraversal.h"
#include "SVGDocument.h"
#include "ScriptController.h"
#include "TouchList.h"
#include "XMLDocument.h"
#include <wtf/GetPtr.h>

#if ENABLE(WEBGL)
#include "JSWebGLRenderingContextBase.h"
#endif

#if ENABLE(TOUCH_EVENTS)
#include "JSTouch.h"
#include "JSTouchList.h"
#endif

using namespace JSC;

namespace WebCore {

static inline JSValue createNewDocumentWrapper(ExecState& state, JSDOMGlobalObject& globalObject, Ref<Document>&& passedDocument)
{
    auto& document = passedDocument.get();
    JSObject* wrapper;
    if (document.isHTMLDocument())
        wrapper = CREATE_DOM_WRAPPER(&globalObject, HTMLDocument, WTFMove(passedDocument));
    else if (document.isXMLDocument())
        wrapper = CREATE_DOM_WRAPPER(&globalObject, XMLDocument, WTFMove(passedDocument));
    else
        wrapper = CREATE_DOM_WRAPPER(&globalObject, Document, WTFMove(passedDocument));

    reportMemoryForDocumentIfFrameless(state, document);

    return wrapper;
}

JSObject* cachedDocumentWrapper(ExecState& state, JSDOMGlobalObject& globalObject, Document& document)
{
    if (auto* wrapper = getCachedWrapper(globalObject.world(), document))
        return wrapper;

    auto* window = document.domWindow();
    if (!window)
        return nullptr;

    // Creating a wrapper for domWindow might have created a wrapper for document as well.
    return getCachedWrapper(toJSDOMWindow(toJS(&state, *window))->world(), document);
}

void reportMemoryForDocumentIfFrameless(ExecState& state, Document& document)
{
    // Make sure the document is kept around by the window object, and works right with the back/forward cache.
    if (document.frame())
        return;

    size_t memoryCost = 0;
    for (Node* node = &document; node; node = NodeTraversal::next(*node))
        memoryCost += node->approximateMemoryCost();

    // FIXME: Adopt reportExtraMemoryVisited, and switch to reportExtraMemoryAllocated.
    // https://bugs.webkit.org/show_bug.cgi?id=142595
    state.heap()->deprecatedReportExtraMemory(memoryCost);
}

JSValue toJSNewlyCreated(ExecState* state, JSDOMGlobalObject* globalObject, Ref<Document>&& document)
{
    return createNewDocumentWrapper(*state, *globalObject, WTFMove(document));
}

JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, Document& document)
{
    if (auto* wrapper = cachedDocumentWrapper(*state, *globalObject, document))
        return wrapper;
    return toJSNewlyCreated(state, globalObject, Ref<Document>(document));
}

JSValue JSDocument::prepend(ExecState& state)
{
    ExceptionCode ec = 0;
    wrapped().prepend(toNodeOrStringVector(state), ec);
    setDOMException(&state, ec);

    return jsUndefined();
}

JSValue JSDocument::append(ExecState& state)
{
    ExceptionCode ec = 0;
    wrapped().append(toNodeOrStringVector(state), ec);
    setDOMException(&state, ec);

    return jsUndefined();
}

#if ENABLE(TOUCH_EVENTS)
JSValue JSDocument::createTouchList(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto touchList = TouchList::create();

    for (size_t i = 0; i < state.argumentCount(); ++i) {
        auto* item = JSTouch::toWrapped(state.uncheckedArgument(i));
        if (!item)
            return JSValue::decode(throwArgumentTypeError(state, scope, i, "touches", "Document", "createTouchList", "Touch"));

        touchList->append(*item);
    }
    return toJSNewlyCreated(&state, globalObject(), WTFMove(touchList));
}
#endif

JSValue JSDocument::getCSSCanvasContext(JSC::ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 4))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    auto contextId = state.uncheckedArgument(0).toWTFString(&state);
    if (UNLIKELY(state.hadException()))
        return jsUndefined();
    auto name = state.uncheckedArgument(1).toWTFString(&state);
    if (UNLIKELY(state.hadException()))
        return jsUndefined();
    auto width = convert<int32_t>(state, state.uncheckedArgument(2), NormalConversion);
    if (UNLIKELY(state.hadException()))
        return jsUndefined();
    auto height = convert<int32_t>(state, state.uncheckedArgument(3), NormalConversion);
    if (UNLIKELY(state.hadException()))
        return jsUndefined();

    auto* context = wrapped().getCSSCanvasContext(WTFMove(contextId), WTFMove(name), WTFMove(width), WTFMove(height));
    if (!context)
        return jsNull();

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(*context))
        return toJS(&state, globalObject(), downcast<WebGLRenderingContextBase>(*context));
#endif

    return toJS(&state, globalObject(), downcast<CanvasRenderingContext2D>(*context));
}

void JSDocument::visitAdditionalChildren(SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(wrapped().scriptExecutionContext());
}

} // namespace WebCore
