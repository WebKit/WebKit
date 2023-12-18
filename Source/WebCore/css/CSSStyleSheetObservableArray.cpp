/*
 * Copyright (C) 2022, 2023 Apple Inc.  All rights reserved.
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

Ref<CSSStyleSheetObservableArray> CSSStyleSheetObservableArray::create(ContainerNode& treeScope)
{
    return adoptRef(*new CSSStyleSheetObservableArray(treeScope));
}

CSSStyleSheetObservableArray::CSSStyleSheetObservableArray(ContainerNode& treeScope)
    : m_treeScope(treeScope)
{
    ASSERT(is<Document>(treeScope) || is<ShadowRoot>(treeScope));
}

bool CSSStyleSheetObservableArray::setValueAt(JSC::JSGlobalObject* lexicalGlobalObject, unsigned index, JSC::JSValue value)
{
    auto& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(index <= m_sheets.size());

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

void CSSStyleSheetObservableArray::removeLast()
{
    RELEASE_ASSERT(!m_sheets.isEmpty());
    auto sheet = m_sheets.takeLast();
    willRemoveSheet(*sheet);
}

void CSSStyleSheetObservableArray::shrinkTo(unsigned length)
{
    RELEASE_ASSERT(length <= m_sheets.size());
    m_sheets.shrink(length);
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
        willRemoveSheet(*sheet);
    m_sheets = WTFMove(sheets);
    for (auto& sheet : m_sheets)
        didAddSheet(*sheet);

    return { };
}

std::optional<Exception> CSSStyleSheetObservableArray::shouldThrowWhenAddingSheet(const CSSStyleSheet& sheet) const
{
    if (!sheet.wasConstructedByJS())
        return Exception { ExceptionCode::NotAllowedError, "Sheet needs to be constructed by JavaScript"_s };
    auto* treeScope = this->treeScope();
    if (!treeScope || sheet.constructorDocument() != &treeScope->documentScope())
        return Exception { ExceptionCode::NotAllowedError, "Sheet constructor document doesn't match"_s };
    return std::nullopt;
}

TreeScope* CSSStyleSheetObservableArray::treeScope() const
{
    if (!m_treeScope)
        return nullptr;
    if (auto* shadowRoot = dynamicDowncast<ShadowRoot>(*m_treeScope))
        return shadowRoot;
    return &downcast<Document>(*m_treeScope);
}

void CSSStyleSheetObservableArray::didAddSheet(CSSStyleSheet& sheet)
{
    if (m_treeScope)
        sheet.addAdoptingTreeScope(*m_treeScope);
}

void CSSStyleSheetObservableArray::willRemoveSheet(CSSStyleSheet& sheet)
{
    if (m_treeScope)
        sheet.removeAdoptingTreeScope(*m_treeScope);
}

} // namespace WebCore
