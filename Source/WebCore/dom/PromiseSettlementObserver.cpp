/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PromiseSettlementObserver.h"

#include "Document.h"
#include "DocumentEventLoop.h"
#include "JSDOMPromise.h"
#include "ScriptExecutionContextInlines.h"

namespace WebCore {

Ref<PromiseSettlementObserver> PromiseSettlementObserver::create(SuccessSteps&& successSteps, FailureSteps&& failureSteps)
{
    return adoptRef(*new PromiseSettlementObserver(WTF::move(successSteps), WTF::move(failureSteps)));
}

PromiseSettlementObserver::PromiseSettlementObserver(SuccessSteps&& successSteps, FailureSteps&& failureSteps)
    : m_successSteps(WTF::move(successSteps))
    , m_failureSteps(WTF::move(failureSteps))
{
}

// https://webidl.spec.whatwg.org/#wait-for-all
void PromiseSettlementObserver::waitForAll(Document& document, const Vector<Ref<DOMPromise>>& promises)
{
    ASSERT(document.isFullyActive());
    ASSERT(!m_settled);
    ASSERT(!m_totalPromises);

    for (const auto& promise : promises) {
        if (registerPromise(promise))
            m_totalPromises++;
    }

    // Step 3 / 6.1 Queue a microtask to perform successSteps given « ».
    if (!m_totalPromises) {
        protect(document.eventLoop())->queueMicrotask(document.vm(), [protectThis = Ref { *this }]() {
            protectThis->resolve();
        });
    }
}

void PromiseSettlementObserver::resolve()
{
    if (m_settled)
        return;
    m_settled = true;
    m_successSteps();
}

void PromiseSettlementObserver::reject(JSC::JSValue result)
{
    if (m_settled)
        return;
    m_settled = true;
    m_failureSteps(result);
}

void PromiseSettlementObserver::handleResult(bool isFulfilled, JSC::JSValue result)
{
    if (m_settled)
        return;

    ASSERT(isWaiting());
    if (!isFulfilled)
        return reject(result);

    ++m_settledPromises;

    if (!isWaiting())
        resolve();
}

bool PromiseSettlementObserver::registerPromise(DOMPromise& promise)
{
    auto handler = [protectThis = Ref { *this }](auto* globalObject, bool isFulfilled, auto result) mutable {
        RefPtr context = globalObject ? globalObject->scriptExecutionContext() : nullptr;
        if (!context || context->activeDOMObjectsAreSuspended() || context->activeDOMObjectsAreStopped())
            return;

        protectThis->handleResult(isFulfilled, result);
    };

    return promise.whenSettledWithResult(WTF::move(handler)) == DOMPromise::IsCallbackRegistered::Yes;
}

} // namespace WebCore
