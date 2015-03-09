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

// Must be kept in sync with values in MockContentFilterSettings.idl.
const uint8_t decisionPointAfterResponse = 0;
const uint8_t decisionPointAfterAddData = 1;
const uint8_t decisionPointAfterFinishedAddingData = 2;
const uint8_t decisionAllow = 0;
const uint8_t decisionBlock = 1;

JSC::JSValue JSMockContentFilterSettings::decisionPoint(JSC::ExecState*) const
{
    switch (impl().decisionPoint()) {
    case MockContentFilterSettings::DecisionPoint::AfterResponse:
        return jsNumber(decisionPointAfterResponse);
    case MockContentFilterSettings::DecisionPoint::AfterAddData:
        return jsNumber(decisionPointAfterAddData);
    case MockContentFilterSettings::DecisionPoint::AfterFinishedAddingData:
        return jsNumber(decisionPointAfterFinishedAddingData);
    }

    ASSERT_NOT_REACHED();
    return { };
}

void JSMockContentFilterSettings::setDecisionPoint(JSC::ExecState* exec, JSC::JSValue value)
{
    uint8_t nativeValue { toUInt8(exec, value, EnforceRange) };
    if (exec->hadException())
        return;

    switch (nativeValue) {
    case decisionPointAfterResponse:
        impl().setDecisionPoint(MockContentFilterSettings::DecisionPoint::AfterResponse);
        return;
    case decisionPointAfterAddData:
        impl().setDecisionPoint(MockContentFilterSettings::DecisionPoint::AfterAddData);
        return;
    case decisionPointAfterFinishedAddingData:
        impl().setDecisionPoint(MockContentFilterSettings::DecisionPoint::AfterFinishedAddingData);
        return;
    default:
        throwTypeError(exec, String::format("%u is not a valid decisionPoint value.", nativeValue));
    }
}

JSC::JSValue JSMockContentFilterSettings::decision(JSC::ExecState*) const
{
    switch (impl().decision()) {
    case MockContentFilterSettings::Decision::Allow:
        return jsNumber(decisionAllow);
    case MockContentFilterSettings::Decision::Block:
        return jsNumber(decisionBlock);
    }

    ASSERT_NOT_REACHED();
    return { };
}

void JSMockContentFilterSettings::setDecision(JSC::ExecState* exec, JSC::JSValue value)
{
    uint8_t nativeValue { toUInt8(exec, value, EnforceRange) };
    if (exec->hadException())
        return;

    switch (nativeValue) {
    case decisionAllow:
        impl().setDecision(MockContentFilterSettings::Decision::Allow);
        return;
    case decisionBlock:
        impl().setDecision(MockContentFilterSettings::Decision::Block);
        return;
    default:
        throwTypeError(exec, String::format("%u is not a valid decision value.", nativeValue));
    }
}

}; // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)
