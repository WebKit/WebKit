/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "JSMockContentFilterSettings.h"

#if ENABLE(CONTENT_FILTERING)

#include "JSDOMBinding.h"
#include "MockContentFilterSettings.h"

using namespace JSC;

namespace WebCore {

JSValue JSMockContentFilterSettings::decisionPoint(ExecState&) const
{
    return jsNumber(static_cast<uint8_t>(wrapped().decisionPoint()));
}

void JSMockContentFilterSettings::setDecisionPoint(ExecState& state, JSValue value)
{
    uint8_t decisionPoint { toUInt8(&state, value, EnforceRange) };
    if (state.hadException())
        return;
    
    switch (decisionPoint) {
    case static_cast<uint8_t>(WebMockContentFilterDecisionPointAfterWillSendRequest):
    case static_cast<uint8_t>(WebMockContentFilterDecisionPointAfterRedirect):
    case static_cast<uint8_t>(WebMockContentFilterDecisionPointAfterResponse):
    case static_cast<uint8_t>(WebMockContentFilterDecisionPointAfterAddData):
    case static_cast<uint8_t>(WebMockContentFilterDecisionPointAfterFinishedAddingData):
    case static_cast<uint8_t>(WebMockContentFilterDecisionPointNever):
        wrapped().setDecisionPoint(static_cast<WebMockContentFilterDecisionPoint>(decisionPoint));
        return;
    default:
        throwTypeError(&state, String::format("%u is not a valid decisionPoint value.", decisionPoint));
    }
}

static inline WebMockContentFilterDecision toDecision(ExecState& state, JSValue value)
{
    uint8_t decision { toUInt8(&state, value, EnforceRange) };
    if (state.hadException())
        return WebMockContentFilterDecisionAllow;

    switch (decision) {
    case static_cast<uint8_t>(WebMockContentFilterDecisionAllow):
    case static_cast<uint8_t>(WebMockContentFilterDecisionBlock):
        return static_cast<WebMockContentFilterDecision>(decision);
    default:
        throwTypeError(&state, String::format("%u is not a valid decision value.", decision));
        return WebMockContentFilterDecisionAllow;
    }
}

JSValue JSMockContentFilterSettings::decision(ExecState&) const
{
    return jsNumber(static_cast<uint8_t>(wrapped().decision()));
}

void JSMockContentFilterSettings::setDecision(ExecState& state, JSValue value)
{
    WebMockContentFilterDecision decision { toDecision(state, value) };
    if (state.hadException())
        return;

    wrapped().setDecision(decision);
}

JSValue JSMockContentFilterSettings::unblockRequestDecision(ExecState&) const
{
    return jsNumber(static_cast<uint8_t>(wrapped().unblockRequestDecision()));
}

void JSMockContentFilterSettings::setUnblockRequestDecision(ExecState& state, JSValue value)
{
    WebMockContentFilterDecision unblockRequestDecision { toDecision(state, value) };
    if (state.hadException())
        return;

    wrapped().setUnblockRequestDecision(unblockRequestDecision);
}

}; // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)
