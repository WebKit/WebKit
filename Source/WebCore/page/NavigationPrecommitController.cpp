/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "NavigationPrecommitController.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "ExceptionOr.h"
#include "LocalDOMWindow.h"
#include "NavigateEvent.h"
#include "Navigation.h"
#include "NavigationDestination.h"
#include "NavigationInterceptHandler.h"
#include "NavigationNavigateOptions.h"
#include "SerializedScriptValue.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NavigationPrecommitController);

NavigationPrecommitController::NavigationPrecommitController(NavigateEvent& event)
    : m_event(event)
{
}

NavigationPrecommitController::~NavigationPrecommitController() = default;

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigationprecommitcontroller-redirect
ExceptionOr<void> NavigationPrecommitController::redirect(JSC::JSGlobalObject& globalObject, Document& document, const String& url, NavigationNavigateOptions&& options)
{
    Ref event = m_event;

    ASSERT(event->wasIntercepted());

    if (auto checkResult = event->sharedChecks(document); checkResult.hasException())
        return checkResult;

    if (event->interceptionState() != InterceptionState::Intercepted)
        return Exception { ExceptionCode::InvalidStateError, "Navigation has already been committed"_s };

    auto navigationType = event->navigationType();
    if (navigationType != NavigationNavigationType::Push && navigationType != NavigationNavigationType::Replace)
        return Exception { ExceptionCode::InvalidStateError, "redirect() can only be called for push or replace navigations"_s };

    URL destinationURL = document.parseURL(url);
    if (!destinationURL.isValid())
        return Exception { ExceptionCode::SyntaxError, "Invalid URL"_s };

    if (!Navigation::documentCanHaveURLRewritten(document, destinationURL))
        return Exception { ExceptionCode::SecurityError, "The document cannot have its URL rewritten to the destination URL"_s };

    if (options.history == NavigationHistoryBehavior::Push || options.history == NavigationHistoryBehavior::Replace)
        event->setNavigationType(options.history == NavigationHistoryBehavior::Push ? NavigationNavigationType::Push : NavigationNavigationType::Replace);

    if (!options.state.isUndefined()) {
        RefPtr window = document.window();
        if (!window)
            return Exception { ExceptionCode::InvalidStateError, "Invalid state"_s };

        Ref navigation = window->navigation();
        auto serializedState = navigation->serializeState(options.state);
        if (serializedState.hasException())
            return serializedState.releaseException();

        RefPtr state = serializedState.releaseReturnValue();
        event->destination().setStateObject(state);
        if (RefPtr tracker = navigation->ongoingAPIMethodTracker())
            tracker->setSerializedState(WTF::move(state));
    }

    event->destination().setURL(WTF::move(destinationURL));

    if (!options.info.isUndefined())
        event->setInfo(globalObject, options.info);

    return { };
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigationprecommitcontroller-addhandler
ExceptionOr<void> NavigationPrecommitController::addHandler(Document& document, Ref<NavigationInterceptHandler>&& handler)
{
    Ref event = m_event;

    ASSERT(event->wasIntercepted());

    if (auto checkResult = event->sharedChecks(document); checkResult.hasException())
        return checkResult;

    if (event->interceptionState() != InterceptionState::Intercepted)
        return Exception { ExceptionCode::InvalidStateError, "Navigation has already been committed"_s };

    event->handlers().append(WTF::move(handler));

    return { };
}

} // namespace WebCore
