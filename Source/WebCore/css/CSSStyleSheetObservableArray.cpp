/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
#include "CSSStyleSheetObservableArray.h"

#include "Document.h"
#include "JSCSSStyleSheet.h"
#include "JSDOMConvert.h"
#include "ShadowRoot.h"
#include "TreeScope.h"

namespace WebCore {

Ref<CSSStyleSheetObservableArray> CSSStyleSheetObservableArray::create(Document& document)
{
    return adoptRef(*new CSSStyleSheetObservableArray(document));
}

Ref<CSSStyleSheetObservableArray> CSSStyleSheetObservableArray::create(ShadowRoot& shadowRoot)
{
    return adoptRef(*new CSSStyleSheetObservableArray(shadowRoot));
}

CSSStyleSheetObservableArray::CSSStyleSheetObservableArray(Document& document)
    : m_treeScope(WeakPtr { document })
{
}

CSSStyleSheetObservableArray::CSSStyleSheetObservableArray(ShadowRoot& shadowRoot)
    : m_treeScope(WeakPtr { shadowRoot })
{
}

void CSSStyleSheetObservableArray::willDestroyTreeScope()
{
    for (auto& sheet : m_sheets)
        willRemoveSheet(*sheet, CSSStyleSheet::IsTreeScopeBeingDestroyed::Yes);
    m_treeScope = nullptr;
}

bool CSSStyleSheetObservableArray::setValueAt(JSC::JSGlobalObject* lexicalGlobalObject, unsigned index, JSC::JSValue value)
{
    auto& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (index > m_sheets.size())
        return false;

    RefPtr sheet = convert<IDLInterface<CSSStyleSheet>>(*lexicalGlobalObject, value);
    if (!sheet)
        return false;

    if (auto exception = shouldThrowWhenAddingSheet(*sheet)) {
        throwException(lexicalGlobalObject, scope, createDOMException(*lexicalGlobalObject, WTFMove(*exception)));
        return false;
    }

    if (index == m_sheets.size())
        m_sheets.append(sheet.copyRef());
    else
        m_sheets[index] = sheet.copyRef();

    didAddSheet(*sheet);
    return true;
}

bool CSSStyleSheetObservableArray::deleteValueAt(JSC::JSGlobalObject*, unsigned index)
{
    if (index >= m_sheets.size())
        return false;

    auto sheet = std::exchange(m_sheets[index], nullptr);
    m_sheets.remove(index);
    willRemoveSheet(*sheet, CSSStyleSheet::IsTreeScopeBeingDestroyed::No);
    return true;
}

JSC::JSValue CSSStyleSheetObservableArray::valueAt(JSC::JSGlobalObject* lexicalGlobalObject, unsigned index) const
{
    if (index >= m_sheets.size())
        return JSC::jsUndefined();
    return toJS(lexicalGlobalObject, JSC::jsCast<JSDOMGlobalObject*>(lexicalGlobalObject), m_sheets[index].get());
}

ExceptionOr<void> CSSStyleSheetObservableArray::setSheets(Vector<RefPtr<CSSStyleSheet>>&& sheets)
{
    for (auto& sheet : sheets) {
        if (auto exception = shouldThrowWhenAddingSheet(*sheet))
            return WTFMove(*exception);
    }

    for (auto& sheet : m_sheets)
        willRemoveSheet(*sheet, CSSStyleSheet::IsTreeScopeBeingDestroyed::No);
    m_sheets = WTFMove(sheets);
    for (auto& sheet : m_sheets)
        didAddSheet(*sheet);

    return { };
}

std::optional<Exception> CSSStyleSheetObservableArray::shouldThrowWhenAddingSheet(const CSSStyleSheet& sheet) const
{
    if (!sheet.wasConstructedByJS())
        return Exception { NotAllowedError, "Sheet needs to be constructed by JavaScript"_s };
    auto* treeScope = this->treeScope();
    if (!treeScope || sheet.constructorDocument() != &treeScope->documentScope())
        return Exception { NotAllowedError, "Sheet constructor document doesn't match"_s };
    return std::nullopt;
}

TreeScope* CSSStyleSheetObservableArray::treeScope() const
{
    return WTF::switchOn(m_treeScope, [](const WeakPtr<Document, WeakPtrImplWithEventTargetData>& document) -> TreeScope* {
        return document.get();
    }, [](const WeakPtr<ShadowRoot, WeakPtrImplWithEventTargetData>& shadowRoot) -> TreeScope* {
        return shadowRoot.get();
    }, [](std::nullptr_t) -> TreeScope* {
        return nullptr;
    });
}

void CSSStyleSheetObservableArray::didAddSheet(CSSStyleSheet& sheet)
{
    WTF::switchOn(m_treeScope, [&](const WeakPtr<Document, WeakPtrImplWithEventTargetData>& document) {
        if (document)
            sheet.addAdoptingTreeScope(*document);
    }, [&](const WeakPtr<ShadowRoot, WeakPtrImplWithEventTargetData>& shadowRoot) {
        if (shadowRoot)
            sheet.addAdoptingTreeScope(*shadowRoot);
    }, [](std::nullptr_t) { });
}

void CSSStyleSheetObservableArray::willRemoveSheet(CSSStyleSheet& sheet, CSSStyleSheet::IsTreeScopeBeingDestroyed isTreeScopeBeingDestroyed)
{
    WTF::switchOn(m_treeScope, [&](const WeakPtr<Document, WeakPtrImplWithEventTargetData>& document) {
        if (document)
            sheet.removeAdoptingTreeScope(*document, isTreeScopeBeingDestroyed);
    }, [&](const WeakPtr<ShadowRoot, WeakPtrImplWithEventTargetData>& shadowRoot) {
        if (shadowRoot)
            sheet.removeAdoptingTreeScope(*shadowRoot, isTreeScopeBeingDestroyed);
    }, [](std::nullptr_t) { });
}

} // namespace WebCore
