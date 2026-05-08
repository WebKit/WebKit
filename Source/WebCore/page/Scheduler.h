/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 */

#pragma once

#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace JSC {
class AbstractSlotVisitor;
class SlotVisitor;
}

namespace WebCore {

class DeferredPromise;
class SchedulerPostTaskCallback;
struct SchedulerPostTaskOptions;
class ScriptExecutionContext;

class Scheduler final : public RefCounted<Scheduler> {
public:
    static Ref<Scheduler> create();
    ~Scheduler();

    void postTask(ScriptExecutionContext&, Ref<SchedulerPostTaskCallback>&&, const SchedulerPostTaskOptions&, Ref<DeferredPromise>&&);
    void yield(ScriptExecutionContext&, Ref<DeferredPromise>&&);

    template<typename Visitor> void visitAdditionalChildren(Visitor&);

private:
    Scheduler();

    HashSet<Ref<SchedulerPostTaskCallback>> m_pendingCallbacks;
};

} // namespace WebCore
