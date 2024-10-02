/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include "WasmPlan.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmCallee.h"
#include "WasmCalleeGroup.h"
#include "WasmMachineThreads.h"
#include <wtf/DataLog.h>
#include <wtf/Locker.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringPrintStream.h>
#include <wtf/SystemTracing.h>

namespace JSC { namespace Wasm {

namespace WasmPlanInternal {
static constexpr bool verbose = false;
}

Plan::Plan(VM& vm, Ref<ModuleInformation> info, CompletionTask&& task)
    : m_moduleInformation(WTFMove(info))
{
    m_completionTasks.append(std::make_pair(&vm, WTFMove(task)));
}
Plan::Plan(VM& vm, CompletionTask&& task)
    : m_moduleInformation(ModuleInformation::create())
{
    m_completionTasks.append(std::make_pair(&vm, WTFMove(task)));
}

void Plan::runCompletionTasks()
{
    ASSERT(isComplete() && !hasWork());

    for (auto& task : m_completionTasks)
        task.second->run(*this);
    m_completionTasks.clear();
    m_completed.notifyAll();
}

bool Plan::addCompletionTaskIfNecessary(VM& vm, CompletionTask&& task)
{
    Locker locker { m_lock };
    if (!isComplete()) {
        m_completionTasks.append(std::make_pair(&vm, WTFMove(task)));
        return true;
    }
    return false;
}

void Plan::waitForCompletion()
{
    Locker locker { m_lock };
    if (!isComplete()) {
        m_completed.wait(m_lock);
    }
}

bool Plan::tryRemoveContextAndCancelIfLast(VM& vm)
{
    Locker locker { m_lock };

    if (ASSERT_ENABLED) {
        // We allow the first completion task to not have a VM.
        for (unsigned i = 1; i < m_completionTasks.size(); ++i)
            ASSERT(m_completionTasks[i].first);
    }

    bool removedAnyTasks = false;
    m_completionTasks.removeAllMatching([&] (const std::pair<VM*, CompletionTask>& pair) {
        bool shouldRemove = pair.first == &vm;
        removedAnyTasks |= shouldRemove;
        return shouldRemove;
    });

    if (!removedAnyTasks)
        return false;

    if (isComplete()) {
        // We trivially cancel anything that's completed.
        return true;
    }

    // FIXME: Make 0 index not so magical: https://bugs.webkit.org/show_bug.cgi?id=171395
    if (m_completionTasks.isEmpty() || (m_completionTasks.size() == 1 && !m_completionTasks[0].first)) {
        fail("WebAssembly Plan was cancelled. If you see this error message please file a bug at bugs.webkit.org!"_s);
        return true;
    }

    return false;
}

void Plan::fail(String&& errorMessage, Error error)
{
    if (failed())
        return;
    ASSERT(errorMessage);
    dataLogLnIf(WasmPlanInternal::verbose, "failing with message: ", errorMessage);
    m_errorMessage = WTFMove(errorMessage);
    m_error = error;
    complete();
}

Plan::~Plan() = default;

CString Plan::signpostMessage(CompilationMode compilationMode, uint32_t functionIndexSpace) const
{
    CString signpostMessage;
    const FunctionData& function = m_moduleInformation->functions[functionIndexSpace - m_moduleInformation->importFunctionTypeIndices.size()];
    StringPrintStream stream;
    stream.print(compilationMode, " ", makeString(IndexOrName(functionIndexSpace, m_moduleInformation->nameSection->get(functionIndexSpace))), " instructions size = ", function.data.size());
    return stream.toCString();
}

void Plan::beginCompilerSignpost(CompilationMode compilationMode, uint32_t functionIndexSpace) const
{
    if (UNLIKELY(Options::useCompilerSignpost())) {
        auto message = signpostMessage(compilationMode, functionIndexSpace);
        WTFBeginSignpost(this, JSCJITCompiler, "%" PUBLIC_LOG_STRING, message.data() ? message.data() : "(nullptr)");
    }
}

void Plan::beginCompilerSignpost(const Callee& callee) const
{
    beginCompilerSignpost(callee.compilationMode(), callee.index());
}

void Plan::endCompilerSignpost(CompilationMode compilationMode, uint32_t functionIndexSpace) const
{
    if (UNLIKELY(Options::useCompilerSignpost())) {
        auto message = signpostMessage(compilationMode, functionIndexSpace);
        WTFEndSignpost(this, JSCJITCompiler, "%" PUBLIC_LOG_STRING, message.data() ? message.data() : "(nullptr)");
    }
}

void Plan::endCompilerSignpost(const Callee& callee) const
{
    endCompilerSignpost(callee.compilationMode(), callee.index());
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
