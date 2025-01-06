/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "WindowOrWorkerGlobalScopeTrustedTypes.h"

#include "Document.h"
#include "LocalDOMWindow.h"
#include "LocalDOMWindowProperty.h"
#include "LocalFrame.h"
#include "Page.h"
#include "TrustedTypePolicyFactory.h"
#include "WorkerGlobalScope.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WorkerGlobalScopeTrustedTypes);

class DOMWindowTrustedTypes : public Supplement<LocalDOMWindow>, public LocalDOMWindowProperty {
    WTF_MAKE_TZONE_ALLOCATED(DOMWindowTrustedTypes);
public:
    explicit DOMWindowTrustedTypes(LocalDOMWindow&);
    virtual ~DOMWindowTrustedTypes() = default;

    static DOMWindowTrustedTypes* from(LocalDOMWindow&);
    TrustedTypePolicyFactory* trustedTypes() const;

private:
    static WTF::ASCIILiteral supplementName() { return "DOMWindowTrustedTypes"_s; }

    mutable RefPtr<TrustedTypePolicyFactory> m_trustedTypes;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(DOMWindowTrustedTypes);

DOMWindowTrustedTypes::DOMWindowTrustedTypes(LocalDOMWindow& window)
    : LocalDOMWindowProperty(&window)
{
}

DOMWindowTrustedTypes* DOMWindowTrustedTypes::from(LocalDOMWindow& window)
{
    auto* supplement = static_cast<DOMWindowTrustedTypes*>(Supplement<LocalDOMWindow>::from(&window, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<DOMWindowTrustedTypes>(window);
        supplement = newSupplement.get();
        provideTo(&window, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

TrustedTypePolicyFactory* DOMWindowTrustedTypes::trustedTypes() const
{
    if (!m_trustedTypes)
        m_trustedTypes = TrustedTypePolicyFactory::create(*window()->document());
    return m_trustedTypes.get();
}

TrustedTypePolicyFactory* WindowOrWorkerGlobalScopeTrustedTypes::trustedTypes(DOMWindow& window)
{
    RefPtr localWindow = dynamicDowncast<LocalDOMWindow>(window);
    if (!localWindow)
        return nullptr;
    return DOMWindowTrustedTypes::from(*localWindow)->trustedTypes();
}

WorkerGlobalScopeTrustedTypes::WorkerGlobalScopeTrustedTypes(WorkerGlobalScope& scope)
    : m_scope(scope)
{
}

WorkerGlobalScopeTrustedTypes* WorkerGlobalScopeTrustedTypes::from(WorkerGlobalScope& scope)
{
    auto* supplement = static_cast<WorkerGlobalScopeTrustedTypes*>(Supplement<WorkerGlobalScope>::from(&scope, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<WorkerGlobalScopeTrustedTypes>(scope);
        supplement = newSupplement.get();
        provideTo(&scope, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

WorkerGlobalScopeTrustedTypes::~WorkerGlobalScopeTrustedTypes() = default;

void WorkerGlobalScopeTrustedTypes::prepareForDestruction()
{
    m_trustedTypes = nullptr;
    m_scope = nullptr;
}

TrustedTypePolicyFactory* WorkerGlobalScopeTrustedTypes::trustedTypes() const
{
    if (!m_trustedTypes && m_scope)
        m_trustedTypes = TrustedTypePolicyFactory::create(Ref { *m_scope });
    return m_trustedTypes.get();
}

ASCIILiteral WindowOrWorkerGlobalScopeTrustedTypes::workerGlobalSupplementName()
{
    return "WorkerGlobalScopeTrustedTypes"_s;
}

TrustedTypePolicyFactory* WindowOrWorkerGlobalScopeTrustedTypes::trustedTypes(WorkerGlobalScope& scope)
{
    return WorkerGlobalScopeTrustedTypes::from(scope)->trustedTypes();
}

} // namespace WebCore
