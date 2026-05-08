/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 */

#include "config.h"
#include "Scheduler.h"

#include "AbortSignal.h"
#include "CallbackResult.h"
#include "DOMException.h"
#include "EventLoop.h"
#include "JSDOMConvertAny.h"
#include "JSDOMPromiseDeferred.h"
#include "JSSchedulerPostTaskCallback.h"
#include "JSSchedulerPostTaskOptions.h"
#include "JSValueInWrappedObjectInlines.h"
#include "SchedulerPostTaskCallback.h"
#include "SchedulerPostTaskOptions.h"
#include "ScriptExecutionContext.h"
#include "TaskSignal.h"
#include "TaskSource.h"
#include <JavaScriptCore/SlotVisitorMacros.h>
#include <JavaScriptCore/StrongInlines.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

Ref<Scheduler> Scheduler::create()
{
    return adoptRef(*new Scheduler());
}

Scheduler::Scheduler() = default;
Scheduler::~Scheduler() = default;

static TaskSource taskSourceForPriority(std::optional<TaskPriority> priority)
{
    if (!priority)
        return TaskSource::PostedMessageQueue;
    switch (*priority) {
    case TaskPriority::UserBlocking:
        return TaskSource::UserInteraction;
    case TaskPriority::UserVisible:
        return TaskSource::PostedMessageQueue;
    case TaskPriority::Background:
        return TaskSource::PostedMessageQueue;
    }
    return TaskSource::PostedMessageQueue;
}

void Scheduler::postTask(ScriptExecutionContext& context, Ref<SchedulerPostTaskCallback>&& callback, const SchedulerPostTaskOptions& options, Ref<DeferredPromise>&& promise)
{
    CheckedRef protectedContext { context };

    if (RefPtr signal = options.signal; signal && signal->aborted()) {
        promise->reject<IDLAny>(signal->reason().getValue());
        return;
    }

    m_pendingCallbacks.add(callback.copyRef());

    struct AbortedFlag : public ThreadSafeRefCounted<AbortedFlag> {
        bool value { false };
    };
    Ref aborted = adoptRef(*new AbortedFlag);

    std::optional<JSC::Strong<JSC::JSObject>> strongCallback;
    if (auto* jsCallback = dynamicDowncast<JSSchedulerPostTaskCallback>(callback.ptr())) {
        if (auto* callbackData = jsCallback->callbackData()) {
            if (auto* globalObject = callbackData->globalObject()) {
                if (auto* function = callbackData->callback())
                    strongCallback.emplace(globalObject->vm(), function);
            }
        }
    }

    uint32_t abortAlgorithmIdentifier { 0 };
    if (RefPtr signal = options.signal) {
        abortAlgorithmIdentifier = signal->addAlgorithm([aborted = aborted.copyRef(), promise = promise.copyRef(), protectedThis = Ref { *this }, callback = callback.copyRef()](JSC::JSValue reason) mutable {
            if (aborted->value)
                return;
            aborted->value = true;
            protectedThis->m_pendingCallbacks.remove(callback);
            promise->reject<IDLAny>(reason);
        });
    }

    auto runTask = [protectedThis = Ref { *this }, callback = callback.copyRef(), promise = promise.copyRef(), aborted = aborted.copyRef(), signal = RefPtr { options.signal.get() }, abortAlgorithmIdentifier, strongCallback = WTF::move(strongCallback)]() mutable {
        protectedThis->m_pendingCallbacks.remove(callback);
        if (aborted->value)
            return;

        auto result = callback->invokeRethrowingException();

        if (signal && abortAlgorithmIdentifier)
            signal->removeAlgorithm(abortAlgorithmIdentifier);

        if (aborted->value)
            return;

        if (result.type() == CallbackResultType::Success) {
            promise->resolveWithJSValue(result.releaseReturnValue());
            return;
        }
        if (result.type() == CallbackResultType::ExceptionThrown) {
            promise->reject(Exception { ExceptionCode::ExistingExceptionError });
            return;
        }
    };

    std::optional<TaskPriority> effectivePriority = options.priority;
    if (!effectivePriority) {
        if (RefPtr taskSignal = dynamicDowncast<TaskSignal>(options.signal.get()))
            effectivePriority = taskSignal->priority();
    }
    auto source = taskSourceForPriority(effectivePriority);
    CheckedRef protectedEventLoop { protectedContext->eventLoop() };
    if (!options.delay) {
        protectedEventLoop->queueTask(source, WTF::move(runTask));
        return;
    }

    struct DelayedRunner : RefCounted<DelayedRunner> {
        DelayedRunner(ScriptExecutionContext& context, TaskSource source, uint64_t delay, Function<void()>&& task)
            : context(&context)
            , source(source)
            , delay(delay)
            , startTime(MonotonicTime::now())
            , task(WTF::move(task))
        {
        }

        CheckedPtr<ScriptExecutionContext> context;
        TaskSource source;
        uint64_t delay;
        MonotonicTime startTime;
        Function<void()> task;

        void run()
        {
            if (!context)
                return;
            CheckedRef protectedContext = *context;
            CheckedRef protectedEventLoop { protectedContext->eventLoop() };
            if (MonotonicTime::now() - startTime < Seconds::fromMilliseconds(delay)) {
                protectedEventLoop->queueTask(source, [protectedThis = Ref { *this }]() mutable {
                    protectedThis->run();
                });
                return;
            }
            task();
        }
    };

    Ref delayedRunner = adoptRef(*new DelayedRunner(protectedContext, source, options.delay, WTF::move(runTask)));
    protectedEventLoop->queueTask(source, [delayedRunner = WTF::move(delayedRunner)]() mutable {
        delayedRunner->run();
    });
}

void Scheduler::yield(ScriptExecutionContext& context, Ref<DeferredPromise>&& promise)
{
    CheckedRef protectedContext { context };
    CheckedRef protectedEventLoop { protectedContext->eventLoop() };
    protectedEventLoop->queueTask(TaskSource::PostedMessageQueue, [promise = WTF::move(promise)]() mutable {
        promise->resolve();
    });
}

template<typename Visitor>
void Scheduler::visitAdditionalChildren(Visitor& visitor)
{
    for (auto& callback : m_pendingCallbacks)
        callback->visitJSFunctionInGCThread(visitor);
}

template void Scheduler::visitAdditionalChildren(JSC::AbstractSlotVisitor&);
template void Scheduler::visitAdditionalChildren(JSC::SlotVisitor&);

} // namespace WebCore
