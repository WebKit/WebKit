/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyInstance.h"

#if ENABLE(WEBASSEMBLY)

#include "AbstractModuleRecord.h"
#include "JSCInlines.h"
#include "JSModuleNamespaceObject.h"
#include "JSWebAssemblyCompileError.h"
#include "JSWebAssemblyLinkError.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyModule.h"
#include "WasmTag.h"
#include "WebAssemblyModuleRecord.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

const ClassInfo JSWebAssemblyInstance::s_info = { "WebAssembly.Instance"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyInstance) };

Structure* JSWebAssemblyInstance::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSWebAssemblyInstance::JSWebAssemblyInstance(VM& vm, Structure* structure, Ref<Wasm::Instance>&& instance)
    : Base(vm, structure)
    , m_instance(WTFMove(instance))
    , m_vm(&vm)
    , m_globalObject(vm, this, structure->globalObject())
    , m_tables(m_instance->module().moduleInformation().tableCount())
{
    m_instance->setOwner(this);
    for (unsigned i = 0; i < this->instance().numImportFunctions(); ++i)
        new (this->instance().importFunction<WriteBarrier<JSObject>>(i)) WriteBarrier<JSObject>();
}

void JSWebAssemblyInstance::finishCreation(VM& vm, JSWebAssemblyModule* module, WebAssemblyModuleRecord* moduleRecord)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    m_module.set(vm, this, module);
    m_moduleRecord.set(vm, this, moduleRecord);

    vm.heap.reportExtraMemoryAllocated(m_instance->extraMemoryAllocated());
}

void JSWebAssemblyInstance::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyInstance*>(cell)->JSWebAssemblyInstance::~JSWebAssemblyInstance();
}

template<typename Visitor>
void JSWebAssemblyInstance::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSWebAssemblyInstance*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_module);
    visitor.append(thisObject->m_moduleRecord);
    visitor.append(thisObject->m_memory);
    for (auto& table : thisObject->m_tables)
        visitor.append(table);
    visitor.reportExtraMemoryVisited(thisObject->m_instance->extraMemoryAllocated());
    for (unsigned i = 0; i < thisObject->instance().numImportFunctions(); ++i)
        visitor.append(*thisObject->instance().importFunction<WriteBarrier<JSObject>>(i)); // This also keeps the functions' JSWebAssemblyInstance alive.

    for (size_t i : thisObject->instance().globalsToBinding()) {
        Wasm::Global* binding = thisObject->instance().getGlobalBinding(i);
        if (binding)
            visitor.appendUnbarriered(binding->owner<JSWebAssemblyGlobal>());
    }
    for (size_t i : thisObject->instance().globalsToMark())
        visitor.appendUnbarriered(JSValue::decode(thisObject->instance().loadI64Global(i)));

    Locker locker { cell->cellLock() };
    for (auto& wrapper : thisObject->instance().functionWrappers())
        visitor.appendUnbarriered(wrapper.get());
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyInstance);

void JSWebAssemblyInstance::initializeImports(JSGlobalObject* globalObject, JSObject* importObject, Wasm::CreationMode creationMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    m_moduleRecord->prepareLink(vm, this);
    if (creationMode == Wasm::CreationMode::FromJS) {
        m_moduleRecord->link(globalObject, jsNull());
        RETURN_IF_EXCEPTION(scope, void());
        m_moduleRecord->initializeImports(globalObject, importObject, creationMode);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

void JSWebAssemblyInstance::finalizeCreation(VM& vm, JSGlobalObject* globalObject, Ref<Wasm::CalleeGroup>&& wasmCalleeGroup, Wasm::CreationMode creationMode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!wasmCalleeGroup->runnable()) {
        throwException(globalObject, scope, createJSWebAssemblyLinkError(globalObject, vm, wasmCalleeGroup->errorMessage()));
        return;
    }

    RELEASE_ASSERT(wasmCalleeGroup->isSafeToRun(memoryMode()));

    // When memory is imported, we will initialize all memory modes with the initial LLInt compilation
    // results, so that later when memory imports become available, the appropriate CalleeGroup can be used.
    // If LLInt is disabled, we instead defer compilation to module evaluation.
    // If the code is already compiled, e.g. the module was already instantiated before, we do not re-initialize.
    if (Options::useWasmLLInt() && module()->moduleInformation().hasMemoryImport())
        module()->module().copyInitialCalleeGroupToAllMemoryModes(memoryMode());

    for (unsigned importFunctionNum = 0; importFunctionNum < instance().numImportFunctions(); ++importFunctionNum) {
        auto* info = instance().importFunctionInfo(importFunctionNum);
        info->wasmToEmbedderStub = m_module->wasmToEmbedderStub(importFunctionNum);
    }

    if (creationMode == Wasm::CreationMode::FromJS) {
        m_moduleRecord->initializeExports(globalObject);
        RETURN_IF_EXCEPTION(scope, void());

        JSValue startResult = m_moduleRecord->evaluate(globalObject);
        UNUSED_PARAM(startResult);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

Identifier JSWebAssemblyInstance::createPrivateModuleKey()
{
    return Identifier::fromUid(PrivateName(PrivateName::Description, "WebAssemblyInstance"_s));
}

JSWebAssemblyInstance* JSWebAssemblyInstance::tryCreate(VM& vm, JSGlobalObject* globalObject, const Identifier& moduleKey, JSWebAssemblyModule* jsModule, JSObject* importObject, Structure* instanceStructure, Ref<Wasm::Module>&& module, Wasm::CreationMode creationMode)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    const Wasm::ModuleInformation& moduleInformation = jsModule->moduleInformation();

    auto exception = [&] (JSObject* error) {
        throwException(globalObject, throwScope, error);
        return nullptr;
    };

    // Disabled by CSP: https://w3c.github.io/webappsec-csp/#can-compile-wasm-bytes
    if (!globalObject->webAssemblyEnabled())
        return exception(createJSWebAssemblyCompileError(globalObject, vm, globalObject->webAssemblyDisabledErrorMessage()));

    WebAssemblyModuleRecord* moduleRecord = WebAssemblyModuleRecord::create(globalObject, vm, globalObject->webAssemblyModuleRecordStructure(), moduleKey, moduleInformation);
    RETURN_IF_EXCEPTION(throwScope, nullptr);

    // FIXME: These objects could be pretty big we should try to throw OOM here.
    auto* jsInstance = new (NotNull, allocateCell<JSWebAssemblyInstance>(vm)) JSWebAssemblyInstance(vm, instanceStructure, Wasm::Instance::create(vm, WTFMove(module)));
    jsInstance->finishCreation(vm, jsModule, moduleRecord);
    RETURN_IF_EXCEPTION(throwScope, nullptr);

    if (creationMode == Wasm::CreationMode::FromJS) {
        // If the list of module.imports is not empty and Type(importObject) is not Object, a TypeError is thrown.
        if (moduleInformation.imports.size() && !importObject)
            return exception(createTypeError(globalObject, "can't make WebAssembly.Instance because there is no imports Object and the WebAssembly.Module requires imports"_s));
    }

    // For each import i in module.imports:
    {
        IdentifierSet specifiers;
        for (auto& import : moduleInformation.imports) {
            Identifier moduleName = Identifier::fromString(vm, String::fromUTF8(import.module));
            Identifier fieldName = Identifier::fromString(vm, String::fromUTF8(import.field));
            auto result = specifiers.add(moduleName.impl());
            if (result.isNewEntry)
                moduleRecord->appendRequestedModule(moduleName, nullptr);
            moduleRecord->addImportEntry(WebAssemblyModuleRecord::ImportEntry {
                WebAssemblyModuleRecord::ImportEntryType::Single,
                moduleName,
                fieldName,
                Identifier::fromUid(PrivateName(PrivateName::Description, "WebAssemblyImportName"_s)),
            });
        }
        ASSERT(moduleRecord->importEntries().size() == moduleInformation.imports.size());
    }

    bool hasMemoryImport = moduleInformation.memory.isImport();
    if (moduleInformation.memory && !hasMemoryImport) {
        // We create a memory when it's a memory definition.
        auto* jsMemory = JSWebAssemblyMemory::tryCreate(globalObject, vm, globalObject->webAssemblyMemoryStructure());
        RETURN_IF_EXCEPTION(throwScope, nullptr);

        RefPtr<Wasm::Memory> memory = Wasm::Memory::tryCreate(vm, moduleInformation.memory.initial(), moduleInformation.memory.maximum(), moduleInformation.memory.isShared() ? Wasm::MemorySharingMode::Shared: Wasm::MemorySharingMode::Default,
            [&vm, jsMemory](Wasm::Memory::GrowSuccess, Wasm::PageCount oldPageCount, Wasm::PageCount newPageCount) { jsMemory->growSuccessCallback(vm, oldPageCount, newPageCount); }
        );
        if (!memory)
            return exception(createOutOfMemoryError(globalObject));

        jsMemory->adopt(memory.releaseNonNull());
        jsInstance->setMemory(vm, jsMemory);
        RETURN_IF_EXCEPTION(throwScope, nullptr);
    }

    if (!jsInstance->memory()) {
        // Make sure we have a dummy memory, so that wasm -> wasm thunks avoid checking for a nullptr Memory when trying to set pinned registers.
        // When there is a memory import, this will be replaced later in the module record import initialization.
        auto* jsMemory = JSWebAssemblyMemory::tryCreate(globalObject, vm, globalObject->webAssemblyMemoryStructure());
        RETURN_IF_EXCEPTION(throwScope, nullptr);
        jsMemory->adopt(Wasm::Memory::create());
        jsInstance->setMemory(vm, jsMemory);
        RETURN_IF_EXCEPTION(throwScope, nullptr);
    }
    
    return jsInstance;
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
