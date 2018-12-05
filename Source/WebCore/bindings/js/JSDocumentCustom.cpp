/*
 * Copyright (C) 2007-2009, 2011, 2016, 2017 Apple Inc. All rights reserved.
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

#include "Frame.h"
#include "JSDOMWindowCustom.h"
#include "JSHTMLDocument.h"
#include "JSXMLDocument.h"
#include "NodeTraversal.h"
#include "SVGDocument.h"
#include <JavaScriptCore/HeapSnapshotBuilder.h>


namespace WebCore {
using namespace JSC;

static inline JSValue createNewDocumentWrapper(ExecState& state, JSDOMGlobalObject& globalObject, Ref<Document>&& passedDocument)
{
    auto& document = passedDocument.get();
    JSObject* wrapper;
    if (document.isHTMLDocument())
        wrapper = createWrapper<HTMLDocument>(&globalObject, WTFMove(passedDocument));
    else if (document.isXMLDocument())
        wrapper = createWrapper<XMLDocument>(&globalObject, WTFMove(passedDocument));
    else
        wrapper = createWrapper<Document>(&globalObject, WTFMove(passedDocument));

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

    auto* documentGlobalObject = toJSDOMWindow(state.vm(), toJS(&state, *window));
    if (!documentGlobalObject)
        return nullptr;

    // Creating a wrapper for domWindow might have created a wrapper for document as well.
    return getCachedWrapper(documentGlobalObject->world(), document);
}

void reportMemoryForDocumentIfFrameless(ExecState& state, Document& document)
{
    // Make sure the document is kept around by the window object, and works right with the back/forward cache.
    if (document.frame())
        return;

    VM& vm = state.vm();
    size_t memoryCost = 0;
    for (Node* node = &document; node; node = NodeTraversal::next(*node))
        memoryCost += node->approximateMemoryCost();

    // FIXME: Adopt reportExtraMemoryVisited, and switch to reportExtraMemoryAllocated.
    // https://bugs.webkit.org/show_bug.cgi?id=142595
    vm.heap.deprecatedReportExtraMemory(memoryCost);
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

void JSDocument::visitAdditionalChildren(SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(static_cast<ScriptExecutionContext*>(&wrapped()));
}

void JSDocument::heapSnapshot(JSCell* cell, HeapSnapshotBuilder& builder)
{
    Base::heapSnapshot(cell, builder);
    auto* thisObject = jsCast<JSDocument*>(cell);
    builder.setLabelForCell(cell, thisObject->wrapped().url().string());
}

} // namespace WebCore
