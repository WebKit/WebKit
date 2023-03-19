/*
 * Copyright (C) 2007-2021 Apple Inc. All rights reserved.
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

#include "FrameDestructionObserverInlines.h"
#include "JSCSSStyleSheet.h"
#include "JSDOMConvert.h"
#include "JSDOMGlobalObjectInlines.h"
#include "JSHTMLDocument.h"
#include "JSLocalDOMWindowCustom.h"
#include "JSXMLDocument.h"
#include "LocalFrame.h"
#include "NodeTraversal.h"
#include "WebCoreOpaqueRootInlines.h"
#include <JavaScriptCore/HeapAnalyzer.h>

namespace WebCore {
using namespace JSC;

static inline JSValue createNewDocumentWrapper(JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, Ref<Document>&& passedDocument)
{
    auto& document = passedDocument.get();
    JSObject* wrapper;
    if (document.isHTMLDocument())
        wrapper = createWrapper<HTMLDocument>(&globalObject, WTFMove(passedDocument));
    else if (document.isXMLDocument())
        wrapper = createWrapper<XMLDocument>(&globalObject, WTFMove(passedDocument));
    else
        wrapper = createWrapper<Document>(&globalObject, WTFMove(passedDocument));

    reportMemoryForDocumentIfFrameless(lexicalGlobalObject, document);

    return wrapper;
}

JSObject* cachedDocumentWrapper(JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, Document& document)
{
    if (auto* wrapper = getCachedWrapper(globalObject.world(), document))
        return wrapper;

    auto* window = document.domWindow();
    if (!window)
        return nullptr;

    auto* documentGlobalObject = toJSDOMGlobalObject<JSLocalDOMWindow>(lexicalGlobalObject.vm(), toJS(&lexicalGlobalObject, *window));
    if (!documentGlobalObject)
        return nullptr;

    // Creating a wrapper for domWindow might have created a wrapper for document as well.
    return getCachedWrapper(documentGlobalObject->world(), document);
}

void reportMemoryForDocumentIfFrameless(JSGlobalObject& lexicalGlobalObject, Document& document)
{
    // Make sure the document is kept around by the window object, and works right with the back/forward cache.
    if (document.frame())
        return;

    VM& vm = lexicalGlobalObject.vm();
    size_t memoryCost = 0;
    for (Node* node = &document; node; node = NodeTraversal::next(*node))
        memoryCost += node->approximateMemoryCost();

    // FIXME: Adopt reportExtraMemoryVisited, and switch to reportExtraMemoryAllocated.
    // https://bugs.webkit.org/show_bug.cgi?id=142595
    vm.heap.deprecatedReportExtraMemory(memoryCost);
}

JSValue toJSNewlyCreated(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, Ref<Document>&& document)
{
    return createNewDocumentWrapper(*lexicalGlobalObject, *globalObject, WTFMove(document));
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, Document& document)
{
    if (auto* wrapper = cachedDocumentWrapper(*lexicalGlobalObject, *globalObject, document))
        return wrapper;
    return toJSNewlyCreated(lexicalGlobalObject, globalObject, Ref<Document>(document));
}

void setAdoptedStyleSheetsOnTreeScope(TreeScope& treeScope, JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto nativeValue = convert<IDLFrozenArray<IDLInterface<CSSStyleSheet>>>(lexicalGlobalObject, value);
    RETURN_IF_EXCEPTION(throwScope, void());
    invokeFunctorPropagatingExceptionIfNecessary(lexicalGlobalObject, throwScope, [&] {
        return treeScope.setAdoptedStyleSheets(WTFMove(nativeValue));
    });
}

void JSDocument::setAdoptedStyleSheets(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
{
    setAdoptedStyleSheetsOnTreeScope(wrapped(), lexicalGlobalObject, value);
}

template<typename Visitor>
void JSDocument::visitAdditionalChildren(Visitor& visitor)
{
    addWebCoreOpaqueRoot(visitor, static_cast<ScriptExecutionContext&>(wrapped()));
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSDocument);

void JSDocument::analyzeHeap(JSCell* cell, HeapAnalyzer& analyzer)
{
    Base::analyzeHeap(cell, analyzer);
    auto* thisObject = jsCast<JSDocument*>(cell);
    analyzer.setLabelForCell(cell, thisObject->wrapped().url().string());
}

} // namespace WebCore
