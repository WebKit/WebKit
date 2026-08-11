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

#pragma once

#include "JSAsyncFunctionGenerator.h"
#include "JSAsyncGenerator.h"
#include "JSInternalFieldObjectImplInlines.h"
#include "JSModuleRecord.h"
#include "JSPromise.h"
#include "JSPromiseReaction.h"

namespace JSC {

ALWAYS_INLINE void JSAsyncGenerator::enqueue(VM& vm, JSValue value, int32_t mode, JSObject* settlementTarget)
{
    ASSERT(settlementTarget->inherits<JSPromise>() || settlementTarget->inherits<JSAsyncGenerator>() || settlementTarget->inherits<JSAsyncFunctionGenerator>() || settlementTarget->inherits<JSModuleRecord>());
    if (isQueueEmpty()) [[likely]] {
        setResumeValue(vm, value);
        setResumeMode(mode);
        setResumePromise(vm, settlementTarget);
        return;
    }

    JSValue last = queue();
    if (last.isNull()) {
        auto* item = JSSlimPromiseReaction::createAsyncGeneratorRequest(
            vm,
            settlementTarget,
            value,
            static_cast<uint8_t>(mode),
            nullptr // Will be set to self (next)
        );
        item->setNext(vm, item);
        setQueue(vm, item);
    } else {
        auto* tail = uncheckedDowncast<JSSlimPromiseReaction>(last);
        auto* head = uncheckedDowncast<JSSlimPromiseReaction>(tail->next());
        auto* item = JSSlimPromiseReaction::createAsyncGeneratorRequest(
            vm,
            settlementTarget,
            value,
            static_cast<uint8_t>(mode),
            head // next = head (to maintain circular)
        );
        tail->setNext(vm, item);
        setQueue(vm, item);
    }
}

ALWAYS_INLINE JSObject* JSAsyncGenerator::dequeue(VM& vm)
{
    ASSERT(!isQueueEmpty());

    JSValue settlementTarget = resumePromise();

    JSValue last = queue();
    if (last.isNull()) {
        setResumeMode(static_cast<int32_t>(AsyncGeneratorResumeMode::Empty));
        setResumeValue(vm, jsUndefined());
        setResumePromise(vm, jsUndefined());
    } else {
        auto* tail = uncheckedDowncast<JSSlimPromiseReaction>(last);
        auto* head = uncheckedDowncast<JSSlimPromiseReaction>(tail->next());

        setResumePromise(vm, head->promise());
        setResumeValue(vm, head->handlerOrContext());
        setResumeMode(head->asyncGeneratorResumeMode());

        if (head == tail)
            setQueue(vm, jsNull());
        else {
            auto* newHead = uncheckedDowncast<JSSlimPromiseReaction>(head->next());
            tail->setNext(vm, newHead);
        }
    }

    return asObject(settlementTarget);
}

} // namespace JSC
