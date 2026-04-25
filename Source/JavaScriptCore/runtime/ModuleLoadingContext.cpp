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
#include "ModuleLoadingContext.h"

#include "CyclicModuleRecord.h"
#include "JSCInlines.h"
#include "ModuleLoaderPayload.h"
#include "ModuleRegistryEntry.h"
#include "ProgramExecutable.h"

namespace JSC {

const ClassInfo ModuleLoadingContext::s_info = { "ModuleLoadingContext"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(ModuleLoadingContext) };

ModuleLoadingContext::ModuleLoadingContext(VM& vm, Structure* structure, Step step, const JSModuleLoader::ModuleReferrer& referrer, AbstractModuleRecord::ModuleRequest&& moduleRequest, ModuleLoaderPayload* payload, ModuleRegistryEntry* entry, RefPtr<ScriptFetcher> scriptFetcher)
    : Base(vm, structure)
    , m_step(step)
    , m_moduleRequest(WTF::move(moduleRequest))
    , m_scriptFetcher(WTF::move(scriptFetcher))
    , m_payload(payload, WriteBarrierEarlyInit)
    , m_entry(entry, WriteBarrierEarlyInit)
    , m_referrer(referrer.toJSValue(), WriteBarrierEarlyInit)
{
}

void ModuleLoadingContext::destroy(JSCell* cell)
{
    SUPPRESS_MEMORY_UNSAFE_CAST auto* thisObject = static_cast<ModuleLoadingContext*>(cell);
    thisObject->~ModuleLoadingContext();
}

ModuleLoadingContext* ModuleLoadingContext::create(VM& vm, Step step, const JSModuleLoader::ModuleReferrer& referrer, const AbstractModuleRecord::ModuleRequest& moduleRequest, ModuleLoaderPayload* payload, ModuleRegistryEntry* entry, RefPtr<ScriptFetcher> scriptFetcher)
{
    AbstractModuleRecord::ModuleRequest requestCopy { moduleRequest };
    auto* context = new (NotNull, allocateCell<ModuleLoadingContext>(vm)) ModuleLoadingContext(vm, vm.moduleLoadingContextStructure.get(), step, referrer, WTF::move(requestCopy), payload, entry, WTF::move(scriptFetcher));
    context->finishCreation(vm);
    return context;
}

ModuleLoadingContext::ModuleLoadingContext(VM& vm, Structure* structure, AbstractModuleRecord::ModuleRequest&& moduleRequest, RefPtr<ScriptFetcher> scriptFetcher, bool evaluate, bool dynamic, bool useImportMap)
    : Base(vm, structure)
    , m_moduleRequest(WTF::move(moduleRequest))
    , m_scriptFetcher(WTF::move(scriptFetcher))
    , m_evaluate(evaluate)
    , m_dynamic(dynamic)
    , m_useImportMap(useImportMap)
{
}

ModuleLoadingContext* ModuleLoadingContext::create(VM& vm, const AbstractModuleRecord::ModuleRequest& moduleRequest, RefPtr<ScriptFetcher> scriptFetcher, bool evaluate, bool dynamic, bool useImportMap)
{
    AbstractModuleRecord::ModuleRequest requestCopy { moduleRequest };
    auto* context = new (NotNull, allocateCell<ModuleLoadingContext>(vm)) ModuleLoadingContext(vm, vm.moduleLoadingContextStructure.get(), WTF::move(requestCopy), WTF::move(scriptFetcher), evaluate, dynamic, useImportMap);
    context->finishCreation(vm);
    return context;
}

JSModuleLoader::ModuleReferrer ModuleLoadingContext::referrer() const
{
    JSValue ref = m_referrer.get();
    if (auto* module = dynamicDowncast<CyclicModuleRecord>(ref))
        return module;
    if (auto* exec = dynamicDowncast<ProgramExecutable>(ref))
        return exec;
    return uncheckedDowncast<JSGlobalObject>(ref);
}

template<typename Visitor>
void ModuleLoadingContext::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = uncheckedDowncast<ModuleLoadingContext>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_payload);
    visitor.append(thisObject->m_entry);
    visitor.append(thisObject->m_referrer);
    visitor.append(thisObject->m_module);
}

DEFINE_VISIT_CHILDREN(ModuleLoadingContext);

} // namespace JSC
