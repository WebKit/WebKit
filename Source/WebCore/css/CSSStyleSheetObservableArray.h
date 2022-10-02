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

#pragma once

#include "CSSStyleSheet.h"
#include "ExceptionOr.h"
#include "JSObservableArray.h"

namespace WebCore {

class Document;
class ShadowRoot;
class TreeScope;

class CSSStyleSheetObservableArray : public JSC::ObservableArray {
public:
    static Ref<CSSStyleSheetObservableArray> create(Document&);
    static Ref<CSSStyleSheetObservableArray> create(ShadowRoot&);

    ExceptionOr<void> setSheets(Vector<RefPtr<CSSStyleSheet>>&&);
    const Vector<RefPtr<CSSStyleSheet>>& sheets() const { return m_sheets; }

    void willDestroyTreeScope();

private:
    explicit CSSStyleSheetObservableArray(Document&);
    explicit CSSStyleSheetObservableArray(ShadowRoot&);

    std::optional<Exception> shouldThrowWhenAddingSheet(const CSSStyleSheet&) const;

    // JSC::ObservableArray
    bool setValueAt(JSC::JSGlobalObject*, unsigned index, JSC::JSValue) final;
    bool deleteValueAt(JSC::JSGlobalObject*, unsigned index) final;
    JSC::JSValue valueAt(JSC::JSGlobalObject*, unsigned index) const final;
    unsigned length() const final { return m_sheets.size(); }

    TreeScope* treeScope() const;

    void didAddSheet(CSSStyleSheet&);
    void willRemoveSheet(CSSStyleSheet&, CSSStyleSheet::IsTreeScopeBeingDestroyed);

    std::variant<WeakPtr<Document, WeakPtrImplWithEventTargetData>, WeakPtr<ShadowRoot, WeakPtrImplWithEventTargetData>, std::nullptr_t> m_treeScope;
    Vector<RefPtr<CSSStyleSheet>> m_sheets;
};

} // namespace WebCore
