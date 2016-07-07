/*
 * Copyright (C) 2007, 2008, 2009, 2011 Apple Inc. All rights reserved.
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

#include "CustomElementDefinitions.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "JSCanvasRenderingContext2D.h"
#include "JSDOMWindowCustom.h"
#include "JSHTMLDocument.h"
#include "JSLocation.h"
#include "JSNodeOrString.h"
#include "JSSVGDocument.h"
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
    else if (document.isSVGDocument())
        wrapper = CREATE_DOM_WRAPPER(&globalObject, SVGDocument, WTFMove(passedDocument));
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
    auto touchList = TouchList::create();

    for (size_t i = 0; i < state.argumentCount(); ++i) {
        auto* item = JSTouch::toWrapped(state.uncheckedArgument(i));
        if (!item)
            return JSValue::decode(throwArgumentTypeError(state, i, "touches", "Document", "createTouchList", "Touch"));

        touchList->append(*item);
    }
    return toJSNewlyCreated(&state, globalObject(), WTFMove(touchList));
}
#endif

#if ENABLE(CUSTOM_ELEMENTS)
JSValue JSDocument::defineElement(ExecState& state)
{
    AtomicString tagName(state.argument(0).toString(&state)->toAtomicString(&state));
    if (UNLIKELY(state.hadException()))
        return jsUndefined();

    JSObject* object = state.argument(1).getObject();
    ConstructData callData;
    if (!object || object->methodTable()->getConstructData(object, callData) == ConstructType::None)
        return throwTypeError(&state, ASCIILiteral("The second argument must be a constructor"));

    Document& document = wrapped();
    if (!document.domWindow()) {
        throwNotSupportedError(state, "Cannot define a custom element in a docuemnt without a browsing context");
        return jsUndefined();
    }

    switch (Document::validateCustomElementName(tagName)) {
    case CustomElementNameValidationStatus::Valid:
        break;
    case CustomElementNameValidationStatus::ConflictsWithBuiltinNames:
        return throwSyntaxError(&state, "Custom element name cannot be same as one of the builtin elements");
    case CustomElementNameValidationStatus::NoHyphen:
        return throwSyntaxError(&state, "Custom element name must contain a hyphen");
    case CustomElementNameValidationStatus::ContainsUpperCase:
        return throwSyntaxError(&state, "Custom element name cannot contain an upper case letter");
    }

    auto& definitions = document.ensureCustomElementDefinitions();
    if (definitions.findInterface(tagName)) {
        throwNotSupportedError(state, "Cannot define multiple custom elements with the same tag name");
        return jsUndefined();
    }

    if (definitions.containsConstructor(object)) {
        throwNotSupportedError(state, "Cannot define multiple custom elements with the same class");
        return jsUndefined();
    }

    // FIXME: 10. Let prototype be Get(constructor, "prototype"). Rethrow any exceptions.
    // FIXME: 11. If Type(prototype) is not Object, throw a TypeError exception.
    // FIXME: 12. Let attachedCallback be Get(prototype, "attachedCallback"). Rethrow any exceptions.
    // FIXME: 13. Let detachedCallback be Get(prototype, "detachedCallback"). Rethrow any exceptions.
    // FIXME: 14. Let attributeChangedCallback be Get(prototype, "attributeChangedCallback"). Rethrow any exceptions.

    PrivateName uniquePrivateName;
    globalObject()->putDirect(globalObject()->vm(), uniquePrivateName, object);

    QualifiedName name(nullAtom, tagName, HTMLNames::xhtmlNamespaceURI);
    definitions.addElementDefinition(JSCustomElementInterface::create(name, object, globalObject()));

    // FIXME: 17. Let map be registry's upgrade candidates map.
    // FIXME: 18. Upgrade a newly-defined element given map and definition.

    return jsUndefined();
}
#endif

} // namespace WebCore
