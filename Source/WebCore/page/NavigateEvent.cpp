/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
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
#include "NavigateEvent.h"

#include "AbortController.h"
#include "ExceptionCode.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(NavigateEvent);

NavigateEvent::NavigateEvent(const AtomString& type, const NavigateEvent::Init& init, AbortController* abortController)
    : Event(EventInterfaceType::NavigateEvent, type, init, Event::IsTrusted::Yes)
    , m_navigationType(init.navigationType)
    , m_destination(init.destination)
    , m_signal(init.signal)
    , m_formData(init.formData)
    , m_downloadRequest(init.downloadRequest)
    , m_info(init.info)
    , m_canIntercept(init.canIntercept)
    , m_userInitiated(init.userInitiated)
    , m_hashChange(init.hashChange)
    , m_hasUAVisualTransition(init.hasUAVisualTransition)
    , m_abortController(abortController)
{
}

Ref<NavigateEvent> NavigateEvent::create(const AtomString& type, const NavigateEvent::Init& init, AbortController* abortController)
{
    return adoptRef(*new NavigateEvent(type, init, abortController));
}

Ref<NavigateEvent> NavigateEvent::create(const AtomString& type, const NavigateEvent::Init& init)
{
    // FIXME: AbortController is required but JS bindings need to create it with one.
    return adoptRef(*new NavigateEvent(type, init, nullptr));
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigateevent-perform-shared-checks
ExceptionOr<void> NavigateEvent::sharedChecks(Document& document)
{
    if (!document.isFullyActive())
        return Exception { ExceptionCode::InvalidStateError, "Document is not fully active"_s };

    if (!isTrusted())
        return Exception { ExceptionCode::SecurityError, "Event is not trusted"_s };

    if (defaultPrevented())
        return Exception { ExceptionCode::InvalidStateError, "Event was already canceled"_s };

    return { };
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigateevent-intercept
ExceptionOr<void> NavigateEvent::intercept(Document& document, NavigationInterceptOptions&& options)
{
    if (auto checkResult = sharedChecks(document); checkResult.hasException())
        return checkResult;

    if (!canIntercept())
        return Exception { ExceptionCode::SecurityError, "Event is not interceptable"_s };

    if (!isBeingDispatched())
        return Exception { ExceptionCode::InvalidStateError, "Event is not being dispatched"_s };

    ASSERT(!m_interceptionState || m_interceptionState == InterceptionState::Intercepted);

    if (options.handler)
        m_handlers.append(options.handler.releaseNonNull());

    if (options.focusReset) {
        // FIXME: Print warning to console if it was already set.
        m_focusReset = options.focusReset;
    }

    if (options.scroll) {
        // FIXME: Print warning to console if it was already set.
        m_scrollBehavior = options.scroll;
    }

    m_interceptionState = InterceptionState::Intercepted;

    return { };
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigateevent-scroll
ExceptionOr<void> NavigateEvent::scroll(Document& document)
{
    auto checkResult = sharedChecks(document);
    if (checkResult.hasException())
        return checkResult;

    if (m_interceptionState != InterceptionState::Committed)
        return Exception { ExceptionCode::InvalidStateError, "Interception has not been committed"_s };

    // FIXME: Scroll document: https://html.spec.whatwg.org/multipage/nav-history-apis.html#process-scroll-behavior

    return { };
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigateevent-finish
void NavigateEvent::finish()
{
    ASSERT(m_interceptionState != InterceptionState::Intercepted && m_interceptionState != InterceptionState::Finished);
    if (!m_interceptionState)
        return;

    // FIXME: 3. Potentially reset the focus
    // FIXME: 4. If didFulfill is true, then potentially process scroll behavior given event.

    m_interceptionState = InterceptionState::Finished;
}

} // namespace WebCore
