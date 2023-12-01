/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(KEYBOARD_LOCK)

#include "config.h"
#include "Keyboard.h"

#include "Document.h"
#include "Exception.h"
#include "JSDOMPromiseDeferred.h"
#include "KeyboardLockController.h"
#include "NavigatorBase.h"
#include "Page.h"
#include "ScriptExecutionContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Keyboard);

Ref<Keyboard> Keyboard::create(NavigatorBase& navigator)
{
    return adoptRef(*new Keyboard(navigator));
}

Keyboard::Keyboard(NavigatorBase& navigator)
    : m_navigator(navigator)
{
}

NavigatorBase* Keyboard::navigator()
{
    return m_navigator.get();
}

Keyboard::~Keyboard() = default;

void Keyboard::lock(const std::optional<Vector<String>>& keyCodes, DOMPromiseDeferred<void>&& promise)
{
    RefPtr context = m_navigator ? m_navigator->scriptExecutionContext() : nullptr;
    if (!context || !context->globalObject()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "The context is invalid"_s });
        return;
    }

    RefPtr document = dynamicDowncast<Document>(*context);
    if (document && !document->isFullyActive()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "The document is not fully active"_s });
        return;
    }

    if (document) {
        WeakPtr page = document->page();
        if (!page) {
            promise.reject(Exception { ExceptionCode::InvalidStateError, "The page does not exist"_s });
            return;
        }
        KeyboardLockController::shared().lock(*page, keyCodes.value(), WTFMove(promise));
    }
    return;
}

void Keyboard::unlock()
{
    RefPtr context = m_navigator ? m_navigator->scriptExecutionContext() : nullptr;
    if (!context || !context->globalObject())
        return;

    RefPtr document = dynamicDowncast<Document>(*context);
    if (document && !document->isFullyActive())
        return;

    if (document) {
        WeakPtr page = document->page();
        if (!page)
            return;

        KeyboardLockController::shared().unlock(*page);
    }
    return;
}

} // namespace WebCore

#endif // ENABLE(KEYBOARD_LOCK)
