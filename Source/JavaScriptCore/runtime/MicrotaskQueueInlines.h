/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/Debugger.h>
#include <JavaScriptCore/JSCellInlines.h>
#include <JavaScriptCore/JSMicrotaskDispatcher.h>
#include <JavaScriptCore/MicrotaskQueue.h>
#include <JavaScriptCore/TopExceptionScope.h>
#include <JavaScriptCore/VMEntryScopeInlines.h>

namespace JSC {

inline JSCell* QueuedTask::dispatcher() const
{
    return std::bit_cast<JSCell*>(std::bit_cast<uintptr_t>(m_dispatcher.pointer()) & ~isJSMicrotaskDispatcherFlag);
}

inline JSGlobalObject* QueuedTask::globalObject() const
{
    if (isJSMicrotaskDispatcher()) [[unlikely]]
        return jsCast<JSMicrotaskDispatcher*>(dispatcher())->globalObject();
    return jsCast<JSGlobalObject*>(dispatcher());
}

inline JSMicrotaskDispatcher* QueuedTask::jsMicrotaskDispatcher() const
{
    if (isJSMicrotaskDispatcher()) [[unlikely]]
        return jsCast<JSMicrotaskDispatcher*>(dispatcher());
    return nullptr;
}

inline std::optional<MicrotaskIdentifier> QueuedTask::identifier() const
{
    auto* dispatcher = jsMicrotaskDispatcher();
    if (!dispatcher)
        return std::nullopt;
    return MicrotaskIdentifier { std::bit_cast<uintptr_t>(dispatcher) };
}

inline void MicrotaskQueue::enqueue(QueuedTask&& task)
{
    if (task.isJSMicrotaskDispatcher()) [[unlikely]] {
        enqueueSlow(WTF::move(task));
        return;
    }
    m_queue.enqueue(WTF::move(task));
    if (!m_isScheduledToRun) [[unlikely]]
        scheduleToRunIfNeeded();
}

template<bool useCallOnEachMicrotask>
inline void MicrotaskQueue::performMicrotaskCheckpoint(VM& vm, NOESCAPE const Invocable<void(JSGlobalObject*, JSGlobalObject*)> auto& globalObjectSwitchCallback)
{
    auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    if (vm.executionForbidden()) [[unlikely]]
        clear();
    else {
        if (vm.disallowVMEntryCount) [[unlikely]] {
            VM::checkVMEntryPermission();
            return;
        }

        std::optional<VMEntryScope> entryScope;
        JSGlobalObject* currentGlobalObject = nullptr;

        while (true) {
            auto [nextGlobalObject, done] = drain<useCallOnEachMicrotask>(currentGlobalObject, vm, catchScope);
            if (done)
                break;

            globalObjectSwitchCallback(currentGlobalObject, nextGlobalObject);

            if (nextGlobalObject) {
                if (!entryScope)
                    entryScope.emplace(vm, nextGlobalObject);
                else
                    entryScope->setGlobalObject(nextGlobalObject);
            } else
                entryScope = std::nullopt;

            currentGlobalObject = nextGlobalObject;
        }

        vm.didEnterVM = true;
    }
    m_queue.swap(m_toKeep);
}


} // namespace JSC
