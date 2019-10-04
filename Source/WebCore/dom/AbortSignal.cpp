/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "AbortSignal.h"

#include "Event.h"
#include "EventNames.h"
#include "ScriptExecutionContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AbortSignal);

Ref<AbortSignal> AbortSignal::create(ScriptExecutionContext& context)
{
    return adoptRef(*new AbortSignal(context));
}

AbortSignal::AbortSignal(ScriptExecutionContext& context)
    : ContextDestructionObserver(&context)
{
}

// https://dom.spec.whatwg.org/#abortsignal-signal-abort
void AbortSignal::abort()
{
    // 1. If signal's aborted flag is set, then return.
    if (m_aborted)
        return;
    
    // 2. Set signal’s aborted flag.
    m_aborted = true;

    auto protectedThis = makeRef(*this);
    auto algorithms = WTFMove(m_algorithms);
    for (auto& algorithm : algorithms)
        algorithm();

    // 5. Fire an event named abort at signal.
    dispatchEvent(Event::create(eventNames().abortEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

// https://dom.spec.whatwg.org/#abortsignal-follow
void AbortSignal::follow(AbortSignal& signal)
{
    if (aborted())
        return;

    if (signal.aborted()) {
        abort();
        return;
    }

    ASSERT(!m_followingSignal);
    m_followingSignal = makeWeakPtr(signal);
    signal.addAlgorithm([weakThis = makeWeakPtr(this)] {
        if (!weakThis)
            return;
        weakThis->abort();
    });
}

}
