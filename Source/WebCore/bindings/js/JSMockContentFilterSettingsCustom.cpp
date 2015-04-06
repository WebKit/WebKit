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

using Decision = MockContentFilterSettings::Decision;
using DecisionPoint = MockContentFilterSettings::DecisionPoint;

// Must be kept in sync with values in MockContentFilterSettings.idl.
const uint8_t decisionPointAfterWillSendRequest = 0;
const uint8_t decisionPointAfterRedirect = 1;
const uint8_t decisionPointAfterResponse = 2;
const uint8_t decisionPointAfterAddData = 3;
const uint8_t decisionPointAfterFinishedAddingData = 4;
const uint8_t decisionAllow = 0;
const uint8_t decisionBlock = 1;

JSValue JSMockContentFilterSettings::decisionPoint(ExecState*) const
{
    switch (impl().decisionPoint()) {
    case DecisionPoint::AfterWillSendRequest:
        return jsNumber(decisionPointAfterWillSendRequest);
    case DecisionPoint::AfterRedirect:
        return jsNumber(decisionPointAfterRedirect);
    case DecisionPoint::AfterResponse:
        return jsNumber(decisionPointAfterResponse);
    case DecisionPoint::AfterAddData:
        return jsNumber(decisionPointAfterAddData);
    case DecisionPoint::AfterFinishedAddingData:
        return jsNumber(decisionPointAfterFinishedAddingData);
    }

    ASSERT_NOT_REACHED();
    return jsUndefined();
}

void JSMockContentFilterSettings::setDecisionPoint(ExecState* exec, JSValue value)
{
    uint8_t nativeValue { toUInt8(exec, value, EnforceRange) };
    if (exec->hadException())
        return;

    switch (nativeValue) {
    case decisionPointAfterWillSendRequest:
        impl().setDecisionPoint(DecisionPoint::AfterWillSendRequest);
        return;
    case decisionPointAfterRedirect:
        impl().setDecisionPoint(DecisionPoint::AfterRedirect);
        return;
    case decisionPointAfterResponse:
        impl().setDecisionPoint(DecisionPoint::AfterResponse);
        return;
    case decisionPointAfterAddData:
        impl().setDecisionPoint(DecisionPoint::AfterAddData);
        return;
    case decisionPointAfterFinishedAddingData:
        impl().setDecisionPoint(DecisionPoint::AfterFinishedAddingData);
        return;
    }

    throwTypeError(exec, String::format("%u is not a valid decisionPoint value.", nativeValue));
}

static inline JSValue toJSValue(Decision decision)
{
    switch (decision) {
    case Decision::Allow:
        return jsNumber(decisionAllow);
    case Decision::Block:
        return jsNumber(decisionBlock);
    }

    ASSERT_NOT_REACHED();
    return jsUndefined();
}

static inline Decision toDecision(ExecState* exec, JSValue value)
{
    uint8_t nativeValue { toUInt8(exec, value, EnforceRange) };
    if (exec->hadException())
        return Decision::Allow;

    switch (nativeValue) {
    case decisionAllow:
        return Decision::Allow;
    case decisionBlock:
        return Decision::Block;
    }

    throwTypeError(exec, String::format("%u is not a valid decision value.", nativeValue));
    return Decision::Allow;
}

JSValue JSMockContentFilterSettings::decision(ExecState*) const
{
    return toJSValue(impl().decision());
}

void JSMockContentFilterSettings::setDecision(ExecState* exec, JSValue value)
{
    Decision decision { toDecision(exec, value) };
    if (exec->hadException())
        return;

    impl().setDecision(decision);
}

JSValue JSMockContentFilterSettings::unblockRequestDecision(ExecState*) const
{
    return toJSValue(impl().unblockRequestDecision());
}

void JSMockContentFilterSettings::setUnblockRequestDecision(ExecState* exec, JSValue value)
{
    Decision unblockRequestDecision { toDecision(exec, value) };
    if (exec->hadException())
        return;

    impl().setUnblockRequestDecision(unblockRequestDecision);
}

}; // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)
