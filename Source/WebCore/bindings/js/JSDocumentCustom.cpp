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

static inline JSValue createNewDocumentWrapper(ExecState& state, JSDOMGlobalObject& globalObject, Document& document)
{
    JSObject* wrapper;
    if (document.isHTMLDocument())
        wrapper = CREATE_DOM_WRAPPER(&globalObject, HTMLDocument, &document);
    else if (document.isSVGDocument())
        wrapper = CREATE_DOM_WRAPPER(&globalObject, SVGDocument, &document);
    else if (document.isXMLDocument())
        wrapper = CREATE_DOM_WRAPPER(&globalObject, XMLDocument, &document);
    else
        wrapper = CREATE_DOM_WRAPPER(&globalObject, Document, &document);

    // Make sure the document is kept around by the window object, and works right with the
    // back/forward cache.
    if (!document.frame()) {
        size_t nodeCount = 0;
        for (Node* n = &document; n; n = NodeTraversal::next(*n))
            ++nodeCount;

        // FIXME: Adopt reportExtraMemoryVisited, and switch to reportExtraMemoryAllocated.
        // https://bugs.webkit.org/show_bug.cgi?id=142595
        state.heap()->deprecatedReportExtraMemory(nodeCount * sizeof(Node));
    }

    return wrapper;
}

JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, Document* document)
{
    if (!document)
        return jsNull();

    JSObject* wrapper = getCachedWrapper(globalObject->world(), document);
    if (wrapper)
        return wrapper;

    if (DOMWindow* domWindow = document->domWindow()) {
        globalObject = toJSDOMWindow(toJS(state, domWindow));
        // Creating a wrapper for domWindow might have created a wrapper for document as well.
        wrapper = getCachedWrapper(globalObject->world(), document);
        if (wrapper)
            return wrapper;
    }

    return createNewDocumentWrapper(*state, *globalObject, *document);
}

JSValue toJSNewlyCreated(ExecState* state, JSDOMGlobalObject* globalObject, Document* document)
{
    return document ? createNewDocumentWrapper(*state, *globalObject, *document) : jsNull();
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
    RefPtr<TouchList> touchList = TouchList::create();

    for (size_t i = 0; i < state.argumentCount(); i++)
        touchList->append(JSTouch::toWrapped(state.argument(i)));

    return toJS(&state, globalObject(), touchList.release());
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
        return throwTypeError(&state, "The second argument must be a constructor");

    Document& document = wrapped();
    if (!document.domWindow()) {
        throwNotSupportedError(state, "Cannot define a custom element in a docuemnt without a browsing context");
        return jsUndefined();
    }

    switch (CustomElementDefinitions::checkName(tagName)) {
    case CustomElementDefinitions::NameStatus::Valid:
        break;
    case CustomElementDefinitions::NameStatus::ConflictsWithBuiltinNames:
        return throwSyntaxError(&state, "Custom element name cannot be same as one of the builtin elements");
    case CustomElementDefinitions::NameStatus::NoHyphen:
        return throwSyntaxError(&state, "Custom element name must contain a hyphen");
    case CustomElementDefinitions::NameStatus::ContainsUpperCase:
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
