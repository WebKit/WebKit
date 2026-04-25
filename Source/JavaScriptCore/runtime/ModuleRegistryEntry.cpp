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
#include "ModuleRegistryEntry.h"

#include "JSCInlines.h"
#include "JSModuleLoader.h"
#include "JSModuleRecord.h"
#include "JSPromise.h"
#include "Microtask.h"

namespace JSC {

const ClassInfo ModuleRegistryEntry::s_info = { "ModuleRegistryEntry"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(ModuleRegistryEntry) };

ModuleRegistryEntry::ModuleRegistryEntry(VM& vm, Structure* structure, Identifier key, ScriptFetchParameters::Type type, RefPtr<ScriptFetcher> scriptFetcher)
    : Base(vm, structure)
    , m_key(WTF::move(key))
    , m_type(type)
    , m_scriptFetcher(WTF::move(scriptFetcher))
{
}

void ModuleRegistryEntry::destroy(JSCell* cell)
{
    SUPPRESS_MEMORY_UNSAFE_CAST auto* thisObject = static_cast<ModuleRegistryEntry*>(cell);
    thisObject->~ModuleRegistryEntry();
}

void ModuleRegistryEntry::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

template<typename Visitor>
void ModuleRegistryEntry::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = uncheckedDowncast<ModuleRegistryEntry>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_record);
    visitor.append(thisObject->m_fetchPromise);
    visitor.append(thisObject->m_modulePromise);
    visitor.append(thisObject->m_loadPromise);
    visitor.append(thisObject->m_fetchError);
    visitor.append(thisObject->m_instantiationError);
    visitor.append(thisObject->m_evaluationError);
}

DEFINE_VISIT_CHILDREN(ModuleRegistryEntry);

ModuleRegistryEntry* ModuleRegistryEntry::create(VM& vm, Structure* structure, Identifier key, ScriptFetchParameters::Type type, RefPtr<ScriptFetcher> scriptFetcher)
{
    ModuleRegistryEntry* instance = new (NotNull, allocateCell<ModuleRegistryEntry>(vm)) ModuleRegistryEntry(vm, structure, WTF::move(key), type, WTF::move(scriptFetcher));
    instance->finishCreation(vm);
    return instance;
}

ModuleRegistryEntry* ModuleRegistryEntry::create(VM& vm, Identifier key, ScriptFetchParameters::Type type, RefPtr<ScriptFetcher> scriptFetcher)
{
    return create(vm, vm.moduleRegistryEntryStructure.get(), WTF::move(key), type, WTF::move(scriptFetcher));
}

const Identifier& ModuleRegistryEntry::key() const
{
    return m_key;
}

ScriptFetchParameters::Type ModuleRegistryEntry::moduleType() const
{
    return m_type;
}

AbstractModuleRecord* ModuleRegistryEntry::record() const
{
    return m_record.get();
}

JSPromise* ModuleRegistryEntry::ensureFetchPromise(JSGlobalObject* globalObject)
{
    if (m_fetchPromise)
        return m_fetchPromise.get();

    VM& vm = globalObject->vm();

    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
    promise->markAsHandled();

    if (m_fetchError)
        promise->reject(vm, globalObject, m_fetchError.get());

    m_fetchPromise.set(vm, this, promise);
    return promise;
}

JSPromise* ModuleRegistryEntry::ensureModulePromise(JSGlobalObject* globalObject)
{
    if (m_modulePromise)
        return m_modulePromise.get();

    VM& vm = globalObject->vm();

    // Pre-create the module promise. It will be resolved/rejected by the
    // ModuleRegistryFetchSettled and ModuleRegistryModuleSettled microtask handlers.
    JSPromise* modulePromise = JSPromise::create(vm, globalObject->promiseStructure());
    modulePromise->markAsHandled();
    m_modulePromise.set(vm, this, modulePromise);

    JSPromise* fetchPromise = ensureFetchPromise(globalObject);
    fetchPromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleRegistryFetchSettled, modulePromise, this);

    return modulePromise;
}

JSPromise* ModuleRegistryEntry::loadPromise() const
{
    return m_loadPromise.get();
}

JSValue ModuleRegistryEntry::error(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (JSValue fetchError = m_fetchError.get()) {
        if (auto* errorInstance = dynamicDowncast<ErrorInstance>(fetchError))
            RELEASE_AND_RETURN(scope, JSModuleLoader::duplicateError(globalObject, errorInstance));
        RELEASE_AND_RETURN(scope, fetchError);
    }
    if (m_instantiationError)
        RELEASE_AND_RETURN(scope, m_instantiationError.get());
    if (m_evaluationError)
        RELEASE_AND_RETURN(scope, m_evaluationError.get());
    if (m_record) {
        if (auto* cyclic = dynamicDowncast<CyclicModuleRecord>(m_record.get()))
            RELEASE_AND_RETURN(scope, cyclic->evaluationError());
    }
    return { };
}

JSValue ModuleRegistryEntry::fetchError() const
{
    return m_fetchError.get();
}

JSValue ModuleRegistryEntry::instantiationError() const
{
    return m_instantiationError.get();
}

JSValue ModuleRegistryEntry::evaluationError() const
{
    return m_evaluationError.get();
}

auto ModuleRegistryEntry::status() const -> Status
{
    return m_status;
}

void ModuleRegistryEntry::setRecord(VM& vm, AbstractModuleRecord* record)
{
    m_record.set(vm, this, record);
}

void ModuleRegistryEntry::setLoadPromise(VM& vm, JSPromise* promise)
{
    m_loadPromise.set(vm, this, promise);
}

void ModuleRegistryEntry::setFetchError(JSGlobalObject* globalObject, JSValue error)
{
    ASSERT(error);
    VM& vm = globalObject->vm();
    m_fetchError.set(vm, this, error);
    if (m_status == Status::New && m_fetchPromise)
        m_fetchPromise->reject(vm, globalObject, error);
    setStatus(Status::FetchFailed);
}

void ModuleRegistryEntry::setInstantiationError(JSGlobalObject* globalObject, JSValue error)
{
    ASSERT(error);
    VM& vm = globalObject->vm();
    m_instantiationError.set(vm, this, error);
    if (m_status != Status::FetchFailed)
        setStatus(Status::InstantiationFailed);
}

void ModuleRegistryEntry::setEvaluationError(JSGlobalObject* globalObject, JSValue error)
{
    ASSERT(error);
    VM& vm = globalObject->vm();
    m_evaluationError.set(vm, this, error);
    if (m_status != Status::FetchFailed)
        setStatus(Status::EvaluationFailed);
}

void ModuleRegistryEntry::setStatus(Status status)
{
    m_status = status;
}

void ModuleRegistryEntry::provideFetch(JSGlobalObject* globalObject, SourceCode&& sourceCode)
{
    provideFetch(globalObject, JSSourceCode::create(globalObject->vm(), WTF::move(sourceCode)));
}

void ModuleRegistryEntry::provideFetch(JSGlobalObject* globalObject, JSSourceCode* jsSourceCode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(m_status == Status::New);

    ensureModulePromise(globalObject);
    RETURN_IF_EXCEPTION(scope, void());

    scope.release();
    m_status = Status::Fetching;
    m_fetchPromise->fulfill(vm, globalObject, jsSourceCode);
}

void ModuleRegistryEntry::fetchComplete(JSGlobalObject* globalObject, AbstractModuleRecord* record)
{
    if (m_status == Status::FetchFailed) {
        // This is possible if resolution fails during HostLoadImportedModule.
        return;
    }
    VM& vm = globalObject->vm();
    ASSERT(m_status == Status::Fetching);
    m_record.set(vm, this, record);
    m_status = Status::Fetched;
}

} // namespace JSC
