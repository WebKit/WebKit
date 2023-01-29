/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "CompilationResult.h"
#include "WasmB3IRGenerator.h"
#include "WasmJS.h"
#include "WasmModuleInformation.h"
#include <wtf/Bag.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/SharedTask.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace JSC {

class CallLinkInfo;
class VM;

namespace Wasm {

class CalleeGroup;

class Plan : public ThreadSafeRefCounted<Plan> {
public:
    typedef void CallbackType(Plan&);
    using CompletionTask = RefPtr<SharedTask<CallbackType>>;

    static CompletionTask dontFinalize() { return createSharedTask<CallbackType>([](Plan&) { }); }
    Plan(VM&, Ref<ModuleInformation>, CompletionTask&&);

    // Note: This constructor should only be used if you are not actually building a module e.g. validation/function tests
    JS_EXPORT_PRIVATE Plan(VM&, CompletionTask&&);
    virtual JS_EXPORT_PRIVATE ~Plan();

    // If you guarantee the ordering here, you can rely on FIFO of the
    // completion tasks being called.
    void addCompletionTask(VM&, CompletionTask&&);

    void setMode(MemoryMode mode) { m_mode = mode; }
    MemoryMode mode() const { return m_mode; }

    String errorMessage() const { return crossThreadCopy(m_errorMessage); }

    bool WARN_UNUSED_RETURN failed() const { return !m_errorMessage.isNull(); }
    virtual bool hasWork() const = 0;
    enum CompilationEffort { All, Partial };
    virtual void work(CompilationEffort = All) = 0;
    virtual bool multiThreaded() const = 0;

    void waitForCompletion();
    // Returns true if it cancelled the plan.
    bool tryRemoveContextAndCancelIfLast(VM&);

protected:
    void runCompletionTasks() WTF_REQUIRES_LOCK(m_lock);
    void fail(String&& errorMessage) WTF_REQUIRES_LOCK(m_lock);

    virtual bool isComplete() const = 0;
    virtual void complete() WTF_REQUIRES_LOCK(m_lock) = 0;

    Ref<ModuleInformation> m_moduleInformation;

    Vector<std::pair<VM*, CompletionTask>, 1> m_completionTasks;

    String m_errorMessage;
    MemoryMode m_mode { MemoryMode::BoundsChecking };
    Lock m_lock;
    Condition m_completed;
};


} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
